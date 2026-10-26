#include "compat/save_codec.h"
#include "compat/save_partner.h"
#include "farm_state.h"
#include <stddef.h>
#include <string.h>
/* Fixed wire spans follow the original SaveFile/SaveImage. No host pointer or
 * compiler-dependent record is overlaid onto these bytes. */
typedef struct {
    u8 animals[0x1f34], records[37][14], pad213a[2], run16[0xf0], run18[0x30];
    u8 flags[0x18], table_addresses[0x18], event_flags[0x94];
    u8 entry_step[4], entry_substep[4], entry_args[0x14], entry_args2[0x14];
    u8 clock[0x14], estate[0xcfc], player[0x3fc], field[0x2328], greenhouse[0x188];
    u8 requests[0x40], run_addresses[0x74], helpers[7][0xf8], world[0x4c];
    u8 pad60d4[0x6200 - 0x60d4], days[0x108], drift[0x24], family[0x34], ending[0x18];
} hm_save_wire;
_Static_assert(sizeof(hm_save_wire) == HM_SAVE_IMAGE_BYTES, "save image wire extent");
_Static_assert(offsetof(hm_save_wire, flags) == 0x225c, "save flag offset");
_Static_assert(offsetof(hm_save_wire, player) == 0x3060, "save player offset");
_Static_assert(offsetof(hm_save_wire, field) == 0x345c, "save field offset");
_Static_assert(offsetof(hm_save_wire, helpers) == 0x59c0, "save helper offset");
_Static_assert(offsetof(hm_save_wire, days) == 0x6200, "save day offset");
_Static_assert(offsetof(FarmActor, sprite) == 0x78, "pointer-free actor prefix");
_Static_assert(offsetof(AnimState, clutOffY) + sizeof(s16) == 20, "animation scalar prefix");
_Static_assert(sizeof(FarmClock) == 0x14 && sizeof(FarmEstate) == 0xcfc, "progress scalar layout");
_Static_assert(sizeof(((FarmState *)0)->fieldTiles) == 0x2328, "field scalar layout");
_Static_assert(sizeof(((FarmState *)0)->slots7x7) == 0x188, "greenhouse scalar layout");
_Static_assert(sizeof(FarmRequests) == 0x40 && sizeof(FarmWorld) == 0x4c,
               "request/world scalar layout");
_Static_assert(sizeof(FarmDrift) == 0x24 && sizeof(FarmFamily) == 0x34, "late scalar layout");
_Static_assert(sizeof(FarmEnding) == 0x18 && sizeof(FarmEstateRecord) == 24, "ending/run layout");
_Static_assert(sizeof(FarmPond) == 398, "pond wire size");
_Static_assert(offsetof(FarmPlayer, toolExp) + sizeof(((FarmPlayer *)0)->toolExp) == 74,
               "player scalar prefix");
_Static_assert(offsetof(FarmPlayer, slowTrail) + sizeof(((FarmPlayer *)0)->slowTrail) -
                       offsetof(FarmPlayer, targetFlags) ==
                   524,
               "player scalar middle");
_Static_assert(offsetof(FarmPet, projectCoords) + sizeof(((FarmPet *)0)->projectCoords) -
                       offsetof(FarmPet, training) ==
                   68,
               "pet scalar tail");
_Static_assert(offsetof(FarmHelper, pairC4) + sizeof(FarmInitPair) - offsetof(FarmHelper, number) ==
                   28,
               "helper scalar middle");
_Static_assert(sizeof(((FarmPlayer *)0)->poses) == 40, "player pose span");
_Static_assert(offsetof(FarmPlayer, cue) + sizeof(ItemCue) - offsetof(FarmPlayer, wedding) == 16,
               "player wedding/cue span");
_Static_assert(sizeof(((FarmAnimals *)0)->slots) == 16, "mode slot span");
_Static_assert(offsetof(FarmAnimals, pendingTrack) + sizeof(s16) -
                       offsetof(FarmAnimals, spawnGate) ==
                   16,
               "animal scalar tail");
