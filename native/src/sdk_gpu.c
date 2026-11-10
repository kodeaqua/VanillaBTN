#include "sdk_gpu.h"
#include "ps1_gpu.h"
#include <libgpu.h>
#include <libgte.h>
#include <libgs.h>
#include <string.h>
#include <stdlib.h>

_Static_assert(sizeof(u_long) == 4, "SDK words must be 32-bit");
_Static_assert(sizeof(RECT) == 8, "SDK rectangles contain four halfwords");

short HWD0 = 320, VWD0 = 240;

static Ps1Video *vram;
static Ps1DrawState draw;
static DISPENV display;
static bool display_enabled;

SdkDisplay sdk_gpu_display(void) {
    return (SdkDisplay){display.disp.x, display.disp.y, display.disp.w, display.disp.h, display_enabled};
}

#define SDK_END 0xFFFFFFul
#define SDK_MAP_COUNT 32
typedef struct PacketMap {
    unsigned char *memory;
    size_t size;
    u_long first;
} PacketMap;
static PacketMap maps[SDK_MAP_COUNT];
static unsigned map_count;
static u_long next_id;
static bool packet_error;

void sdk_gpu_reset_packets(void) {
    map_count = 0;
    next_id = 4;
    packet_error = false;
}

bool sdk_gpu_ok(void) { return !packet_error; }

bool sdk_gpu_map(void *memory, size_t size) {
    uintptr_t start = (uintptr_t)memory;
    if (memory == NULL || size == 0 || (start & 3) || (size & 3)
        || size > SDK_END - next_id || start > UINTPTR_MAX - size
        || map_count == SDK_MAP_COUNT) return false;
    for (unsigned i = 0; i < map_count; ++i) {
        uintptr_t other = (uintptr_t)maps[i].memory;
        if (start < other + maps[i].size && other < start + size) return false;
    }
    maps[map_count++] = (PacketMap){memory, size, next_id};
    next_id += (u_long)size;
    return true;
}

static u_long address_id(const void *pointer, size_t bytes) {
    uintptr_t address = (uintptr_t)pointer;
    if (address & 3) return 0;
    for (unsigned i = 0; i < map_count; ++i) {
        uintptr_t base = (uintptr_t)maps[i].memory;
        if (address >= base && address - base <= maps[i].size
            && bytes <= maps[i].size - (address - base))
            return maps[i].first + (u_long)(address - base);
    }
    return 0;
}

static u_long *resolve_id(u_long id, size_t bytes) {
    for (unsigned i = 0; i < map_count; ++i) {
        if (id >= maps[i].first && id - maps[i].first <= maps[i].size
            && bytes <= maps[i].size - (id - maps[i].first))
            return (u_long *)(maps[i].memory + id - maps[i].first);
    }
    return NULL;
}

void ClearOTagR(u_long *ot, long count) {
    if (count <= 0 || (!address_id(ot, (size_t)count * 4)
        && !sdk_gpu_map(ot, (size_t)count * 4))) {
        packet_error = true;
        return;
    }
    ot[0] = SDK_END;
    for (long i = 1; i < count; ++i) ot[i] = address_id(ot + i - 1, 4);
}

void sdk_add_prim(void *ot, void *packet) {
    if (!address_id(ot, 4) || !packet || ((uintptr_t)packet & 3) || ot == packet) {
        packet_error = true;
        return;
    }
    u_long *tag = packet;
    size_t bytes = (size_t)((*tag >> 24) + 1) * 4;
    /* Original modules also own private static packets outside their frame
     * arenas. The SDK call receives a trusted live host pointer; register that
     * packet lazily. Serialized links still resolve only through mappings.
     * Partial overlap with a declared arena remains a hard bounds failure. */
    u_long id = address_id(packet, bytes);
    if (!id && sdk_gpu_map(packet, bytes)) id = address_id(packet, bytes);
    if (!id) { packet_error = true; return; }
    *tag = (*tag & 0xFF000000ul) | (*(u_long *)ot & SDK_END);
    *(u_long *)ot = (*(u_long *)ot & 0xFF000000ul) | id;
}

void DrawOTag(u_long *ot) {
    u_long id = address_id(ot, 4);
    size_t budget = next_id / 4;
    if (vram == NULL || !id || packet_error) { packet_error = true; return; }
    while (id != SDK_END && budget-- > 0) {
        u_long *packet = resolve_id(id, 4);
        if (!packet || (id & 3)) { packet_error = true; return; }
        unsigned words = *packet >> 24;
        if (!resolve_id(id, (size_t)(words + 1) * 4)) { packet_error = true; return; }
        if (words) {
            /* long and uint32_t have the same representation on Windows, but
             * copy into the dispatcher's type instead of aliasing them. */
            uint32_t commands[255];
            memcpy(commands, packet + 1, (size_t)words * 4);
            if (!ps1_gpu_exec(vram, &draw, commands, words)) { packet_error = true; return; }
        }
        id = *packet & SDK_END;
    }
    if (id != SDK_END) packet_error = true; /* Cycles cannot hang the host. */
}

