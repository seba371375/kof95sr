/*
 * window_sdl.c — Window management, input (gamepads & keyboard), and rendering
 * backends (SDL Accelerated, Native OpenGL, Vulkan).
 *
 * Supports:
 *   - PS5 DualSense, PS4 DualShock, Xbox, and USB gamepads/fightsticks (P1 & P2).
 *   - Hotplugging & arcade macro shortcuts (L1: A+B Roll, R1: C+D Blow-away, L2: Charge).
 *   - Keyboard (P1: Arrows+Z/X/C/V, P2: WASD/IJKL, 1/2: Start, 5/6: Coin).
 *   - Renderers: SDL2 accelerated, OpenGL, and Vulkan (via NG_RENDERER or runtime).
 *   - Fullscreen toggle (Alt+Enter / F11).
 */
#include "ng_internal.h"
#include "osd_menu.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __vita__
#include <psp2/io/fcntl.h>
#elif !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#define strcasecmp _stricmp
#endif

#ifdef __vita__
static int s_vsync = 1; // Default 1 = VSync ON
#else
static int s_vsync = 1; // 1 = VSync ON (Smooth 60Hz), 0 = VSync OFF (Ultra Low Latency)
#endif
static int s_perf_log = 0; // 0 = disabled by default, 1 = log telemetry for debugging

#ifdef NG_HAVE_OPENGL
#define GL_SILENCE_DEPRECATION
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <SDL_opengl.h>
#endif

#ifndef APIENTRY
#ifdef _WIN32
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif
#ifndef GLchar
typedef char GLchar;
#endif

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER   0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS  0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS     0x8B82
#endif

