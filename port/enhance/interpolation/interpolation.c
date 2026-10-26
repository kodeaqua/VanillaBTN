#include "enhance/interpolation/interpolation.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <stdlib.h>
#include <string.h>

enum { WORDS = 65536, COMMANDS = 16384, HASH = 32768 };
typedef struct {
    uintptr_t address;
    u32 offset, length;
    int arena, world, previous;
    u16 page;
    s16 dx, dy;
} command;
typedef struct {
    hm_render_frame meta;
    hm_vram_context before;
    hm_gpu_environment env;
    command commands[COMMANDS];
    u32 words[WORDS], count, used;
    int complete;
} frame;
typedef struct {
    frame frames[2];
    hm_vram_context work;
    hm_gpu_environment environment;
    int maps[2][HASH];
    int current, capturing, world, ready;
} interpolation;
static interpolation *state;

static void invalidate(void *opaque) {
    interpolation *s = opaque;
    s->ready = s->capturing = 0;
    s->frames[0].complete = s->frames[1].complete = 0;
}
static void begin(void *opaque, const hm_render_frame *meta) {
    interpolation *s = opaque;
    s->current ^= 1;
    frame *f = &s->frames[s->current];
    f->meta = *meta;
    f->count = f->used = 0;
    f->complete = 0;
    s->ready = s->world = 0;
    s->capturing = meta->eligible;
    if (!s->capturing)
        return;
    hm_vram_snapshot(&f->before);
    f->env = *hm_gpu_environment_state();
}
static void group(void *opaque, int world) { ((interpolation *)opaque)->world = world; }
static void packet(void *opaque, const u32 *words, u32 count) {
    interpolation *s = opaque;
    if (!s->capturing)
        return;
    frame *f = &s->frames[s->current];
    if (f->count == COMMANDS || count > WORDS - f->used) {
        s->capturing = 0;
        return;
    }
    command *c = &f->commands[f->count++];
    *c = (command){.address = (uintptr_t)words,
                   .offset = f->used,
                   .length = count,
                   .world = s->world,
                   .previous = -1};
    for (int i = 0; i < 2; ++i) {
        uintptr_t base = (uintptr_t)f->meta.arena[i];
        if (base && c->address >= base && c->address - base < f->meta.arena_bytes[i]) {
            c->arena = i + 1;
            c->address -= base;
            break;
        }
    }
    const hm_gpu_environment *env = hm_gpu_environment_state();
    c->page = env->draw.tpage;
    c->dx = env->draw.ofs[0] - f->env.draw.ofs[0];
    c->dy = env->draw.ofs[1] - f->env.draw.ofs[1];
    memcpy(f->words + f->used, words, count * sizeof(*words));
    f->used += count;
}
static u32 bucket(const command *c) {
    uintptr_t h = c->address ^ ((uintptr_t)c->arena * 0x9e3779b9u);
    h ^= h >> 17;
    return (u32)h & (HASH - 1);
}
static int same_key(const command *a, const command *b) {
    return a->address == b->address && a->arena == b->arena;
}
/* Keep duplicate addresses ambiguous: a static packet can be submitted twice. */
static void make_map(frame *f, int *map) {
    memset(map, 0, HASH * sizeof(*map));
    for (u32 i = 0; i < f->count; ++i) {
        u32 slot = bucket(&f->commands[i]);
        while (map[slot]) {
            int index = abs(map[slot]) - 1;
            if (same_key(&f->commands[i], &f->commands[index])) {
                map[slot] = -(index + 1);
                break;
            }
            slot = (slot + 1) & (HASH - 1);
        }
        if (!map[slot])
            map[slot] = (int)i + 1;
    }
}
static int find(frame *f, int *map, const command *c) {
    u32 slot = bucket(c);
    while (map[slot]) {
        int index = abs(map[slot]) - 1;
        if (same_key(c, &f->commands[index]))
            return map[slot] > 0 ? index : -1;
        slot = (slot + 1) & (HASH - 1);
    }
    return -1;
}
static int coordinates(const u32 *w, u32 length, int *indices) {
    u32 op = w[0] >> 24;
    if (op >= 0x20 && op <= 0x3f) {
        int vertices = op & 8 ? 4 : 3, at = 1;
        for (int i = 0; i < vertices; ++i) {
            if (i && (op & 16))
                ++at;
            indices[i] = at++;
            if (op & 4)
                ++at;
        }
        return (u32)at == length ? vertices : 0;
    }
    if (op >= 0x60 && op <= 0x7f && length >= 2) {
        indices[0] = 1;
        return 1;
    }
    if (op >= 0x40 && op <= 0x5f && !(op & 8)) {
        indices[0] = 1;
        indices[1] = op & 16 ? 3 : 2;
        return (u32)indices[1] < length ? 2 : 0;
    }
    return 0;
}
static int coordinate(u32 value) { return (int)((value & 0x7ff) ^ 0x400) - 0x400; }
static int compatible(frame *a, command *ca, frame *b, command *cb) {
    if (!ca->world || !cb->world || ca->length != cb->length || ca->page != cb->page ||
        ca->dx != cb->dx || ca->dy != cb->dy)
        return 0;
    const u32 *wa = a->words + ca->offset, *wb = b->words + cb->offset;
    int indices[4], n = coordinates(wb, cb->length, indices);
    if (!n || cb->length > 16)
        return 0;
    for (u32 i = 0; i < cb->length; ++i) {
        int xy = 0;
        for (int j = 0; j < n; ++j)
            if ((u32)indices[j] == i)
                xy = 1;
        if (xy) {
            for (int shift = 0; shift <= 16; shift += 16)
                if (abs(coordinate(wa[i] >> shift) - coordinate(wb[i] >> shift)) > 12)
                    return 0;
        } else if (wa[i] != wb[i]) {
            /* Sprite animation changes its UV pose at native cadence. */
            u32 op = wb[0] >> 24;
            if (!(op >= 0x60 && op <= 0x7f && (op & 4) && i == 2 && (wa[i] >> 16) == (wb[i] >> 16)))
                return 0;
        }
    }
    return 1;
}
static void end(void *opaque) {
    interpolation *s = opaque;
    frame *f = &s->frames[s->current], *p = &s->frames[s->current ^ 1];
    if (!s->capturing)
        return;
    s->capturing = 0;
    f->complete = f->count != 0;
    if (!f->complete || !p->complete || f->meta.key != p->meta.key ||
        f->meta.tick != p->meta.tick + 2 || f->env.draw.clip.w != p->env.draw.clip.w ||
        f->env.draw.clip.h != p->env.draw.clip.h)
        return;
    make_map(p, s->maps[0]);
    make_map(f, s->maps[1]);
    for (u32 i = 0; i < f->count; ++i) {
        command *c = &f->commands[i];
        int previous = find(p, s->maps[0], c);
        if (previous >= 0 && find(f, s->maps[1], c) == (int)i &&
            compatible(p, &p->commands[previous], f, c))
            c->previous = previous;
    }
    s->ready = 1;
}
int hm_interpolation_enable(int enabled) {
    if (!enabled) {
        hm_render_subscribe((hm_render_observer){0});
        free(state);
        state = NULL;
        return 1;
    }
    if (state)
        return 1;
    state = calloc(1, sizeof(*state));
    if (!state)
        return 0;
    hm_render_subscribe((hm_render_observer){state, begin, group, packet, end, invalidate});
    return 1;
}
int hm_interpolation_render(u64 ns, const hm_render_frame *live, hm_interpolated_view *view) {
    interpolation *s = state;
    if (!s || !s->ready)
        return 0;
    frame *f = &s->frames[s->current], *p = &s->frames[s->current ^ 1];
    if (!live->eligible || live->key != f->meta.key) {
        invalidate(s);
        return 0;
    }
    RECT area = f->env.draw.clip;
    if (area.x < 0 || area.y < 0 || area.w <= 0 || area.h <= 0 || area.x + area.w > 1024 ||
        area.y + area.h > 512)
        return 0;
    u64 start = f->meta.tick * 1000000000ull / 60;
    u64 elapsed = ns > start ? ns - start : 0;
    u32 alpha = elapsed >= 33333334 ? 65536 : (u32)(elapsed * 60 * 65536 / 2000000000ull);
    s->work = f->before;
    s->environment = f->env;
    hm_vram_context *saved_vram = hm_vram_bind(&s->work);
    hm_gpu_environment *saved_env = hm_gpu_environment_bind(&s->environment);
    for (u32 i = 0; i < f->count; ++i) {
        command *c = &f->commands[i];
        const u32 *words = f->words + c->offset;
        u32 scratch[16];
        if (c->previous >= 0) {
            const u32 *old = p->words + p->commands[c->previous].offset;
            int indices[4], n = coordinates(words, c->length, indices);
            memcpy(scratch, words, c->length * sizeof(u32));
            for (int j = 0; j < n; ++j) {
                int index = indices[j];
                u32 xy = 0;
                for (int shift = 0; shift <= 16; shift += 16) {
                    int a = coordinate(old[index] >> shift), b = coordinate(words[index] >> shift);
                    s64 delta = (s64)(b - a) * alpha;
                    int value = a + (int)(delta >= 0 ? (delta + 32768) / 65536
                                                     : -((-delta + 32768) / 65536));
                    xy |= (u32)(u16)value << shift;
                }
                scratch[index] = xy;
            }
            words = scratch;
        }
        hm_gpu_execute_words(words, c->length);
    }
    hm_gpu_environment_bind(saved_env);
    hm_vram_bind(saved_vram);
    *view = (hm_interpolated_view){&s->work.pixels[0][0], 1024, area};
    return 1;
}
