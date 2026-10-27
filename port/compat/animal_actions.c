#include "actor.h"
#include "animals.h"
#include "cue.h"
#include "dog.h"
#include "event_commands.h"
#include "farm_state.h"
#include "harvest_sprites.h"
#include "picture_cells.h"
#include <stddef.h>

_Static_assert(offsetof(FarmActor, taskNo) == 0x26, "Shared scalar task prefix");
_Static_assert(offsetof(FarmActor, flags) == 0x34, "Shared scalar flags prefix");
_Static_assert(offsetof(FarmActor, speed) == 0x48, "Original payload48 is actor speed");

u16 hm_actor_helper_number(const void *target) { return ((const FarmHelper *)target)->number; }

/* The original function's local padded view places cue at PS1 offset 0xa8.
 * Native AnimState pointers grow that prefix: 0xa8 now holds partLists.
 * Use the shared native actor layout instead. See ADR-075. */
s32 farm_action_begin(struct FarmActionSlot *self) {
    FarmActor *actor = (FarmActor *)(void *)self;
    if (!item_is_field_object(actor->kind))
        return 1;
    u32 flags = actor->flags;
    if (flags & 0x100000) {
        if (event_cmd_has_room(1, 0, 0, 0) == 1) {
            dog_set_phase(&game_state.animals.dog, DOG_PHASE_IDLE_ACTION);
            actor->flags &= 0xffefffff;
        }
        return 2;
    }
    actor->flags = (flags | 0x20000) & ~0x2000;
    actor->taskNo = 0;
    actor->taskId = 0;
    actor->taskTime = 0;
    actor->interruptTime = 0;
    farm_vector_adjust(4, &actor->speed, 0);
    cue_reset(&actor->cue);
    return 0;
}
