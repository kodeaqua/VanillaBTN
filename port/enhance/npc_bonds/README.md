# NPC Relationships Page

This opt-in gameplay enhancement adds a scrollable page to the farm status menu.
It lists the 32 named adult villagers, with a small pixel head icon, the NPC's
name, and a bar from the original friendship or affection field. Friendship bars
are green; the five bachelorette affection bars are pink. Unnamed records and
the player's child are omitted.

The page is compiled with `HMBTN_ENH_NPC_BONDS` and requires both the gameplay
enhancements and pause menu. The runtime setting is `enhance.npc_bonds` and
defaults to `false`, preserving the original eight-page status menu until the
owner enables **NPC Relationships** in Settings > Gameplay.

The bar uses friendship 0-255 and affection 0-65535 without changing either game
value. Names use the original uppercase font glyphs. The icons are composed from
small native rectangles and do not load or mutate villager sprite state.

The status sprite pack contains Pete's small HUD head, but no matching roster of
NPC head icons. The disc's dialogue text banks do contain 80x80 face portraits;
the current game API maps those by dialogue face code and loads one bank into the
shared portrait texture area. It does not provide a direct NPC-ID-to-small-icon
mapping or a multi-portrait cache, so the status page does not use those faces.