static DRAWENV gs_draw[2];
static DISPENV gs_display[2];
static u_long *gs_work;
static int graph_debug;
static short gs_origin_x = 160, gs_origin_y = 120;

void sdk_gpu_bind(Ps1Video *video) {
    vram = video;
    ps1_draw_state_reset(&draw);
    sdk_gpu_reset_packets();
    SetDefDispEnv(&display, 0, 240, 320, 240);
    display_enabled = true;
    SetDefDrawEnv(&gs_draw[0], 0, 0, 320, 240);
    SetDefDrawEnv(&gs_draw[1], 0, 240, 320, 240);
    SetDefDispEnv(&gs_display[0], 0, 240, 320, 240);
    SetDefDispEnv(&gs_display[1], 0, 0, 320, 240);
    gs_origin_x = 160;
    gs_origin_y = 120;
}

static bool valid_area(int x, int y, int w, int h) {
    return x >= 0 && y >= 0 && w > 0 && h > 0
        && w <= PS1_VRAM_WIDTH - x && h <= PS1_VRAM_HEIGHT - y;
}

DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h) {
    if (!env || !valid_area(x, y, w, h)) return NULL;
    memset(env, 0, sizeof(*env));
    env->clip = (RECT){(short)x, (short)y, (short)w, (short)h};
    env->ofs[0] = (short)x;
    env->ofs[1] = (short)y;
    env->dtd = 1;
    return env;
}

DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h) {
    if (!env || !valid_area(x, y, w, h)) return NULL;
    memset(env, 0, sizeof(*env));
    env->disp = (RECT){(short)x, (short)y, (short)w, (short)h};
    return env;
}

DRAWENV *PutDrawEnv(DRAWENV *env) {
    if (!vram || !env || !valid_area(env->clip.x, env->clip.y, env->clip.w, env->clip.h)
        || env->tw.x || env->tw.y || env->tw.w || env->tw.h) {
        packet_error = true;
        return NULL;
    }
    draw.left = env->clip.x;
    draw.top = env->clip.y;
    draw.right = env->clip.x + env->clip.w - 1;
    draw.bottom = env->clip.y + env->clip.h - 1;
    draw.offset_x = env->ofs[0];
    draw.offset_y = env->ofs[1];
    draw.tpage = env->tpage & 0x1FF;
    draw.blend_mode = (env->tpage >> 5) & 3;
    if (env->isbg) ClearImage(&env->clip, env->r0, env->g0, env->b0);
    return env;
}

DISPENV *PutDispEnv(DISPENV *env) {
    if (!env || !valid_area(env->disp.x, env->disp.y, env->disp.w, env->disp.h) || env->isrgb24) {
        packet_error = true;
        return NULL;
    }
    display = *env;
    return env;
}

DISPENV *GetDispEnv(DISPENV *env) {
    if (!env) return NULL;
    *env = display;
    return env;
}

void SetDispMask(int mask) { display_enabled = mask != 0; }

int ClearImage(RECT *rect, u_char r, u_char g, u_char b) {
    if (!vram || !rect || !valid_area(rect->x, rect->y, rect->w, rect->h)) return -1;
    ps1_video_quick_fill(vram, rect->x, rect->y, rect->w, rect->h,
                        (uint32_t)r << 16 | (uint32_t)g << 8 | b);
    return 0;
}

int MoveImage(RECT *rect, int x, int y) {
    if (!vram || !rect || !valid_area(rect->x, rect->y, rect->w, rect->h)
        || !valid_area(x, y, rect->w, rect->h)) return -1;
    size_t size = (size_t)rect->w * rect->h * 2;
    unsigned char *copy = malloc(size);
    if (!copy) return -1;
    for (int row = 0; row < rect->h; ++row)
        memcpy(copy + (size_t)row * rect->w * 2, &vram->vram[rect->y + row][rect->x], (size_t)rect->w * 2);
    for (int row = 0; row < rect->h; ++row)
        memcpy(&vram->vram[y + row][x], copy + (size_t)row * rect->w * 2, (size_t)rect->w * 2);
    free(copy);
    return 0;
}

static int valid_transfer(const RECT *rect, const void *pixels) {
    return vram != NULL && rect != NULL && pixels != NULL
        && rect->x >= 0 && rect->y >= 0 && rect->w > 0 && rect->h > 0
        && rect->w <= PS1_VRAM_WIDTH - rect->x
        && rect->h <= PS1_VRAM_HEIGHT - rect->y;
}

int LoadImage(RECT *rect, u_long *pixels) {
    if (!valid_transfer(rect, pixels)) return -1;
    /* The SDK pointer is word-typed, but each row contains w halfwords.
     * Copy bytes so odd widths and unaligned bank payloads remain valid. */
    for (int row = 0; row < rect->h; ++row) {
        memcpy(&vram->vram[rect->y + row][rect->x],
               (const unsigned char *)pixels + (size_t)row * rect->w * 2,
               (size_t)rect->w * 2);
    }
    return 0;
}

