# Visual interpolation

Owner-requested experimental screen-space primitive interpolation, not image
crossfading or additional simulation. Build HMBTN_ENH_INTERPOLATION=ON (OFF by
default); Windows build.ps1 -Game enables compilation. Runtime defaults OFF.
Choose ESC > Settings > Graphics > Interpolation > On and a higher FPS Limit.
The boolean persists as enhance.interpolation in Documents/BackToNature/settings.json.

Read-only frame observers capture native GPU commands, the pre-draw environment
and VRAM. Two frame histories associate world primitives by arena-relative
packet address, unique occurrence, opcode/material/layout and bounded motion.
The presentation path interpolates signed screen coordinates and replays the
current command list into private VRAM and GPU state. It then restores the
native contexts. Current texture/animation poses are retained; images are not
blended. Native packets, masks, dirty tiles, GTE, RNG and tick state are untouched.
Secondary UI ordering tables retain current positions. Allocation occurs only
when enabling the option; buffers are bounded and overflow falls back to native.

The initial eligible context is the normal farm handler/view, with resident
area data, no loading, transition, pause or menu flags. Title, minigames,
ineligible contexts, missing history and irregular frame intervals use native
scanout. Overlay changes invalidate history. Matching rejects duplicate packet
addresses, changed materials/topology and displacement above 12 PS1 pixels per
coordinate per frame. Unmatched primitives keep current coordinates; this can
produce mixed cadence when command allocation changes. This heuristic does not
provide persistent actor identities or world-space/camera interpolation.

Normal logic remains 30 Hz via original VSync(2), with fixed 60 Hz hardware ticks.
Presentation uses the existing back-buffer lead to interpolate previous to
current geometry over two hardware ticks, clamped without extrapolation. It
scans the private current draw rectangle rather than native front VRAM. Sprite
animation, texture samples and position quantization remain at native fidelity;
high-resolution rasterization is a separate pending enhancement. UI is drawn
with current native commands, without text blending. Pause snapshots can read
the private render result without advancing simulation.

Synthetic tests check midpoint geometry without double images, current UI,
double-buffer relocation, material changes, large jumps, history invalidation,
irregular ticks and exact preservation of native VRAM/environment/commands/ticks.
These tests do not establish gameplay smoothness, complete primitive identity,
full-playthrough parity or achieved FPS. Owner gameplay/device testing remains
necessary, particularly walking, camera scroll, actor animation and transitions.

The subsequent ground-seam regression adds stable cell identities for field
sprites and connected-polygon coherence (ADR-053). A sprite's work allocation
can move when another sprite is inserted; identity follows its world cell and
packet part instead. Shared vertices must agree about their previous position;
otherwise the entire connected mesh uses current geometry. This prevents the
tested holes caused by mixing current and intermediate neighboring positions.
It can reduce interpolation cadence when history changes. Actual farm gameplay
confirmation remains with the owner.