typedef GLuint (APIENTRY *PFN_glCreateShader)(GLenum type);
typedef void   (APIENTRY *PFN_glShaderSource)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void   (APIENTRY *PFN_glCompileShader)(GLuint shader);
typedef void   (APIENTRY *PFN_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
typedef GLuint (APIENTRY *PFN_glCreateProgram)(void);
typedef void   (APIENTRY *PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void   (APIENTRY *PFN_glLinkProgram)(GLuint program);
typedef void   (APIENTRY *PFN_glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
typedef void   (APIENTRY *PFN_glUseProgram)(GLuint program);

static PFN_glCreateShader  glCreateShader_p = NULL;
static PFN_glShaderSource  glShaderSource_p = NULL;
static PFN_glCompileShader glCompileShader_p = NULL;
static PFN_glGetShaderiv   glGetShaderiv_p = NULL;
static PFN_glCreateProgram glCreateProgram_p = NULL;
static PFN_glAttachShader  glAttachShader_p = NULL;
static PFN_glLinkProgram   glLinkProgram_p = NULL;
static PFN_glGetProgramiv  glGetProgramiv_p = NULL;
static PFN_glUseProgram    glUseProgram_p = NULL;

static SDL_GLContext s_gl_ctx = NULL;
static GLuint        s_gl_tex = 0;
static GLuint        s_gl_prog_scan_50 = 0;
static GLuint        s_gl_prog_scan_75 = 0;
static GLuint        s_gl_prog_trinitron = 0;
static GLuint        s_gl_prog_scale2x = 0;
static GLuint        s_gl_prog_crt_warm = 0;

static GLuint compile_shader_prog(const char *fs_src) {
    if (!glCreateShader_p || !glShaderSource_p || !glCompileShader_p || !glCreateProgram_p) return 0;
    static const char *vs_src =
        "#version 120\n"
        "varying vec2 v_texcoord;\n"
        "void main() {\n"
        "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "    v_texcoord = gl_MultiTexCoord0.xy;\n"
        "}\n";

    GLuint vs = glCreateShader_p(GL_VERTEX_SHADER);
    glShaderSource_p(vs, 1, &vs_src, NULL);
    glCompileShader_p(vs);

    GLuint fs = glCreateShader_p(GL_FRAGMENT_SHADER);
    glShaderSource_p(fs, 1, &fs_src, NULL);
    glCompileShader_p(fs);

    GLint ok = 0;
    glGetShaderiv_p(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) return 0;

    GLuint prog = glCreateProgram_p();
    glAttachShader_p(prog, vs);
    glAttachShader_p(prog, fs);
    glLinkProgram_p(prog);
    glGetProgramiv_p(prog, GL_LINK_STATUS, &ok);
    return ok ? prog : 0;
}
#endif

#ifdef NG_HAVE_VULKAN
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#endif

typedef enum {
    RENDERER_SDL = 0,
    RENDERER_OPENGL,
    RENDERER_VULKAN
} renderer_kind_t;

static renderer_kind_t s_backend = RENDERER_SDL;
static SDL_Window      *s_win = NULL;
static SDL_Renderer    *s_ren = NULL;
static SDL_Texture     *s_tex = NULL;
static uint64_t         s_next_tick = 0;
static bool             s_is_fullscreen = false;
static SDL_AudioDeviceID s_audio_dev = 0;

/* GameController state (Player 1 & Player 2) */
static SDL_GameController *s_pads[2] = {NULL, NULL};
static SDL_JoystickID      s_pad_ids[2] = {-1, -1};

/*
 * Controller layout presets:
 * 0 = SNK Arcade: Cross=A (LP), Circle=B (LK), Square=C (HP), Triangle=D (HK)
 * 1 = Fighting Modern: Square=A (LP), Triangle=C (HP), Cross=B (LK), Circle=D (HK)
 * 2 = KOF Line: Square=A (LP), Cross=B (LK), Triangle=C (HP), R1=D (HK)
 */
static int s_pad_layout = 0;

static const char *s_layout_names[3] = {
    "SNK Default (X=A,O=B,Sq=C,Tr=D)",
    "Fighting Modern (Sq=A,Tr=C,X=B,O=D)",
    "KOF Line (Sq=A,X=B,Tr=C,R1=D)"
};

/*
 * Video Filter presets:
 * 0 = Raw Pixel (Nearest)
 * 1 = Bilinear (Smooth)
 * 2 = CRT Scanlines (50%)
 * 3 = CRT Scanlines (75%)
 * 4 = CRT Trinitron (RGB)
 * 5 = Scale2x (Pixel HD)
 * 6 = CRT Warm (Curved)
 */
typedef enum {
    FILTER_NEAREST = 0,
    FILTER_BILINEAR,
    FILTER_CRT_SCANLINES_50,
    FILTER_CRT_SCANLINES_75,
    FILTER_CRT_TRINITRON,
    FILTER_SCALE2X,
    FILTER_CRT_WARM,
    NUM_VIDEO_FILTERS
} video_filter_t;

static int s_video_filter = FILTER_NEAREST;

static const char *s_filter_names[NUM_VIDEO_FILTERS] = {
    "Raw Pixel (Nearest)",
    "Bilinear (Smooth)",
    "CRT Scanlines (50%)",
    "CRT Scanlines (75%)",
    "CRT Trinitron (RGB)",
    "Scale2x (Pixel HD)",
    "CRT Warm (Curved)"
};

/* Keyboard keybindings table */
static SDL_Scancode s_key_mappings[NUM_KEY_ACTIONS] = {
    SDL_SCANCODE_UP,     /* P1 UP */
    SDL_SCANCODE_DOWN,   /* P1 DOWN */
    SDL_SCANCODE_LEFT,   /* P1 LEFT */
    SDL_SCANCODE_RIGHT,  /* P1 RIGHT */
    SDL_SCANCODE_Z,      /* P1 A */
    SDL_SCANCODE_X,      /* P1 B */
    SDL_SCANCODE_C,      /* P1 C */
    SDL_SCANCODE_V,      /* P1 D */
    SDL_SCANCODE_1,      /* START 1 */
    SDL_SCANCODE_5,      /* COIN 1 */
    SDL_SCANCODE_W,      /* P2 UP */
    SDL_SCANCODE_S,      /* P2 DOWN */
    SDL_SCANCODE_A,      /* P2 LEFT */
    SDL_SCANCODE_D,      /* P2 RIGHT */
    SDL_SCANCODE_U,      /* P2 A */
    SDL_SCANCODE_I,      /* P2 B */
    SDL_SCANCODE_O,      /* P2 C */
    SDL_SCANCODE_P,      /* P2 D */
    SDL_SCANCODE_2,      /* START 2 */
    SDL_SCANCODE_6       /* COIN 2 */
};

static const char *s_key_action_names[NUM_KEY_ACTIONS] = {
    "P1 Up",
    "P1 Down",
    "P1 Left",
    "P1 Right",
    "P1 Button A",
    "P1 Button B",
    "P1 Button C",
    "P1 Button D",
    "P1 Start",
    "P1 Coin",
    "P2 Up",
    "P2 Down",
    "P2 Left",
    "P2 Right",
    "P2 Button A",
    "P2 Button B",
    "P2 Button C",
    "P2 Button D",
    "P2 Start",
    "P2 Coin"
};

static const char *s_key_cfg_tags[NUM_KEY_ACTIONS] = {
    "key_p1_up", "key_p1_down", "key_p1_left", "key_p1_right",
    "key_p1_a", "key_p1_b", "key_p1_c", "key_p1_d",
    "key_p1_start", "key_p1_coin",
    "key_p2_up", "key_p2_down", "key_p2_left", "key_p2_right",
    "key_p2_a", "key_p2_b", "key_p2_c", "key_p2_d",
    "key_p2_start", "key_p2_coin"
};

SDL_Scancode window_get_key_mapping(key_action_t action) {
    if (action < 0 || action >= NUM_KEY_ACTIONS) return SDL_SCANCODE_UNKNOWN;
    return s_key_mappings[action];
}

void window_set_key_mapping(key_action_t action, SDL_Scancode code) {
    if (action < 0 || action >= NUM_KEY_ACTIONS) return;
    s_key_mappings[action] = code;
    window_save_controls_cfg();
}

const char *window_get_key_action_name(key_action_t action) {
    if (action < 0 || action >= NUM_KEY_ACTIONS) return "Unknown";
    return s_key_action_names[action];
}

const char *window_get_key_name(SDL_Scancode code) {
    const char *name = SDL_GetScancodeName(code);
    if (!name || !name[0]) return "None";
    return name;
}

void window_reset_default_keys(void) {
    s_key_mappings[KEY_ACT_P1_UP]    = SDL_SCANCODE_UP;
    s_key_mappings[KEY_ACT_P1_DOWN]  = SDL_SCANCODE_DOWN;
    s_key_mappings[KEY_ACT_P1_LEFT]  = SDL_SCANCODE_LEFT;
    s_key_mappings[KEY_ACT_P1_RIGHT] = SDL_SCANCODE_RIGHT;
    s_key_mappings[KEY_ACT_P1_A]     = SDL_SCANCODE_Z;
    s_key_mappings[KEY_ACT_P1_B]     = SDL_SCANCODE_X;
    s_key_mappings[KEY_ACT_P1_C]     = SDL_SCANCODE_C;
    s_key_mappings[KEY_ACT_P1_D]     = SDL_SCANCODE_V;
    s_key_mappings[KEY_ACT_START1]   = SDL_SCANCODE_1;
    s_key_mappings[KEY_ACT_COIN1]    = SDL_SCANCODE_5;

    s_key_mappings[KEY_ACT_P2_UP]    = SDL_SCANCODE_W;
    s_key_mappings[KEY_ACT_P2_DOWN]  = SDL_SCANCODE_S;
    s_key_mappings[KEY_ACT_P2_LEFT]  = SDL_SCANCODE_A;
    s_key_mappings[KEY_ACT_P2_RIGHT] = SDL_SCANCODE_D;
    s_key_mappings[KEY_ACT_P2_A]     = SDL_SCANCODE_U;
    s_key_mappings[KEY_ACT_P2_B]     = SDL_SCANCODE_I;
    s_key_mappings[KEY_ACT_P2_C]     = SDL_SCANCODE_O;
    s_key_mappings[KEY_ACT_P2_D]     = SDL_SCANCODE_P;
    s_key_mappings[KEY_ACT_START2]   = SDL_SCANCODE_2;
    s_key_mappings[KEY_ACT_COIN2]    = SDL_SCANCODE_6;
    window_save_controls_cfg();
}

int window_get_pad_layout(void) {
    return s_pad_layout;
}

const char *window_get_pad_layout_name(int layout) {
    if (layout < 0 || layout > 2) return "Unknown";
    return s_layout_names[layout];
}

int window_get_video_filter(void) {
    return s_video_filter;
}

const char *window_get_video_filter_name(int filter) {
    if (filter < 0 || filter >= NUM_VIDEO_FILTERS) return "Unknown";
    return s_filter_names[filter];
}

void window_save_controls_cfg(void) {
    FILE *f = ng_open_config("kof95_controls.cfg", "w");
    if (!f) return;
    fprintf(f, "# KOF95 Controller, Video & Keyboard Configuration\n");
    fprintf(f, "# pad_layout:\n");
    fprintf(f, "#   0 = SNK Default   (Cross=A, Circle=B, Square=C, Triangle=D)\n");
    fprintf(f, "#   1 = Fighting Std  (Square=A, Triangle=C, Cross=B, Circle=D)\n");
    fprintf(f, "#   2 = KOF Classic   (Square=A, Cross=B, Triangle=C, R1=D)\n\n");
    fprintf(f, "# video_filter:\n");
    fprintf(f, "#   0 = Raw Pixel (Nearest)   1 = Bilinear (Smooth)\n");
    fprintf(f, "#   2 = CRT Scanlines (50%%)    3 = CRT Scanlines (75%%)\n");
    fprintf(f, "#   4 = CRT Trinitron (RGB)   5 = Scale2x (Pixel HD)\n");
    fprintf(f, "#   6 = CRT Warm (Curved)\n\n");
    fprintf(f, "pad_layout=%d\n", s_pad_layout);
    fprintf(f, "video_filter=%d\n", s_video_filter);
    fprintf(f, "vsync=%d\n", s_vsync);
    fprintf(f, "perf_log=%d\n\n", s_perf_log);

    fprintf(f, "# Keyboard Scancodes\n");
    for (int i = 0; i < NUM_KEY_ACTIONS; i++) {
        fprintf(f, "%s=%d\n", s_key_cfg_tags[i], (int)s_key_mappings[i]);
    }
    fclose(f);
}

int window_get_vsync(void) {
    return s_vsync;
}

int window_get_perf_log(void) {
    return s_perf_log;
}

void window_set_vsync(int vsync) {
    s_vsync = (vsync != 0) ? 1 : 0;
#ifdef NG_HAVE_OPENGL
    if (s_backend == RENDERER_OPENGL && s_gl_ctx) {
        SDL_GL_SetSwapInterval(s_vsync);
    }
#endif
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (s_ren) {
        SDL_RenderSetVSync(s_ren, s_vsync);
    }
#endif
    window_save_controls_cfg();
    fprintf(stderr, "[video] VSync set to: %s\n", s_vsync ? "ON (Smooth 60Hz)" : "OFF (Ultra Low Latency)");
}

void window_set_video_filter(int filter) {
    if (filter < 0) filter = NUM_VIDEO_FILTERS - 1;
    if (filter >= NUM_VIDEO_FILTERS) filter = 0;
    s_video_filter = filter;
#if SDL_VERSION_ATLEAST(2, 0, 12)
    if (s_tex) {
        SDL_SetTextureScaleMode(s_tex, (s_video_filter == FILTER_BILINEAR)
                                       ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    }
#endif
    window_save_controls_cfg();
    fprintf(stderr, "[video] Video filter changed to: %s\n", s_filter_names[s_video_filter]);
}

static void load_controls_cfg(void) {
    FILE *f = ng_open_config("kof95_controls.cfg", "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        int val = 0;
        if (sscanf(line, "pad_layout=%d", &val) == 1) {
            if (val >= 0 && val <= 2) s_pad_layout = val;
        } else if (sscanf(line, "video_filter=%d", &val) == 1) {
            if (val >= 0 && val < NUM_VIDEO_FILTERS) s_video_filter = val;
        } else if (sscanf(line, "vsync=%d", &val) == 1) {
            s_vsync = (val != 0) ? 1 : 0;
        } else if (sscanf(line, "perf_log=%d", &val) == 1) {
            s_perf_log = (val != 0) ? 1 : 0;
        } else {
            for (int i = 0; i < NUM_KEY_ACTIONS; i++) {
                char prefix[64];
                snprintf(prefix, sizeof(prefix), "%s=%%d", s_key_cfg_tags[i]);
                if (sscanf(line, prefix, &val) == 1) {
                    if (val > 0 && val < SDL_NUM_SCANCODES) {
                        s_key_mappings[i] = (SDL_Scancode)val;
                    }
                    break;
                }
            }
        }
    }
    fclose(f);
}

void window_set_pad_layout(int layout) {
    if (layout < 0) layout = 2;
    if (layout > 2) layout = 0;
    s_pad_layout = layout;
    window_save_controls_cfg();
    fprintf(stderr, "[input] Gamepad layout changed to: %s\n", s_layout_names[s_pad_layout]);
}


static void add_controller(int device_index) {
    if (!SDL_IsGameController(device_index)) return;

    // Check if this physical joystick is already opened in any player slot
    SDL_JoystickID jid = SDL_JoystickGetDeviceInstanceID(device_index);
    for (int p = 0; p < 2; p++) {
        if (s_pads[p] && s_pad_ids[p] == jid) {
            return; // Controller already registered
        }
    }

    for (int p = 0; p < 2; p++) {
        if (!s_pads[p]) {
            s_pads[p] = SDL_GameControllerOpen(device_index);
            if (s_pads[p]) {
                SDL_Joystick *joy = SDL_GameControllerGetJoystick(s_pads[p]);
                s_pad_ids[p] = SDL_JoystickInstanceID(joy);
                const char *name = SDL_GameControllerName(s_pads[p]);
                fprintf(stderr, "[input] Player %d gamepad connected: %s (Instance ID: %d)\n",
                        p + 1, name ? name : "Generic Gamepad", (int)s_pad_ids[p]);
                return;
            }
        }
    }
}

static void remove_controller(SDL_JoystickID instance_id) {
    for (int p = 0; p < 2; p++) {
        if (s_pads[p] && s_pad_ids[p] == instance_id) {
            fprintf(stderr, "[input] Player %d gamepad disconnected\n", p + 1);
            SDL_GameControllerClose(s_pads[p]);
            s_pads[p] = NULL;
            s_pad_ids[p] = -1;
            return;
        }
    }
}

static int init_opengl_backend(int w, int h) {
#ifdef NG_HAVE_OPENGL
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    s_gl_ctx = SDL_GL_CreateContext(s_win);
    if (!s_gl_ctx) {
        fprintf(stderr, "[render] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_GL_SetSwapInterval(s_vsync);

    glGenTextures(1, &s_gl_tex);
    glBindTexture(GL_TEXTURE_2D, s_gl_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Load OpenGL shader function pointers
    glCreateShader_p  = (PFN_glCreateShader)SDL_GL_GetProcAddress("glCreateShader");
    glShaderSource_p  = (PFN_glShaderSource)SDL_GL_GetProcAddress("glShaderSource");
    glCompileShader_p = (PFN_glCompileShader)SDL_GL_GetProcAddress("glCompileShader");
    glGetShaderiv_p   = (PFN_glGetShaderiv)SDL_GL_GetProcAddress("glGetShaderiv");
    glCreateProgram_p = (PFN_glCreateProgram)SDL_GL_GetProcAddress("glCreateProgram");
    glAttachShader_p  = (PFN_glAttachShader)SDL_GL_GetProcAddress("glAttachShader");
    glLinkProgram_p   = (PFN_glLinkProgram)SDL_GL_GetProcAddress("glLinkProgram");
    glGetProgramiv_p  = (PFN_glGetProgramiv)SDL_GL_GetProcAddress("glGetProgramiv");
    glUseProgram_p    = (PFN_glUseProgram)SDL_GL_GetProcAddress("glUseProgram");

    if (glCreateShader_p) {
        static const char *fs_scan_50 =
            "#version 120\n"
            "uniform sampler2D u_tex;\n"
            "varying vec2 v_texcoord;\n"
            "void main() {\n"
            "    vec4 col = texture2D(u_tex, v_texcoord);\n"
            "    float scan = sin(v_texcoord.y * 224.0 * 3.14159265 * 2.0);\n"
            "    float f = 0.75 + 0.25 * ((scan + 1.0) * 0.5);\n"
            "    gl_FragColor = vec4(col.rgb * f, col.a);\n"
            "}\n";

        static const char *fs_scan_75 =
            "#version 120\n"
            "uniform sampler2D u_tex;\n"
            "varying vec2 v_texcoord;\n"
            "void main() {\n"
            "    vec4 col = texture2D(u_tex, v_texcoord);\n"
            "    float scan = sin(v_texcoord.y * 224.0 * 3.14159265 * 2.0);\n"
            "    float f = 0.55 + 0.45 * ((scan + 1.0) * 0.5);\n"
            "    gl_FragColor = vec4(pow(col.rgb, vec3(0.95)) * f, col.a);\n"
            "}\n";

        static const char *fs_trinitron =
            "#version 120\n"
            "uniform sampler2D u_tex;\n"
            "varying vec2 v_texcoord;\n"
            "void main() {\n"
            "    vec4 col = texture2D(u_tex, v_texcoord);\n"
            "    float scan = sin(v_texcoord.y * 224.0 * 3.14159265 * 2.0);\n"
            "    float s_factor = 0.72 + 0.28 * ((scan + 1.0) * 0.5);\n"
            "    int px = int(v_texcoord.x * 320.0 * 3.0);\n"
            "    int m = px - (px / 3) * 3;\n"
            "    vec3 mask = (m == 0) ? vec3(1.15, 0.88, 0.88) : ((m == 1) ? vec3(0.88, 1.15, 0.88) : vec3(0.88, 0.88, 1.15));\n"
            "    gl_FragColor = vec4(col.rgb * s_factor * mask, col.a);\n"
            "}\n";

        static const char *fs_scale2x =
            "#version 120\n"
            "uniform sampler2D u_tex;\n"
            "varying vec2 v_texcoord;\n"
            "bool eq(vec4 a, vec4 b) { return distance(a, b) < 0.08; }\n"
            "void main() {\n"
            "    vec2 size = vec2(320.0, 224.0);\n"
            "    vec2 pos = v_texcoord * size;\n"
            "    vec2 p = floor(pos);\n"
            "    vec2 f = fract(pos);\n"
            "    vec2 dx = vec2(1.0 / size.x, 0.0);\n"
            "    vec2 dy = vec2(0.0, 1.0 / size.y);\n"
            "    vec2 tc = (p + 0.5) / size;\n"
            "    vec4 C = texture2D(u_tex, tc);\n"
            "    vec4 U = texture2D(u_tex, tc - dy);\n"
            "    vec4 D = texture2D(u_tex, tc + dy);\n"
            "    vec4 L = texture2D(u_tex, tc - dx);\n"
            "    vec4 R = texture2D(u_tex, tc + dx);\n"
            "    vec4 col = C;\n"
            "    if (f.x < 0.5 && f.y < 0.5) {\n"
            "        if (eq(U, L) && !eq(U, R) && !eq(L, D)) col = U;\n"
            "    } else if (f.x >= 0.5 && f.y < 0.5) {\n"
            "        if (eq(U, R) && !eq(U, L) && !eq(R, D)) col = U;\n"
            "    } else if (f.x < 0.5 && f.y >= 0.5) {\n"
            "        if (eq(D, L) && !eq(D, R) && !eq(L, U)) col = D;\n"
            "    } else {\n"
            "        if (eq(D, R) && !eq(D, L) && !eq(R, U)) col = D;\n"
            "    }\n"
            "    gl_FragColor = col;\n"
            "}\n";

        static const char *fs_crt_warm =
            "#version 120\n"
            "uniform sampler2D u_tex;\n"
            "varying vec2 v_texcoord;\n"
            "void main() {\n"
            "    vec2 cc = v_texcoord - 0.5;\n"
            "    float dist = dot(cc, cc);\n"
            "    vec2 uv = v_texcoord + cc * (dist * 0.08);\n"
            "    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {\n"
            "        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "        return;\n"
            "    }\n"
            "    vec4 col = texture2D(u_tex, uv);\n"
            "    float scan = sin(uv.y * 224.0 * 3.14159265 * 2.0);\n"
            "    float f = 0.70 + 0.30 * ((scan + 1.0) * 0.5);\n"
            "    vec3 warm = col.rgb * vec3(1.05, 1.0, 0.95);\n"
            "    float vignette = clamp(1.0 - dist * 0.6, 0.0, 1.0);\n"
            "    gl_FragColor = vec4(warm * f * vignette, col.a);\n"
            "}\n";

        s_gl_prog_scan_50   = compile_shader_prog(fs_scan_50);
        s_gl_prog_scan_75   = compile_shader_prog(fs_scan_75);
        s_gl_prog_trinitron = compile_shader_prog(fs_trinitron);
        s_gl_prog_scale2x   = compile_shader_prog(fs_scale2x);
        s_gl_prog_crt_warm  = compile_shader_prog(fs_crt_warm);
        fprintf(stderr, "[render] Initialized Video Post-Processing Shaders\n");
    }

    fprintf(stderr, "[render] Initialized Native OpenGL Backend\n");
    return 0;
#else
    (void)w; (void)h;
    fprintf(stderr, "[render] OpenGL not compiled into this build\n");
    return -1;
#endif
}

static int init_sdl_backend(int w, int h) {
    (void)w; (void)h;
#ifdef __vita__
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "VITA gxm");
#endif
    Uint32 render_flags = SDL_RENDERER_ACCELERATED | (s_vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
    s_ren = SDL_CreateRenderer(s_win, -1, render_flags);
    if (!s_ren) {
        s_ren = SDL_CreateRenderer(s_win, -1, 0);
    }
    if (!s_ren) {
        fprintf(stderr, "error: SDL_CreateRenderer: %s\n", SDL_GetError());
        return -1;
    }
    SDL_RenderSetLogicalSize(s_ren, NG_SCREEN_W, NG_SCREEN_H);
    s_tex = SDL_CreateTexture(s_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                              NG_SCREEN_W, NG_SCREEN_H);
    if (!s_tex) {
        fprintf(stderr, "error: SDL_CreateTexture: %s\n", SDL_GetError());
        return -1;
    }
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(s_tex, (s_video_filter == FILTER_BILINEAR)
                                   ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
#endif
    fprintf(stderr, "[render] Initialized SDL Accelerated Backend\n");
    return 0;
}

int window_open(const char *title, int scale) {

#ifdef _WIN32
    timeBeginPeriod(1);
#endif
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "error: SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    // Audio setup (48 kHz stereo 16-bit)
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
#ifdef __vita__
    want.samples = 512;
#else
    want.samples = 1024;
#endif
    s_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_audio_dev > 0) {
        SDL_PauseAudioDevice(s_audio_dev, 0);
        fprintf(stderr, "[audio] Opened audio device (%d Hz, %d ch)\n", have.freq, have.channels);
    } else {
        fprintf(stderr, "[audio] Failed to open audio device: %s\n", SDL_GetError());
    }

    // Load custom game controller database if present
    SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");

    // Load custom controller and video configuration
    load_controls_cfg();

    // Detect requested backend from environment (Default: OpenGL when compiled)
    const char *env_backend = getenv("NG_RENDERER");
    if (env_backend) {
        if (!strcasecmp(env_backend, "opengl") || !strcasecmp(env_backend, "gl")) {
            s_backend = RENDERER_OPENGL;
        } else if (!strcasecmp(env_backend, "vulkan") || !strcasecmp(env_backend, "vk")) {
            s_backend = RENDERER_VULKAN;
        } else {
            s_backend = RENDERER_SDL;
        }
    } else {
#ifdef NG_HAVE_OPENGL
        s_backend = RENDERER_OPENGL;
#endif
    }

    Uint32 win_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (s_backend == RENDERER_OPENGL) {
        win_flags |= SDL_WINDOW_OPENGL;
    } else if (s_backend == RENDERER_VULKAN) {
        win_flags |= SDL_WINDOW_VULKAN;
    }

#if defined(__PROSPERO__) || defined(__PS5__)
    int win_w = 1920;
    int win_h = 1080;
    win_flags |= SDL_WINDOW_FULLSCREEN;
#elif defined(__vita__)
    int win_w = 960;
    int win_h = 544;
    win_flags |= SDL_WINDOW_FULLSCREEN;
#else
    int win_w = NG_SCREEN_W * scale;
    int win_h = NG_SCREEN_H * scale;
#endif

    int win_x = SDL_WINDOWPOS_CENTERED;
    int win_y = SDL_WINDOWPOS_CENTERED;
    const char *env_pos_x = getenv("NG_WINDOW_X");
    const char *env_pos_y = getenv("NG_WINDOW_Y");
    if (env_pos_x) win_x = atoi(env_pos_x);
    if (env_pos_y) win_y = atoi(env_pos_y);

    s_win = SDL_CreateWindow(title, win_x, win_y,
                             win_w, win_h, win_flags);
    if (!s_win) {
        fprintf(stderr, "error: SDL_CreateWindow: %s\n", SDL_GetError());
        return -1;
    }

    int res = -1;
    if (s_backend == RENDERER_OPENGL) {
        res = init_opengl_backend(win_w, win_h);
    }
    if (res != 0) {
        // Fallback to SDL renderer
        s_backend = RENDERER_SDL;
        if (init_sdl_backend(win_w, win_h) != 0) {
            return -1;
        }
    }

    // Connect any currently plugged in controllers
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; i++) {
        add_controller(i);
    }

    s_next_tick = SDL_GetPerformanceCounter();
    return 0;
}

static void read_gamepad(int p, SDL_GameController *pad, ng_input_t *in) {
    if (!pad) return;
    uint8_t *bits = (p == 0) ? &in->p1 : &in->p2;

    // D-Pad
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    *bits |= NG_UP;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  *bits |= NG_DOWN;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  *bits |= NG_LEFT;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) *bits |= NG_RIGHT;

    // Left Analog Stick (Deadzone 8000)
    int16_t lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    int16_t ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    if (lx < -8000) *bits |= NG_LEFT;
    if (lx >  8000) *bits |= NG_RIGHT;
    if (ly < -8000) *bits |= NG_UP;
    if (ly >  8000) *bits |= NG_DOWN;

    // Action Buttons & Shortcuts based on selected layout preset:
    bool btn_a = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A); // Cross (A)
    bool btn_b = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B); // Circle (B)
    bool btn_x = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X); // Square (X)
    bool btn_y = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y); // Triangle (Y)
    bool btn_l1 = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    bool btn_r1 = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

    int16_t lt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    int16_t rt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    bool btn_l2 = (lt > 16000);
    bool btn_r2 = (rt > 16000);

    if (s_pad_layout == 0) {
        // Layout 0: SNK Default
        // Cross=A (LP), Circle=B (LK), Square=C (HP), Triangle=D (HK)
        if (btn_a)  *bits |= NG_A;
        if (btn_b)  *bits |= NG_B;
        if (btn_x)  *bits |= NG_C;
        if (btn_y)  *bits |= NG_D;
        if (btn_l1) *bits |= (NG_A | NG_B);               // Roll / Dodge
        if (btn_r1) *bits |= (NG_C | NG_D);               // Blow Away
    } else if (s_pad_layout == 1) {
        // Layout 1: Fighting Modern (Capcom / Street Fighter style layout)
        // Square=A (LP), Triangle=C (HP), Cross=B (LK), Circle=D (HK)
        if (btn_x)  *bits |= NG_A;
        if (btn_y)  *bits |= NG_C;
        if (btn_a)  *bits |= NG_B;
        if (btn_b)  *bits |= NG_D;
        if (btn_l1) *bits |= (NG_A | NG_B);               // Roll / Dodge (LP + LK)
        if (btn_r1) *bits |= (NG_C | NG_D);               // Blow Away (HP + HK)
    } else if (s_pad_layout == 2) {
        // Layout 2: KOF Classic Line
        // Square=A (LP), Cross=B (LK), Triangle=C (HP), R1=D (HK), Circle=Blow Away
        if (btn_x)  *bits |= NG_A;
        if (btn_a)  *bits |= NG_B;
        if (btn_y)  *bits |= NG_C;
        if (btn_r1) *bits |= NG_D;
        if (btn_b)  *bits |= (NG_C | NG_D);               // Circle: Blow Away
        if (btn_l1) *bits |= (NG_A | NG_B);               // L1: Roll
    }

    // Triggers (L2 / R2) and Right Stick shortcuts (useful on PS Vita handheld without L2/R2)
    int16_t rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
    int16_t ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
    // L2 or Right Stick Left/Up: Power Charge (A + B + C)
    if (btn_l2 || rx < -16000 || ry < -16000) *bits |= (NG_A | NG_B | NG_C);
    // R2 or Right Stick Right/Down: Taunt (B + C + D)
    if (btn_r2 || rx > 16000 || ry > 16000) *bits |= (NG_B | NG_C | NG_D);

    // System Buttons
    if (p == 0) {
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) in->start1 = 1;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) ||
            SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_TOUCHPAD) ||
            SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_GUIDE)) in->coin1 = 1;
        bool l3 = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSTICK);
        bool r3 = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
        if (l3 && !r3) { in->select1 = 1; in->coin1 = 1; }
    } else {
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) in->start2 = 1;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) ||
            SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_TOUCHPAD) ||
            SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_GUIDE)) in->coin2 = 1;
        bool l3 = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSTICK);
        if (l3) { in->select2 = 1; in->coin2 = 1; }
    }
}

