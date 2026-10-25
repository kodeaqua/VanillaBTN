/* Preserve the original nested helper's captured `object` explicitly. */
static void hm_harvest_helper_arrive_or_wait(FarmHelper *object, FarmHelper *o, s16 kind,
                                             u32 mask) {
    if (o->actor.place.location == kind) {
        o->targetAnimal = SLOT_NONE;
        o->actor.taskId = 0;
        o->actor.flags &= ~mask;
        o->flags |= HELPER_BIT9;
    } else {
        o->actor.flags |= mask;
        harvest_obj_wait_walk(object, HARVEST_NEAR_PROXIMITY);
    }
}
#define harvest_helper_arrive_or_wait(o, kind, mask)                                               \
    hm_harvest_helper_arrive_or_wait(object, (o), (kind), (mask))
