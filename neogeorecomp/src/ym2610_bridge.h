#ifndef YM2610_BRIDGE_H
#define YM2610_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void ym2610_bridge_init(const uint8_t *vrom_data, size_t vrom_size);
void ym2610_bridge_reset(void);
void ym2610_bridge_write(uint8_t port, uint8_t data);
uint8_t ym2610_bridge_read(uint8_t port);
void ym2610_bridge_clock(int64_t current_z80_clock);
int64_t ym2610_bridge_next_timer(void);
int  ym2610_bridge_irq_pending(void);
size_t ym2610_bridge_generate(int16_t *out, size_t frames);
void ym2610_bridge_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* YM2610_BRIDGE_H */
