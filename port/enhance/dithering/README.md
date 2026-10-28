# Dithering Control Enhancement

This enhancement module allows toggling PS1 hardware ordered dithering (4x4 Bayer matrix).

## Features
- **Vanilla Parity**: Defaults to enabled (`enhance.disable_dithering = false`), matching original PS1 hardware behavior.
- **Smooth Gradients**: When disabled (`enhance.disable_dithering = true`), ordered dither noise is bypassed, rendering clean true-color 32-bit gradients on modern high-resolution displays.
