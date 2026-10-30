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
value. Names use normal title case and the original lowercase font glyphs. Each
icon is a cropped, downsampled portrait from the matching NPC's original face
bank in the user's `face.bin`; visible portraits load progressively while the
page is open. The portrait-bank mapping follows the original Character ID order.
The page heading and bar labels use title case as well. Game image data is read
at runtime and is not copied into the repository or release package.
