# Work NN: Title

Copy this file to `NN-short-name.md`, replace every placeholder, and delete
this instruction before starting a new major work item. The developer decides
the work's vision and whether it changes `VERSION_MAJOR`.

## Status

State whether the work is proposed, active, blocked, or complete. Link its
branch or pull request when one exists and point to `TODO.md` for active tasks.

## Vision

Describe the intended outcome and default user experience. Explain how this
work follows the previous numbered work and what developer decision authorizes
it.

## Scope and boundaries

- State what is included.
- State what is deliberately excluded.
- Record compatibility, fidelity, data, licensing, and ownership constraints.

## Architecture and approach

Describe the important system boundaries and technical choices. Prefer a small
table when several components have distinct responsibilities.

## Work plan

| Order | Work | Agent verification | Developer acceptance |
| --- | --- | --- | --- |
| **1** | First independently useful milestone | Repeatable code or synthetic check | Hands-on behavior to validate |

Keep open task details in `TODO.md`; this section records the stable sequence
and completion criteria.

## Verification

Record the checks that ran and their scope. Separate compilation and synthetic
evidence from gameplay, visual, audio, or hardware acceptance.

## Implementation notes

Document non-obvious behavior, limitations, audit findings, and decisions that
must remain understandable after the work is complete.
