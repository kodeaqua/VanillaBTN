# PGXP Subpixel Precision and Perspective-Correct Textures

Owner-requested enhancement implementing subpixel vertex precision to eliminate
PS1 polygon wobble (vertex snap) and perspective-correct texture mapping to eliminate
affine texture warping.

Build with `HMBTN_ENH_PGXP=ON` (compiled by default in `build.ps1 -Game`).
Runtime default is **OFF** (`enhance.pgxp = false`), preserving authentic vanilla PS1 rendering.
When enabled via ESC > Settings > Graphics > PGXP Geometry > On, high-precision
vertex coordinates and perspective depth weights are computed and applied to 3D geometry.
