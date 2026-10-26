# Texture filtering

Optional palette-aware Bilinear selected by the existing Graphics > Filtering
row. HMBTN_ENH_FILTERING defaults OFF; Windows -Game compiles it. Runtime
 enhance.filtering defaults false. If absent, an explicit graphics.bilinear
preference migrates; explicit enhance.filtering overrides it.

sample.c provides the CPU reference; texture.hlsl supplies the shared GPU
implementation. UV Q8 sampling clamps to the primitive atlas domain before
PS1 texture-window/page and CLUT decoding. Each texel is shaded and blended
against the same destination before weighted composition. Transparent taps
retain background. The highest-weight covered tap supplies the mask bit
(first tie wins); force-mask applies only to covered pixels.

Native textures retain their original detail. Filtering works independently
of Enhance Scaling and never writes filtered pixels into native VRAM. Software
output is shared in gfx/software_raster and gfx/software_output. GPU scenes
rejected by the shared backend retain CPU output. Nearest keeps the original
branch. Primitive UV bounds can produce local filtering differences at seams;
owner gameplay validation is pending. Final window presentation also retains
its existing nearest/bilinear selection. See ADR-055 and tests/texture_filter.c.
