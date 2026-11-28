# PsyZ Windows fragment shader

The game build uses `psx_frag.dxil` with the pinned PsyZ `00fb42e` renderer.
The companion patch truncates the quarter foreground in RGB555 for blend mode 3.
See [NOTICE](../../NOTICE) for the PsyZ MPL 2.0 terms.

To regenerate the blob in a disposable PsyZ checkout, apply
`quarter_blend.patch` at the PsyZ repository root. Run the fragment steps
from PsyZ's `psyz/src/platform/shaders/build_shaders.sh` in the shader directory.

```sh
glslang -V --target-env vulkan1.0 -S frag psx.frag.glsl -o psx.frag.spv
spirv-cross --hlsl --shader-model 60 psx.frag.spv --output psx.frag.hlsl
dxc -T ps_6_0 -E main psx.frag.hlsl -Fo psx_frag.dxil
```

The checked-in artifact uses glslang 16.6.0, SPIRV-Cross
`vulkan-sdk-1.4.363.0`, and DXC 1.9.2602.17. Copy the DXIL here and run
`ctest -R vanillabtn_gpu_pixels --output-on-failure` in the native game build.
