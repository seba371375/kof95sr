#include "ym2610_bridge.h"
#include "ymfm_opn.h"
#include <memory>
#include <vector>
#include <algorithm>
#include <climits>

namespace {

class NeoGeoYmInterface : public ymfm::ymfm_interface {
public:
    NeoGeoYmInterface()
        : m_vrom(nullptr), m_vrom_size(0), m_irq_asserted(false) {
        m_timer_deadline[0] = INT64_MAX;
        m_timer_deadline[1] = INT64_MAX;
    }

    void set_vrom(const uint8_t *vrom, size_t size) {
        m_vrom = vrom;
        m_vrom_size = size;
    }

    void reset_timers() {
        m_timer_deadline[0] = INT64_MAX;
        m_timer_deadline[1] = INT64_MAX;
        m_irq_asserted = false;
    }

    virtual void ymfm_update_irq(bool asserted) override {
        m_irq_asserted = asserted;
    }

    virtual void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override {
        if (tnum > 1) return;
        if (duration_in_clocks < 0) {
            m_timer_deadline[tnum] = INT64_MAX;
        } else {
            // YM2610 runs at 8 MHz, Z80 runs at 4 MHz (1:2 ratio)
            int64_t z80_clocks = ((int64_t)duration_in_clocks + 1) / 2;
            if (m_timer_deadline[tnum] != INT64_MAX && m_timer_deadline[tnum] <= m_current_z80_clock) {
                m_timer_deadline[tnum] += z80_clocks;
            } else {
                m_timer_deadline[tnum] = m_current_z80_clock + z80_clocks;
            }
        }
    }

    virtual uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override {
        if ((type == ymfm::ACCESS_ADPCM_A || type == ymfm::ACCESS_ADPCM_B) && m_vrom) {
            if (address < m_vrom_size) {
                return m_vrom[address];
            }
        }
        return 0;
    }

    void update_timers(int64_t z80_clock) {
        m_current_z80_clock = z80_clock;
        for (int t = 0; t < 2; t++) {
            while (m_timer_deadline[t] != INT64_MAX && z80_clock >= m_timer_deadline[t]) {
                if (m_engine) {
                    m_engine->engine_timer_expired(t);
                }
            }
        }
    }

    bool is_irq_asserted() const {
        return m_irq_asserted;
    }

private:
    const uint8_t *m_vrom;
    size_t m_vrom_size;
    int64_t m_current_z80_clock = 0;
    int64_t m_timer_deadline[2];
    bool m_irq_asserted = false;
};

static std::unique_ptr<NeoGeoYmInterface> s_interface;
static std::unique_ptr<ymfm::ym2610> s_ym2610;
static uint32_t s_resample_accum = 0;
static ymfm::ym2610::output_data s_last_ym_out;

} // namespace

extern "C" {

void ym2610_bridge_init(const uint8_t *vrom_data, size_t vrom_size) {
    s_interface = std::make_unique<NeoGeoYmInterface>();
    s_interface->set_vrom(vrom_data, vrom_size);
    s_ym2610 = std::make_unique<ymfm::ym2610>(*s_interface);
    s_ym2610->set_fidelity(ymfm::OPN_FIDELITY_MIN);
    s_ym2610->reset();
    s_resample_accum = 0;
    s_last_ym_out.clear();
}

void ym2610_bridge_reset(void) {
    if (s_interface) s_interface->reset_timers();
    if (s_ym2610) {
        s_ym2610->set_fidelity(ymfm::OPN_FIDELITY_MIN);
        s_ym2610->reset();
    }
    s_resample_accum = 0;
    s_last_ym_out.clear();
}

void ym2610_bridge_write(uint8_t port, uint8_t data) {
    if (!s_ym2610) return;
    switch (port & 3) {
    case 0: s_ym2610->write_address(data); break;
    case 1: s_ym2610->write_data(data); break;
    case 2: s_ym2610->write_address_hi(data); break;
    case 3: s_ym2610->write_data_hi(data); break;
    }
}

uint8_t ym2610_bridge_read(uint8_t port) {
    if (!s_ym2610) return 0;
    switch (port & 3) {
    case 0: return s_ym2610->read_status();
    case 1: return s_ym2610->read_data();
    case 2: return s_ym2610->read_status_hi();
    case 3: return s_ym2610->read_data_hi();
    default: return 0;
    }
}

void ym2610_bridge_clock(int64_t current_z80_clock) {
    if (s_interface) {
        s_interface->update_timers(current_z80_clock);
    }
}

int ym2610_bridge_irq_pending(void) {
    return s_interface && s_interface->is_irq_asserted() ? 1 : 0;
}

size_t ym2610_bridge_generate(int16_t *out, size_t frames) {
    if (!s_ym2610) {
        std::fill_n(out, frames * 2, 0);
        return frames;
    }

    // YM2610 native rate = 8,000,000 / 144 = 55555.5555... Hz.
    // Target rate = 48,000 Hz.
    // Fixed point ratio: 55556 / 48000.
    const uint32_t step = 55556;
    const uint32_t base = 48000;

    for (size_t i = 0; i < frames; i++) {
        s_resample_accum += step;
        while (s_resample_accum >= base) {
            s_ym2610->generate(&s_last_ym_out);
            s_resample_accum -= base;
        }

        // Channels: data[0] = FM Left + ADPCM, data[1] = FM Right + ADPCM, data[2] = SSG mono
        int32_t left = ((s_last_ym_out.data[0] + s_last_ym_out.data[2]) * 3) / 2;
        int32_t right = ((s_last_ym_out.data[1] + s_last_ym_out.data[2]) * 3) / 2;

        out[i * 2 + 0] = (int16_t)std::max<int32_t>(-32768, std::min<int32_t>(32767, left));
        out[i * 2 + 1] = (int16_t)std::max<int32_t>(-32768, std::min<int32_t>(32767, right));
    }

    return frames;
}

void ym2610_bridge_shutdown(void) {
    s_ym2610.reset();
    s_interface.reset();
}

} // extern "C"