static void read_keys(ng_input_t *in) {
    const Uint8 *k = SDL_GetKeyboardState(NULL);

    // Player 1 Keyboard
    if (k[s_key_mappings[KEY_ACT_P1_UP]])    in->p1 |= NG_UP;
    if (k[s_key_mappings[KEY_ACT_P1_DOWN]])  in->p1 |= NG_DOWN;
    if (k[s_key_mappings[KEY_ACT_P1_LEFT]])  in->p1 |= NG_LEFT;
    if (k[s_key_mappings[KEY_ACT_P1_RIGHT]]) in->p1 |= NG_RIGHT;
    if (k[s_key_mappings[KEY_ACT_P1_A]])     in->p1 |= NG_A;
    if (k[s_key_mappings[KEY_ACT_P1_B]])     in->p1 |= NG_B;
    if (k[s_key_mappings[KEY_ACT_P1_C]])     in->p1 |= NG_C;
    if (k[s_key_mappings[KEY_ACT_P1_D]])     in->p1 |= NG_D;

    // Player 2 Keyboard
    if (k[s_key_mappings[KEY_ACT_P2_UP]])    in->p2 |= NG_UP;
    if (k[s_key_mappings[KEY_ACT_P2_DOWN]])  in->p2 |= NG_DOWN;
    if (k[s_key_mappings[KEY_ACT_P2_LEFT]])  in->p2 |= NG_LEFT;
    if (k[s_key_mappings[KEY_ACT_P2_RIGHT]]) in->p2 |= NG_RIGHT;
    if (k[s_key_mappings[KEY_ACT_P2_A]])     in->p2 |= NG_A;
    if (k[s_key_mappings[KEY_ACT_P2_B]])     in->p2 |= NG_B;
    if (k[s_key_mappings[KEY_ACT_P2_C]])     in->p2 |= NG_C;
    if (k[s_key_mappings[KEY_ACT_P2_D]])     in->p2 |= NG_D;

    // System Keys
    in->start1  = k[s_key_mappings[KEY_ACT_START1]];
    in->start2  = k[s_key_mappings[KEY_ACT_START2]];
    in->coin1   = k[s_key_mappings[KEY_ACT_COIN1]];
    in->coin2   = k[s_key_mappings[KEY_ACT_COIN2]];
    in->select1 = k[SDL_SCANCODE_3];
    in->select2 = k[SDL_SCANCODE_4];
    in->service = k[SDL_SCANCODE_9];
    in->test    = k[SDL_SCANCODE_F2];
}

