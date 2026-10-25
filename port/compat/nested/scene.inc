/* Native copy of the repository's CC0 nested helper. No algorithm changes. */
static s32 scene_slot_handoff_step(FarmSceneModel *owner, FarmSceneModel *other, s32 nextStage) {
    FarmMorph *counter;

    owner->morph->step++;
    counter = owner->morph;
    if (counter->step < counter->steps) {
        return 0;
    }
    other->morph->step = 0;
    counter->step = 0;
    owner->flags |= SCENE_ENTRY_HIDDEN;
    other->flags &= ~SCENE_ENTRY_HIDDEN;
    scene_flag_stage = nextStage;
    return 1;
}
