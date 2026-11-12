/*
 * Link-only stubs for event_request.c's unrelated game services.
 *
 * event_request_claim_test exercises only request-slot allocation. The production
 * translation unit also contains timer, event-dispatch, and stale-area handlers;
 * keep those unrelated dependencies local to this test instead of linking the
 * complete game runtime (or relying on MSVC's dead-code elimination behavior).
 */
#include "types.h"
#include "farm_state.h"
#include "clock.h"
#include "event_commands.h"
#include "event_flags.h"
#include "field.h"
#include "shop_dialogues.h"

/* event_request.c references this global from its stale-request path. */
FarmLoadArea farm_area;

/* Timer helpers are not exercised by the slot-allocation test. */
s32 clock_date_index(s16 season, s16 day)
{
    (void)season;
    (void)day;
    return 0;
}

enum WeddingOfferState wedding_offer_state(s16 mode)
{
    (void)mode;
    return WEDDING_OFFER_NO;
}

/* Event-step helpers are link-only for this focused test. */
s32 event_flag_get(u16 flag)
{
    (void)flag;
    return 0;
}

void event_flag_set(u16 flag, u16 on)
{
    (void)flag;
    (void)on;
}

s32 shop_dialogue_current(void)
{
    return 0;
}

s32 (*const event_request_checks[])(void) = { 0 };