int StoreImage(RECT *rect, u_long *pixels) {
    if (!valid_transfer(rect, pixels)) return -1;
    for (int row = 0; row < rect->h; ++row) {
        memcpy((unsigned char *)pixels + (size_t)row * rect->w * 2,
               &vram->vram[rect->y + row][rect->x], (size_t)rect->w * 2);
    }
    return 0;
}

int DrawSync(int mode) {
    /* There is no pending queue: transfers above finish before returning.
     * Both waiting (0) and polling (1) therefore report completion. */
    return mode == 0 || mode == 1 ? 0 : -1;
}

void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw) {
    if (!p || (tw && (tw->x != 0 || tw->y != 0 || tw->w != 0 || tw->h != 0))) {
        packet_error = true;
        return;
    }
    setlen(p, 2);
    p->code[0] = 0xE1000000ul | ((dfe) ? 0x400ul : 0ul)
               | ((dtd) ? 0x200ul : 0ul) | ((u_long)tpage & 0x9FFul);
    p->code[1] = 0xE2000000ul;
}

void SetDrawArea(DR_AREA *p, RECT *r) {
    if (!p || !r || !valid_area(r->x, r->y, r->w, r->h)) {
        packet_error = true;
        return;
    }
    setlen(p, 2);
    p->code[0] = 0xE3000000ul | (((u_long)((unsigned)r->y & 0x3FFu)) << 10)
               | ((u_long)((unsigned)r->x & 0x3FFu));
    p->code[1] = 0xE4000000ul | (((u_long)((unsigned)(r->y + r->h - 1) & 0x3FFu)) << 10)
               | ((u_long)((unsigned)(r->x + r->w - 1) & 0x3FFu));
}

void SetDrawLoad(DR_LOAD *p, RECT *r) {
    if (!p || !r || !valid_area(r->x, r->y, r->w, r->h)) {
        packet_error = true;
        return;
    }
    unsigned data_words = ((unsigned)r->w * (unsigned)r->h + 1u) / 2u;
    if (data_words > sizeof(p->p) / sizeof(p->p[0])) {
        packet_error = true;
        return;
    }
    setlen(p, 3u + data_words);
    p->code[0] = 0xA0000000ul;
    p->code[1] = ((u_long)(uint16_t)r->y << 16) | (u_long)(uint16_t)r->x;
    p->code[2] = ((u_long)(uint16_t)r->h << 16) | (u_long)(uint16_t)r->w;
}

/* GS bootstrap state is kept separately from the game's explicit per-frame
 * environments. PutDrawEnv/PutDispEnv remain authoritative when submitted. */

int ResetGraph(int mode) {
    if (mode != 0) abort(); /* Reset variants are not used by this port yet. */
    ps1_draw_state_reset(&draw);
    display_enabled = false;
    return 0;
}

int SetGraphDebug(int level) {
    int previous = graph_debug;
    graph_debug = level;
    /* Diagnostics are intentionally not emitted in either profile. */
    return previous;
}

void GsSetWorkBase(u_long *packets) { gs_work = packets; }
u_long *GsGetWorkBase(void) { return gs_work; }

void GsInitGraph(unsigned short x, unsigned short y, unsigned short mode,
                 unsigned short dith, unsigned short vrammode) {
    if (!x || !y || mode || vrammode) abort();
    HWD0 = (short)x;
    VWD0 = (short)y;
    gs_origin_x = (short)(x / 2);
    gs_origin_y = (short)(y / 2);
    for (unsigned i = 0; i < 2; ++i) {
        if (!SetDefDrawEnv(&gs_draw[i], 0, 0, x, y) ||
            !SetDefDispEnv(&gs_display[i], 0, 0, x, y)) abort();
        gs_draw[i].dtd = dith != 0;
    }
}

void GsDefDispBuff(unsigned short x0, unsigned short y0,
                   unsigned short x1, unsigned short y1) {
    if (!SetDefDrawEnv(&gs_draw[0], x0, y0, HWD0, VWD0) ||
        !SetDefDrawEnv(&gs_draw[1], x1, y1, HWD0, VWD0) ||
        !SetDefDispEnv(&gs_display[0], x1, y1, HWD0, VWD0) ||
        !SetDefDispEnv(&gs_display[1], x0, y0, HWD0, VWD0)) abort();
    GsSetDrawBuffOffset();
}

void GsSetOffset(long x, long y) {
    /* The supported mode uses the inactive GS buffer's VRAM origin; the
     * original game never swaps GS buffers, so buffer 1 is inactive. */
    SetGeomOffset((short)(gs_draw[1].clip.x + x),
                  (short)(gs_draw[1].clip.y + y));
}

void GsSetDrawBuffOffset(void) {
    SetGeomOffset((short)(gs_draw[1].clip.x + gs_origin_x),
                  (short)(gs_draw[1].clip.y + gs_origin_y));
}

void GsInit3D(void) {
    InitGeom();
    gs_origin_x = (short)(HWD0 / 2);
    gs_origin_y = (short)(VWD0 / 2);
    GsSetDrawBuffOffset();
}
