# Spec: Host-side texture upscaling and replacement

**Author:** VanillaBTN contributors
**Date:** 2026-11-14
**Status:** Approved
**Reviewers:** Developer (requirements supplied in the implementation request)
**Related work:** [Work 02](../02-enhancements.md)

## Context

VanillaBTN currently accepts `texture_filter = xbrz`, but the runtime maps that
value to nearest-neighbor sampling. Its high-resolution sidecar raster also
mirrors ordinary 2D primitives as enlarged source pixels, so sprites, UI and
pixel-art textures do not gain the edge reconstruction requested by the
setting. Applying one post-process to the complete framebuffer would also
stylize authored prerendered backgrounds that should remain photographic or
painted rather than pixel-art-like.

The original 1024x512 PS1 VRAM, CLUT reads and framebuffer operations are part
of the authoritative game runtime. Modern filtering must therefore operate on
decoded host pixels and the host presentation sidecar only. The repository is
MIT-licensed, while the upstream xBRZ implementation is GPLv3; this feature
uses an independently written compatible scaler and does not copy or link the
upstream implementation.

## Functional Requirements

- FR-1: `texture_filter = nearest` MUST preserve nearest-neighbor host
  presentation for textured sprites, UI and polygons.
- FR-2: `texture_filter = xbrz` MUST apply an independently implemented xBRZ
  edge-directed scale to non-background textured sprites, UI and projected
  pixel-art texture regions at host render scales greater than one.
- FR-3: The xBRZ scaler MUST support integer source scaling factors 2 through
  6; larger presentation scales MUST reuse the 6x result without modifying the
  source texture.
- FR-4: Explicitly marked 2D background primitives MUST NOT use xBRZ and MUST
  use the configured `background_filter` (`bilinear` or `bicubic`) in modern
  presentation.
- FR-5: Background classification MUST use existing explicit primitive
  provenance and MUST NOT infer a background from primitive dimensions.
- FR-6: `background_filter = nearest` MUST remain available for an authentic
  pixel presentation.
- FR-7: Upscaled and replacement pixels MUST be stored only in bounded
  host-side caches or presentation buffers and MUST NOT overwrite PS1 VRAM,
  CLUT data or imported assets.
- FR-8: `texture_replacements = on` MUST optionally look for host replacement
  images under a configured repository-relative or executable-relative pack
  directory; the default MUST be off.
- FR-9: Replacement lookup MUST use a deterministic content key derived from
  decoded source ARGB pixels and source dimensions, so packs do not depend on
  VRAM addresses or scene-specific allocation order.
- FR-10: A valid replacement MUST override xBRZ for the matching source region,
  while an absent, unreadable or invalid replacement MUST fall back to the
  selected source filter without changing game state.
- FR-11: Texture and replacement caches MUST be invalidated by their decoded
  pixel content, so VRAM uploads and CLUT animation cannot return stale pixels.
- FR-12: The authentic PS1 preset MUST resolve both texture and background
  filtering to nearest and MUST disable replacements unless explicitly
  overridden in custom configuration.
- FR-13: The implementation MUST expose a deterministic content-key utility
  and document the replacement filename convention for future HD packs.

## Non-Functional Requirements

- NFR-1: Host texture cache memory MUST be bounded to 64 MiB or less and MUST
  evict old entries rather than grow without limit.
- NFR-2: A cache hit MUST perform no image-file read and no xBRZ recomputation.
- NFR-3: Release builds MUST add no per-pixel diagnostic logging.
- NFR-4: Native Debug and Release builds MUST pass their registered CTest
  suites, and the unchanged PS1 source MUST pass the 14-file SHA-1 code check.
- NFR-5: The implementation MUST add no dependency whose license requires the
  MIT application to be distributed under a copyleft license.

## Acceptance Criteria

### AC-1: Authentic nearest presentation (FR-1, FR-6, FR-12)
Given a synthetic two-color sprite and the authentic preset
When it is rendered at a host scale greater than one
Then every source texel is copied as a uniform nearest-neighbor block
And texture replacement lookup is disabled.

### AC-2: xBRZ edge reconstruction (FR-2, FR-3)
Given a synthetic diagonal pixel-art edge with transparent surroundings
When it is rendered with `texture_filter = xbrz` at factors 2 through 6
Then the host output contains edge-blended subpixels that differ from nearest
And transparent pixels do not introduce black halos.

### AC-3: Background filter separation (FR-4, FR-5, FR-6)
Given identical textured primitives with and without explicit background provenance
When xBRZ is selected and `background_filter = bicubic`
Then only the non-background primitive uses xBRZ
And the marked background uses bicubic sampling
And changing its dimensions alone does not change classification.

### AC-4: VRAM remains authoritative (FR-7)
Given a snapshot of VRAM and CLUT words before host upscaling
When xBRZ, bicubic and a replacement image are each presented
Then all source VRAM and CLUT words equal the snapshot except for writes made
by the original PS1 primitive raster path.

### AC-5: Deterministic replacement hit (FR-8, FR-9, FR-10, FR-13)
Given two identical decoded source regions at different VRAM addresses and a
valid replacement file named with their documented content key
When replacements are enabled
Then both regions select the same replacement pixels.