_Static_assert(sizeof(((FarmState *)0)->days) == 0x108, "day count span");
static void hm_span(u8 *wire, void *live, size_t bytes, int decode) {
    if (decode)
        memcpy(live, wire, bytes);
    else
        memcpy(wire, live, bytes);
}
static void hm_anim(u8 *wire, AnimState *live, int decode) {
    hm_span(wire, &live->status, 20, decode);
    /* Retain/rebind owned native assets on load; ignore all saved PS1 addresses. */
}
static void hm_actor(u8 *wire, FarmActor *live, int decode) {
    hm_span(wire, live, 0x78, decode);
    hm_anim(wire + 0x78, &live->sprite, decode);
    hm_span(wire + 0xa8, &live->cue, 4, decode);
}
static void hm_chicken(u8 *wire, FarmChicken *live, int decode) {
    hm_actor(wire, &live->actor, decode);
    hm_span(wire + 0xac, &live->grazingTime, 8, decode);
}
static void hm_pet(u8 *wire, FarmPet *live, int decode) {
    hm_actor(wire, &live->actor, decode);
    hm_span(wire + 0xac, &live->training, 68, decode);
}
static void hm_animals(u8 *wire, FarmAnimals *live, int decode) {
    for (int i = 0; i < 10; ++i) {
        hm_chicken(wire + i * 0xb4, &live->chickens[i], decode);
        if (decode)
            live->chickens[i].actor.sprite.source = NULL;
    }
    for (int i = 0; i < 20; ++i) {
        hm_actor(wire + 0x708 + i * 0xb8, &live->livestock[i].actor, decode);
        hm_span(wire + 0x708 + i * 0xb8 + 0xac, &live->livestock[i].grazeCount, 12, decode);
        if (decode)
            live->livestock[i].actor.sprite.source = NULL;
    }
    hm_span(wire + 0x1568, &live->pond, 398, decode);
    hm_pet(wire + 0x16f8, &live->dog, decode);
    hm_pet(wire + 0x17e8, &live->horse, decode);
    for (int i = 0; i < 6; ++i) {
        hm_actor(wire + 0x18d8 + i * 0xb0, &live->actions[i].actor, decode);
        hm_span(wire + 0x18d8 + i * 0xb0 + 0xac, &live->actions[i].countdown, 2, decode);
    }
    /* The original packer clears all three extra chicken records. */
    if (decode)
        memset(live->extraChickens, 0, sizeof live->extraChickens);
    hm_span(wire + 0x1f14, live->slots, sizeof live->slots, decode);
    hm_span(wire + 0x1f24, &live->spawnGate, 16, decode);
}
static void hm_player(u8 *wire, FarmPlayer *live, int decode) {
    hm_span(wire, live, 74, decode);
    hm_span(wire + 84, &live->targetFlags, 524, decode);
    hm_anim(wire + 608, &live->secondSprite, decode);
    hm_anim(wire + 656, &live->sprite, decode);
    hm_span(wire + 704, &live->wedding, 16, decode);
    for (int i = 0; i < 5; ++i) {
        hm_span(wire + 720 + i * 52, live->toys[i].pad00, 4, decode);
        hm_anim(wire + 724 + i * 52, &live->toys[i].sprite, decode);
    }
    hm_span(wire + 980, live->poses, sizeof live->poses, decode);
}
int hm_save_partner_decode(hm_save_partner *partner, const void *image, size_t bytes) {
    if (!partner || !image || bytes < HM_SAVE_IMAGE_BYTES)
        return 0;
    const hm_save_wire *wire = image;
    memset(partner, 0, sizeof(*partner));
    hm_animals((u8 *)wire->animals, &partner->animals, 1);
    memcpy(&partner->estate, wire->estate, sizeof partner->estate);
    memcpy(partner->identity, wire->flags, sizeof partner->identity);
    return 1;
}
int hm_save_partner_encode_animals(void *image, size_t bytes, hm_save_partner *partner) {
    if (!partner || !image || bytes < HM_SAVE_IMAGE_BYTES)
        return 0;
    hm_save_wire *wire = image;
    memset(wire->animals, 0, sizeof wire->animals);
    hm_animals(wire->animals, &partner->animals, 0);
    return 1;
}
static void hm_helper(u8 *wire, FarmHelper *live, int decode) {
    hm_actor(wire, &live->actor, decode);
    hm_span(wire + 172, &live->number, 28, decode);
    hm_anim(wire + 200, &live->itemSprite, decode);
}
static int hm_save_ready(size_t bytes, const FarmState *state) {
    return bytes >= HM_SAVE_IMAGE_BYTES && state && state->runs.records[16] &&
           state->runs.records[18] && state->runs.counts[16] >= 10 && state->runs.counts[18] >= 2;
}
static void hm_save_fields(hm_save_wire *wire, FarmState *state, int decode) {
    hm_animals(wire->animals, &state->animals, decode);
    for (int i = 0; i < 37; ++i) {
        u8 *r = wire->records[i];
        FarmVillager *v = &state->villagers[i];
        hm_span(r, &v->birthday, 4, decode);
        hm_span(r + 4, &v->friendship, 2, decode);
        hm_span(r + 6, &v->affection, 2, decode);
        hm_span(r + 8, &v->flags, 2, decode);
        hm_span(r + 10, &v->placeFlags, 2, decode);
        hm_span(r + 12, &v->step, 2, decode);
    }
    hm_span(wire->run16, state->runs.records[16], 0xf0, decode);
    hm_span(wire->run18, state->runs.records[18], 0x30, decode);
    hm_span(wire->flags, &state->unk00, 0x18, decode);
    hm_span(wire->event_flags, state->eventFlags, 0x94, decode);
    hm_span(wire->entry_step, &state->progress.entryStep, 4, decode);
    hm_span(wire->entry_substep, &state->progress.entrySubStep, 4, decode);
    hm_span(wire->entry_args, state->progress.entryArgs, 0x14, decode);
    hm_span(wire->entry_args2, state->progress.entryArgs2, 0x14, decode);
    hm_span(wire->clock, &state->progress.clock, 0x14, decode);
    hm_span(wire->estate, &state->progress.estate, 0xcfc, decode);
    hm_player(wire->player, &state->player, decode);
    hm_span(wire->field, state->fieldTiles, 0x2328, decode);
    hm_span(wire->greenhouse, state->slots7x7, 0x188, decode);
    hm_span(wire->requests, &state->requests, 0x40, decode);
    for (int i = 0; i < 7; ++i)
        hm_helper(wire->helpers[i], &state->helpers[i], decode);
    hm_span(wire->world, &state->world, 0x4c, decode);
    hm_span(wire->days, &state->days, 0x108, decode);
    hm_span(wire->drift, &state->drift, 0x24, decode);
    hm_span(wire->family, &state->family, 0x34, decode);
    hm_span(wire->ending, &state->ending, 0x18, decode);
}
int hm_save_encode(void *image, size_t bytes, FarmState *state) {
    if (!image || !hm_save_ready(bytes, state))
        return 0;
    memset(image, 0, HM_SAVE_IMAGE_BYTES);
    hm_save_fields(image, state, 0);
    return 1;
}
int hm_save_decode(const void *image, size_t bytes, FarmState *state) {
    if (!image || !hm_save_ready(bytes, state))
        return 0;
    /* Reads use the same fixed spans; the input bytes are never modified. */
    hm_save_fields((hm_save_wire *)image, state, 1);
    return 1;
}
