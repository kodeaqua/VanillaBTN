# Dithering Control Enhancement

This enhancement module allows toggling PS1 hardware ordered dithering (4x4 Bayer matrix).

## Features
- **Vanilla Parity**: Defaults to enabled (`enhance.disable_dithering = false`), matching original PS1 hardware behavior.
- **Dithering bypass**: When disabled (`enhance.disable_dithering = true`), ordered dither noise is bypassed. The shared GPU scene still quantizes intermediate colors to BGR555, so this does not provide true-color gradients or eliminate color banding.