static void toggle_fullscreen(void) {
    s_is_fullscreen = !s_is_fullscreen;
    SDL_SetWindowFullscreen(s_win, s_is_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static void render_frame_gl(const uint32_t *fb) {
#ifdef NG_HAVE_OPENGL
    int win_w, win_h;
    SDL_GL_GetDrawableSize(s_win, &win_w, &win_h);

    // Maintain 4:3 arcade aspect ratio with letterboxing
    float target_ar = 4.0f / 3.0f;
    int vp_w = win_w;
    int vp_h = (int)(win_w / target_ar);
    if (vp_h > win_h) {
        vp_h = win_h;
        vp_w = (int)(win_h * target_ar);
    }
    int vp_x = (win_w - vp_w) / 2;
    int vp_y = (win_h - vp_h) / 2;

    glViewport(0, 0, win_w, win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(vp_x, vp_y, vp_w, vp_h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, NG_SCREEN_W, NG_SCREEN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    GLenum filter_mode = (s_video_filter == FILTER_NEAREST || s_video_filter == FILTER_SCALE2X)
                         ? GL_NEAREST : GL_LINEAR;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s_gl_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mode);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, NG_SCREEN_W, NG_SCREEN_H, 0,
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, fb);

    GLuint prog = 0;
    if (s_video_filter == FILTER_CRT_SCANLINES_50)      prog = s_gl_prog_scan_50;
    else if (s_video_filter == FILTER_CRT_SCANLINES_75) prog = s_gl_prog_scan_75;
    else if (s_video_filter == FILTER_CRT_TRINITRON)    prog = s_gl_prog_trinitron;
    else if (s_video_filter == FILTER_SCALE2X)          prog = s_gl_prog_scale2x;
    else if (s_video_filter == FILTER_CRT_WARM)         prog = s_gl_prog_crt_warm;

    if (glUseProgram_p) glUseProgram_p(prog);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f((float)NG_SCREEN_W, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f((float)NG_SCREEN_W, (float)NG_SCREEN_H);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, (float)NG_SCREEN_H);
    glEnd();

    if (glUseProgram_p) glUseProgram_p(0);

    glDisable(GL_TEXTURE_2D);
    SDL_GL_SwapWindow(s_win);
#else
    (void)fb;
#endif
}

static void render_frame_sdl(const uint32_t *fb) {
    const uint32_t *upload_fb = fb;

#ifndef __vita__
    static uint32_t s_sdl_filtered_fb[NG_SCREEN_W * NG_SCREEN_H];
    if (s_video_filter >= FILTER_CRT_SCANLINES_50 && s_video_filter <= FILTER_CRT_WARM && s_video_filter != FILTER_SCALE2X) {
        for (int y = 0; y < NG_SCREEN_H; y++) {
            uint32_t *d = &s_sdl_filtered_fb[y * NG_SCREEN_W];
            const uint32_t *s = &fb[y * NG_SCREEN_W];
            if (y % 2 == 1) {
                for (int x = 0; x < NG_SCREEN_W; x++) {
                    uint32_t c = s[x];
                    uint32_t r = (s_video_filter == FILTER_CRT_SCANLINES_75) ? (((c >> 16) & 0xFF) * 141) >> 8 : (((c >> 16) & 0xFF) * 3) >> 2;
                    uint32_t g = (s_video_filter == FILTER_CRT_SCANLINES_75) ? (((c >> 8) & 0xFF) * 141) >> 8 : (((c >> 8) & 0xFF) * 3) >> 2;
                    uint32_t b = (s_video_filter == FILTER_CRT_SCANLINES_75) ? ((c & 0xFF) * 141) >> 8 : ((c & 0xFF) * 3) >> 2;
                    d[x] = (c & 0xFF000000) | (r << 16) | (g << 8) | b;
                }
            } else {
                memcpy(d, s, NG_SCREEN_W * sizeof(uint32_t));
            }
        }
        upload_fb = s_sdl_filtered_fb;
    }
#endif

    SDL_Rect update_rect = {0, 0, NG_SCREEN_W, NG_SCREEN_H};
    SDL_UpdateTexture(s_tex, &update_rect, upload_fb, NG_SCREEN_W * 4);
    SDL_RenderClear(s_ren);
    SDL_RenderCopy(s_ren, s_tex, NULL, NULL);
    SDL_RenderPresent(s_ren);
}

bool window_poll_input(ng_input_t *in) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return false;

        // Give OSD menu first chance at keyboard events
        if (osd_menu_handle_event(&e)) continue;

        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) return false;
            // Fullscreen toggle: Alt+Enter or F11
            if (e.key.keysym.sym == SDLK_F11 ||
                (e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT))) {
                toggle_fullscreen();
            }
            // Quick filter toggle: F5
            if (e.key.keysym.sym == SDLK_F5) {
                window_set_video_filter((s_video_filter + 1) % NUM_VIDEO_FILTERS);
            }
        }

        if (e.type == SDL_CONTROLLERDEVICEADDED) {
            add_controller(e.cdevice.which);
        }
        if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
            remove_controller(e.cdevice.which);
        }
    }

    // Check gamepad navigation for OSD menu
    if (s_pads[0]) osd_menu_update_gamepad(s_pads[0]);

    static bool s_was_menu_open = false;
    bool menu_open = osd_menu_is_open();
    if (menu_open != s_was_menu_open) {
        s_was_menu_open = menu_open;
        if (s_audio_dev > 0) {
            SDL_PauseAudioDevice(s_audio_dev, menu_open ? 1 : 0);
        }
    }

    if (osd_menu_is_open()) {
        // Suppress in-game player actions while configuring DIP switches
        memset(in, 0, sizeof(*in));
    } else {
        // Read keyboard inputs
        read_keys(in);

        // Read gamepads (Player 1 & Player 2)
        read_gamepad(0, s_pads[0], in);
        read_gamepad(1, s_pads[1], in);

        // Check for P2 keyboard IPC input (from test_latency.sh / fbneo_sdl on desktop)
#if !defined(__vita__) && !defined(_WIN32)
        static int s_p2_file_fd = -1;
        static int s_p2_retry = 0;
        if (s_p2_file_fd < 0 && (++s_p2_retry % 30 == 1)) {
            const char *home = getenv("HOME");
            if (home) {
                char p2_path[512];
                snprintf(p2_path, sizeof(p2_path),
                         "%s/Library/Application Support/Vita3K/Vita3K/ux0/data/kof95/p2_input.bin", home);
                s_p2_file_fd = open(p2_path, O_RDONLY);
            }
        }
        if (s_p2_file_fd >= 0) {
            uint8_t p2_data[4];
            if (pread(s_p2_file_fd, p2_data, 4, 0) == 4) {
                in->p2 |= p2_data[0];
                if (p2_data[1]) in->start2 = 1;
                if (p2_data[2]) in->coin2 = 1;
            }
        }
#endif
    }
    return true;
}

