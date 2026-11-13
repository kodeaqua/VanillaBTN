# TODO

This file tracks only active engineering work and developer acceptance that is
still required. Completed implementation milestones and their limitations are
recorded in [Work 02](docs/02-enhancements.md).

## Gameplay and visual acceptance

- [ ] **Hor+ scene coverage** — inspect scenes outside the farm for game-side
      visibility gates that still reject actors, objects, weather, or other
      world content inside the expanded horizontal view.
- [ ] **Photo-map marker bounds** — verify native photo-map navigation with the
      dog indoors and outdoors, including locations that have no map marker.

## Audio acceptance

- [ ] **Hi-Fi reverb fidelity** — compare the optional full-rate stereo reverb
      against objective hardware traces where available and perform developer
      listening checks. Existing synthetic tests establish code behavior, not
      PlayStation hardware equivalence or perceived audio quality.

## Turbo acceptance

- [ ] **Consistent turbo cadence** — toggle turbo after several minutes of
      runtime and compare the farm, Popuri's area, church, interiors, events,
      and transitions. Moving simulation must use the configured multiplier;
      places where the original game stops its clock must remain stopped.
      Confirm audio is silent during turbo and resumes cleanly afterward.

## References

- [Work 02](docs/02-enhancements.md) — completed enhancements, implementation
  notes, and known limitations.
- [Work 01](docs/01-porting.md#work-plan) — baseline port architecture and roadmap.
- [Building](docs/building.md) — Windows build and verification commands.