### AC-6: Safe replacement fallback (FR-10)
Given a missing, unreadable, wrong-sized or malformed replacement file
When the matching source region is presented
Then rendering falls back to the selected source filter
And the draw operation succeeds without modifying the source pixels.

### AC-7: Content-aware cache invalidation (FR-11, NFR-2)
Given a cached indexed texture region
When either an indexed texel or its CLUT color changes
Then the next lookup produces a different content key and refreshed output
And a subsequent unchanged lookup reuses the refreshed cache entry.

### AC-8: Bounded cache (NFR-1)
Given unique textures whose scaled pixels exceed the configured cache budget
When they are inserted sequentially
Then the cache remains at or below 64 MiB
And at least one least-recently-used entry is evicted.

### AC-9: Build separation (NFR-3, NFR-4, NFR-5)
Given the completed implementation
When native Debug, native Release and the PS1 code check are run
Then both native CTest suites pass
And all 14 PS1 code hashes match
And no GPL implementation is compiled or linked.

## Edge Cases

- EC-1: Render scale is 1 -> Keep the authoritative 1x raster; do not allocate
  or run an upscaler.
- EC-2: Render scale exceeds 6 -> Produce a 6x xBRZ cache entry and resample it
  to the requested host scale without touching source pixels.
- EC-3: Source rectangle crosses a 256-coordinate texture wrap -> Decode with
  the same wrapped PS1 texture addressing used by the original sampler.
- EC-4: Source includes transparent index/color zero -> Preserve zero alpha
  during filtering and exclude transparent black from color interpolation.
- EC-5: Cache allocation fails -> Draw with the existing nearest host mirror
  and retain a successful original PS1 draw.
- EC-6: Replacement directory is absent or not a directory -> Disable pack
  lookup for that request and use the selected source filter.
- EC-7: Replacement dimensions are zero, smaller than the source, non-integral
  relative to it, or exceed 16x -> Reject it and use the selected source filter.
- EC-8: Two source regions have the same content key -> Treat them as the same
  texture content; no scene or VRAM address may be part of the key.
- EC-9: A marked background is semitransparent -> Filter its source colors but
  preserve the existing PS1 blend ordering and blend mode.

## API Contracts

```c
typedef enum Ps1TextureFilterMode {
    PS1_FILTER_NEAREST,
    PS1_FILTER_BILINEAR,
    PS1_FILTER_XBRZ
} Ps1TextureFilterMode;

typedef enum Ps1BackgroundFilterMode {
    PS1_BACKGROUND_NEAREST,
    PS1_BACKGROUND_BILINEAR,
    PS1_BACKGROUND_BICUBIC
} Ps1BackgroundFilterMode;

typedef bool (*Ps1TextureReplacementProvider)(
    uint64_t content_key,
    int source_width,
    int source_height,
    Ps1ReplacementImage *out_image,
    void *userdata);

uint64_t ps1_texture_content_key(
    const uint32_t *argb, int width, int height, size_t stride);

void ps1_video_set_texture_filter(Ps1TextureFilterMode mode);
void ps1_video_set_background_filter(Ps1BackgroundFilterMode mode);
void ps1_video_set_texture_replacement_provider(
    Ps1TextureReplacementProvider provider, void *userdata);
```

The replacement provider returns `false` for absent or invalid content. On
success it returns immutable ARGB8888 pixels and dimensions; ownership and the
release callback are explicit in `Ps1ReplacementImage`. No network or archive
lookup occurs in the renderer.

## Data Models

### Host texture cache key

| Field | Type | Constraints |
| --- | --- | --- |
| content_key | uint64 | Hash of dimensions and decoded ARGB pixels |
| source_width | uint16 | 1..256 |
| source_height | uint16 | 1..256 |
| scale | uint8 | 2..6 for xBRZ; replacement-defined otherwise |
| filter | enum | xBRZ, bilinear, bicubic or replacement |

### Host texture cache entry

| Field | Type | Constraints |
| --- | --- | --- |
| key | HostTextureCacheKey | Immutable while cached |
| pixels | ARGB8888 array | Host allocation; never aliases VRAM |
| width | int | Positive and allocation-checked |
| height | int | Positive and allocation-checked |
| byte_size | size_t | Included in the 64 MiB limit |
| last_use | uint64 | Monotonic LRU stamp |

### Replacement pack file

| Field | Type | Constraints |
| --- | --- | --- |
| filename | string | `<16-hex-key>_<width>x<height>.bmp` |
| pixels | ARGB8888 | Loaded by SDL on the host only |
| scale | integer | 1..16 and integral in both dimensions |

## Out of Scope

- OS-1: Shipping copyrighted game textures or generated HD packs; users must
  supply their own legal replacement content.
- OS-2: Modifying, repacking or persisting replacements into PS1 VRAM, disc
  images or imported game data.
- OS-3: Automatically classifying large sprites as backgrounds; explicit
  provenance is required to avoid incorrect filtering.
- OS-4: AI-generated textures, offline asset extraction or batch upscaling.
- OS-5: Proving subjective visual quality or gameplay fidelity; the developer
  performs hands-on comparison after synthetic checks pass.
- OS-6: Copying or linking the GPLv3 upstream xBRZ source into VanillaBTN.