void window_present(const uint32_t *fb) {
    if (!fb) return;

    const uint32_t *render_fb = fb;
    static uint32_t s_disp_fb[NG_SCREEN_W * NG_SCREEN_H];

    if (osd_menu_is_open()) {
        memcpy(s_disp_fb, fb, sizeof(s_disp_fb));
        osd_menu_render(s_disp_fb);
        render_fb = s_disp_fb;
    }

    uint64_t t_before = SDL_GetPerformanceCounter();
    if (s_backend == RENDERER_OPENGL) {
        render_frame_gl(render_fb);
    } else {
        render_frame_sdl(render_fb);
    }
    uint64_t t_after = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();
    double swap_ms = (double)(t_after - t_before) * 1000.0 / (double)freq;

    /* Pace to the MVS refresh rate, 59.1856 Hz.
     * When VSync is ON and SwapWindow already blocked on hardware VSync (took >= 6ms),
     * skip software SDL_Delay to eliminate double waiting and GPU queue lag.
     * On PS5 and PS Vita (GXM display queue), SDL_RenderPresent paces on hardware VBlank. */
#if defined(__vita__)
    /* On Vita with VSync ON, GXM hardware paces on 60 Hz VBlank.
     * If VSync is OFF, pace in software to 59.1856 Hz to prevent fast-forward running. */
    if (!s_vsync) {
        s_next_tick += (uint64_t)((double)freq / 59.185606);
        if (t_after < s_next_tick) {
            Uint32 sleep_ms = (Uint32)((s_next_tick - t_after) * 1000 / freq);
            if (sleep_ms > 0) SDL_Delay(sleep_ms);
        } else if (t_after - s_next_tick > freq / 10) {
            s_next_tick = t_after;
        }
    }
#elif !defined(__PROSPERO__) && !defined(__PS5__)
    if (s_vsync && swap_ms >= 6.0) {
        s_next_tick = t_after;
    } else {
        s_next_tick += (uint64_t)((double)freq / 59.185606);
        if (t_after < s_next_tick) {
            Uint32 sleep_ms = (Uint32)((s_next_tick - t_after) * 1000 / freq);
            if (sleep_ms > 0) SDL_Delay(sleep_ms);
        } else if (t_after - s_next_tick > freq / 10) {
            s_next_tick = t_after;
        }
    }
#else
    (void)swap_ms;
#endif
}

bool window_frame(const uint32_t *fb, ng_input_t *in) {
    if (!window_poll_input(in)) return false;
    if (fb) window_present(fb);
    return true;
}

void window_audio(const int16_t *samples, size_t frames) {
    if (!s_audio_dev || !samples || frames == 0) return;

#ifdef __vita__
    // Tighter ~42ms audio queue cap on PS Vita to minimize audio latency
    Uint32 max_queued = 2048 * 2 * sizeof(int16_t);
#else
    // Buffer limit: don't let audio lag more than ~100ms (4800 samples)
    Uint32 max_queued = 4800 * 2 * sizeof(int16_t);
#endif
    Uint32 queued = SDL_GetQueuedAudioSize(s_audio_dev);
    if (queued < max_queued) {
        SDL_QueueAudio(s_audio_dev, samples, (Uint32)(frames * 2 * sizeof(int16_t)));
    }
}

void window_close(void) {
    if (s_audio_dev) {
        SDL_CloseAudioDevice(s_audio_dev);
        s_audio_dev = 0;
    }
    for (int p = 0; p < 2; p++) {
        if (s_pads[p]) {
            SDL_GameControllerClose(s_pads[p]);
            s_pads[p] = NULL;
        }
    }
#ifdef NG_HAVE_OPENGL
    if (s_gl_tex) { glDeleteTextures(1, &s_gl_tex); s_gl_tex = 0; }
    if (s_gl_ctx) { SDL_GL_DeleteContext(s_gl_ctx); s_gl_ctx = NULL; }
#endif
    if (s_tex) { SDL_DestroyTexture(s_tex); s_tex = NULL; }
    if (s_ren) { SDL_DestroyRenderer(s_ren); s_ren = NULL; }
    if (s_win) { SDL_DestroyWindow(s_win); s_win = NULL; }

#ifdef _WIN32
    timeEndPeriod(1);
#endif
    SDL_Quit();
}
