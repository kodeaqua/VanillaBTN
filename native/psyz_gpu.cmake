# Compile PsyZ with the Windows renderer fixes without editing the checkout.
set(psyz_platform "${VANILLABTN_PSYZ_SOURCE_DIR}/psyz/src/platform")
set(psyz_gpu_source "${psyz_platform}/sdl3_gpu.c")
set(psyz_gpu_common "${psyz_platform}/sdl3_common.h")
set(psyz_gpu_draw "${psyz_platform}/sdl3_draw.h")
set(psyz_upstream_shader "${psyz_platform}/shaders/psx_frag_dxil.h")
set(psyz_patched_shader "${CMAKE_CURRENT_LIST_DIR}/shaders/psx_frag.dxil")
set(psyz_gpu_build "${CMAKE_CURRENT_BINARY_DIR}/psyz-win-gpu")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${psyz_gpu_source}" "${psyz_gpu_common}" "${psyz_gpu_draw}"
    "${psyz_upstream_shader}" "${psyz_patched_shader}")

function(patch_psyz_renderer source expected replacement description)
    string(FIND "${${source}}" "${expected}" first_match)
    string(FIND "${${source}}" "${expected}" last_match REVERSE)
    if(first_match LESS 0 OR NOT first_match EQUAL last_match)
        message(FATAL_ERROR "Expected one PsyZ patch location for ${description}.")
    endif()
    string(REPLACE "${expected}" "${replacement}" patched "${${source}}")
    set(${source} "${patched}" PARENT_SCOPE)
endfunction()

function(patch_psyz_renderer_section source begin end replacement description)
    string(FIND "${${source}}" "${begin}" first)
    string(FIND "${${source}}" "${end}" last)
    if(first LESS 0 OR last LESS_EQUAL first)
        message(FATAL_ERROR "Could not locate PsyZ section for ${description}.")
    endif()
    math(EXPR length "${last} - ${first}")
    string(SUBSTRING "${${source}}" ${first} ${length} section)
    patch_psyz_renderer(${source} "${section}" "${replacement}" "${description}")
    set(${source} "${${source}}" PARENT_SCOPE)
endfunction()

file(READ "${psyz_upstream_shader}" upstream_shader)
string(REPLACE "\r\n" "\n" upstream_shader "${upstream_shader}")
string(SHA256 upstream_shader_hash "${upstream_shader}")
if(NOT upstream_shader_hash STREQUAL "9b43a002d7268da9333226b2a78dad3ea856ac8350a3bace23b0134262eec1eb")
    message(FATAL_ERROR "The patched pixel shader requires the PsyZ 00fb42e Windows shader.")
endif()

file(READ "${psyz_patched_shader}" shader_hex HEX)
string(REGEX REPLACE "(..)" "0x\\1," shader_bytes "${shader_hex}")
file(CONFIGURE OUTPUT "${psyz_gpu_build}/vanillabtn_psx_frag_dxil.h" CONTENT [=[
static const unsigned char psx_frag_dxil[] = { @shader_bytes@ };
#define psx_frag_dxil_len (sizeof(psx_frag_dxil))
]=] @ONLY)

# Test the part of the texture or palette that continues on the left of VRAM.
file(READ "${psyz_gpu_common}" gpu_common)
string(REPLACE "\r\n" "\n" gpu_common "${gpu_common}")
set(texture_overlap [=[    return OverlapsDirtyCanvas(page, pending) ||
           (bpp < 2 && OverlapsDirtyCanvas(pal, pending));]=])
set(wrapped_overlap [=[    if (OverlapsDirtyCanvas(page, pending) ||
        (bpp < 2 && OverlapsDirtyCanvas(pal, pending))) {
        return true;
    }
    page.x -= VRAM_W;
    pal.x -= VRAM_W;
    return (page.x + page.w > 0 && OverlapsDirtyCanvas(page, pending)) ||
           (bpp < 2 && pal.x + pal.w > 0 && OverlapsDirtyCanvas(pal, pending));]=])
patch_psyz_renderer(gpu_common "${texture_overlap}" "${wrapped_overlap}"
    "wrapped texture dependencies")

# Raw textured polygons copy RGB555 texels without shading or dithering.
patch_psyz_renderer(gpu_common "(isGouraud || (isTextured && isShadeTex))"
    "(isTextured ? isShadeTex : isGouraud)" "raw texture dithering")
file(CONFIGURE OUTPUT "${psyz_gpu_build}/vanillabtn_sdl3_common.h"
    CONTENT "#line 1 \"psyz/src/platform/sdl3_common.h\"\n${gpu_common}" @ONLY)

file(READ "${psyz_gpu_source}" gpu_source)
string(REPLACE "\r\n" "\n" gpu_source "${gpu_source}")
patch_psyz_renderer(gpu_source "#include \"shaders/psx_frag_dxil.h\""
    "#include \"vanillabtn_psx_frag_dxil.h\"" "the pixel shader include")
patch_psyz_renderer(gpu_source "#include \"sdl3_common.h\""
    "#include \"vanillabtn_sdl3_common.h\"" "the common header include")

# Extend axis-aligned draws past the larger coordinate to cover both endpoints.
set(line_vertex [=[                Vertex* q = vertex_cur;
                unsigned short base = n_vertices;]=])
set(line_vertex_with_endpoints [=[                if (dy == 0 && ex < 0) {
                    ++x0;
                    ex = 0;
                } else if (dx == 0 && ey < 0) {
                    ++y0;
                    ey = 0;
                }

                Vertex* q = vertex_cur;
                unsigned short base = n_vertices;]=])
patch_psyz_renderer(gpu_source "${line_vertex}" "${line_vertex_with_endpoints}"
    "axis-aligned line endpoints")

# Reject lines beyond the PS1 span before expanding them into geometry.
set(line_axis [=[                if ((dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy)) {]=])
set(bounded_line_axis [=[                enum { MAX_LINE_WIDTH = 1023, MAX_LINE_HEIGHT = 511 };
                const int width = abs(dx);
                const int height = abs(dy);
                if (width > MAX_LINE_WIDTH || height > MAX_LINE_HEIGHT) {
                    continue;
                }
                if (width >= height) {]=])
patch_psyz_renderer(gpu_source "${line_axis}" "${bounded_line_axis}" "line size limits")

# A one-pixel line uses the first endpoint's color.
set(line_end_color [=[                q[1].r = cr[s + 1];
                q[1].g = cg[s + 1];
                q[1].b = cb[s + 1];
                q[1].a = ca[s + 1];
                q[2].r = cr[s + 1];
                q[2].g = cg[s + 1];
                q[2].b = cb[s + 1];
                q[2].a = ca[s + 1];]=])
string(REPLACE "[s + 1]" "[end_color]" point_line_color "${line_end_color}")
patch_psyz_renderer(gpu_source "${line_end_color}"
    "                const int end_color = (dx != 0 || dy != 0) ? s + 1 : s;\n${point_line_color}"
    "one-pixel line colors")

# Polygon TPage updates the page, blend mode and depth without changing dither.
set(polygon_tpage [=[                tpage = cur_tpage | TPAGE_NOTEXTURE;
            }
            tpage |= TPAGE_POLYGON;]=])
set(polygon_tpage_latch [=[                tpage = cur_tpage | TPAGE_NOTEXTURE;
            } else {
                enum { TPAGE_RENDER_MASK = 0x1ff };
                tpage &= TPAGE_RENDER_MASK;
                cur_tpage = tpage;
            }
            tpage |= TPAGE_POLYGON;]=])
patch_psyz_renderer(gpu_source "${polygon_tpage}" "${polygon_tpage_latch}"
    "polygon texture page state")

# A VRAM fill must execute after the primitives already queued before it.
set(clear_image [=[    r = To15BitChannel(r);
    g = To15BitChannel(g);
    b = To15BitChannel(b);]=])
patch_psyz_renderer(gpu_source "${clear_image}" "    Draw_FlushBuffer();\n${clear_image}"
    "VRAM fill order")

# The fragment shader writes the final RGB555 color with fixed-function blending disabled.
file(READ "${psyz_gpu_draw}" gpu_draw)
string(REPLACE "\r\n" "\n" gpu_draw "${gpu_draw}")
patch_psyz_renderer_section(gpu_draw "static inline bool is_subtract_abr(" "void Draw_SetTexpageMode("
    "" "obsolete subtract grouping")
patch_psyz_renderer(gpu_draw [=[void Draw_SetMask(int bit0, int bit1) {
    if (bit0 || bit1) {
        NOT_IMPLEMENTED;
    }
}]=] [=[enum { VRAM_MASK_SET = 1, VRAM_MASK_CHECK = 2 };
static Uint32 vram_mask_flags;

void Draw_SetMask(int bit0, int bit1) {
    const Uint32 flags = (bit0 ? VRAM_MASK_SET : 0) | (bit1 ? VRAM_MASK_CHECK : 0);
    if (flags != vram_mask_flags) {
        Draw_FlushBuffer();
        vram_mask_flags = flags;
    }
}]=] "packet-ordered VRAM mask flags")
file(CONFIGURE OUTPUT "${psyz_gpu_build}/vanillabtn_sdl3_draw.h"
    CONTENT "#line 1 \"psyz/src/platform/sdl3_draw.h\"\n${gpu_draw}" @ONLY)
patch_psyz_renderer(gpu_source "#include \"sdl3_draw.h\""
    "#include \"vanillabtn_sdl3_draw.h\"" "the draw header include")
patch_psyz_renderer(gpu_source "static SDL_GPUGraphicsPipeline* pipe_tri[BLEND_COUNT];"
    [=[static SDL_GPUGraphicsPipeline* pipe_tri;
static SDL_GPUTexture* blend_background;
static PsyzSize blend_background_size;]=] "blend resources")
patch_psyz_renderer(gpu_source "SDL_GPUShader* vs, SDL_GPUShader* fs, BlendMode mode) {"
    "SDL_GPUShader* vs, SDL_GPUShader* fs) {" "the triangle pipeline signature")
patch_psyz_renderer_section(gpu_source "    // the shader outputs alpha 0 on B-F texels"
    "    const SDL_GPUColorTargetDescription target = {" "" "fixed-function blend states")
patch_psyz_renderer(gpu_source "        .blend_state = blend[mode],\n" "" "the triangle blend state")
patch_psyz_renderer(gpu_source "psx_frag, psx_frag_len, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);"
    "psx_frag, psx_frag_len, SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 1);" "background and mask inputs")
patch_psyz_renderer(gpu_source "void Draw_Reset() { NOT_IMPLEMENTED; }"
    "void Draw_Reset() { Draw_SetMask(0, 0); NOT_IMPLEMENTED; }" "mask reset")
patch_psyz_renderer(gpu_source "void ResetPlatform(void) {\n    cur_tpage = 0;"
    "void ResetPlatform(void) {\n    vram_mask_flags = 0;\n    cur_tpage = 0;" "device mask reset")
patch_psyz_renderer_section(gpu_source "        for (int i = 0; i < BLEND_COUNT; i++) {"
    "        pipe_clear = CreateClearPipeline" "        pipe_tri = CreatePsxPipeline(psx_vs, psx_fs);\n"
    "the triangle pipeline allocation")
patch_psyz_renderer(gpu_source [=[    for (int i = 0; i < BLEND_COUNT; i++) {
        shaders_ok = shaders_ok && pipe_tri[i];
    }
    return shaders_ok && pipe_clear;]=] "    return shaders_ok && pipe_tri && pipe_clear;"
    "the triangle pipeline initialization check")
patch_psyz_renderer(gpu_source [=[        for (int i = 0; i < BLEND_COUNT; i++) {
            if (pipe_tri[i]) {
                SDL_ReleaseGPUGraphicsPipeline(device, pipe_tri[i]);
                pipe_tri[i] = NULL;
            }
        }]=] [=[        if (pipe_tri) {
            SDL_ReleaseGPUGraphicsPipeline(device, pipe_tri);
            pipe_tri = NULL;
        }
        if (blend_background) {
            SDL_ReleaseGPUTexture(device, blend_background);
            blend_background = NULL;
        }]=] "triangle and background resource cleanup")

# Masked transfers keep the direct copy paths when both GP0(E6) flags are clear.
patch_psyz_renderer(gpu_source "// places the image in tex_upload_transfer on the canvas, past the VRAM edges"
    "#include \"psyz_gpu_blend.h\"\n\n// places the image in tex_upload_transfer on the canvas, past the VRAM edges"
    "shared GPU draw and transfer helpers")
patch_psyz_renderer(gpu_source [=[    if (canvas_res == 1) {
        UploadImage(canvas_tex[fb_index], dst.x, dst.y, rect->w, dst.x - img.x,]=]
    [=[    if (vram_mask_flags) {
        UploadMaskedImage(canvas_tex[fb_index], canvas_size, dst, (int)canvas_res, rect->w,
                          dst.x - img.x, dst.y - img.y);
        return;
    }
    if (canvas_res == 1) {
        UploadImage(canvas_tex[fb_index], dst.x, dst.y, rect->w, dst.x - img.x,]=]
    "masked canvas uploads")
patch_psyz_renderer(gpu_source [=[    UploadImage(vram_render, part.x, part.y, rect->w, part.x - rect->x,
                part.y - rect->y, part.w, part.h);]=] [=[    const SDL_Point offset = {part.x - rect->x, part.y - rect->y};
    if (!UploadVramRegion(part, rect->w, offset)) {
        return;
    }]=] "masked VRAM uploads")
patch_psyz_renderer(gpu_source [=[    SDL_Rect clamped;
    if (!layout.native) {
        LeaveNative();
        if (!SDL_GetRectIntersection(&area, &vram, &clamped) ||
            !SDL_RectsEqual(&area, &clamped)) {]=] [=[    SDL_Rect clamped;
    const bool wrapped = ValidVramTransfer(area) &&
                         (area.x + area.w > VRAM_W || area.y + area.h > VRAM_H);
    if (!wrapped && layout.native && (vram_mask_flags & VRAM_MASK_CHECK)) {
        SyncDirtyCanvases(&area);
    }
    if (!layout.native) {
        LeaveNative();
        if (!wrapped && (!SDL_GetRectIntersection(&area, &vram, &clamped) ||
                         !SDL_RectsEqual(&area, &clamped))) {]=] "detect wrapped uploads")
patch_psyz_renderer(gpu_source "    // an image touching the framebuffer being drawn only writes VRAM inside"
    [=[    if (wrapped) {
        UploadWrappedVram(&layout, area);
        return;
    }

    // an image touching the framebuffer being drawn only writes VRAM inside]=]
    "wrapped physical uploads")

patch_psyz_renderer(gpu_source [=[    if (rect->x < 0 || rect->y < 0 || rect->w < 0 || rect->h < 0 ||
        rect->x + rect->w > VRAM_W || rect->y + rect->h > VRAM_H) {]=]
    [=[    const SDL_Rect area = {rect->x, rect->y, rect->w, rect->h};
    if (!ValidVramTransfer(area)) {]=] "validate wrapped downloads")
patch_psyz_renderer(gpu_source [=[    SyncDirtyCanvases(&(SDL_Rect){rect->x, rect->y, rect->w, rect->h});
    SDL_GPUCommandBuffer* cmd = AcquireCmd();]=] [=[    VramPart parts[MAX_VRAM_PARTS];
    const int count = SplitVramRect(area, parts);
    for (int i = 0; i < count; ++i) {
        SyncDirtyCanvases(&parts[i].area);
    }
    SDL_GPUCommandBuffer* cmd = AcquireCmd();]=] "flush each wrapped download area")
patch_psyz_renderer(gpu_source [=[    const SDL_GPUTextureRegion region = {
        .texture = vram_render,
        .x = (Uint32)rect->x,
        .y = (Uint32)rect->y,
        .w = (Uint32)rect->w,]=] [=[    SDL_GPUTexture* texture = vram_render;
    SDL_Rect read = parts[0].area;
    if (count > 1) {
        read = StageVramParts(copy, parts, count, area);
        texture = vram_sample;
    }
    const SDL_GPUTextureRegion region = {
        .texture = texture,
        .x = (Uint32)read.x,
        .y = (Uint32)read.y,
        .w = (Uint32)rect->w,]=] "pack wrapped downloads in row order")

patch_psyz_renderer_section(gpu_source
    "void Draw_MoveImage(PS1_RECT* rect, unsigned int x, unsigned int y) {"
    "void Draw_ResetBuffer(void) {"
    [=[void Draw_MoveImage(PS1_RECT* rect, unsigned int x, unsigned int y) {
    SDL_Rect source = {rect->x, rect->y, rect->w, rect->h};
    const int dst_x = CLAMP((int)x, 0, VRAM_W - 1);
    const int dst_y = CLAMP((int)y, 0, VRAM_H - 1);
    if (!ValidVramTransfer(source) || x >= VRAM_W || y >= VRAM_H) {
        source.x = CLAMP(source.x, 0, VRAM_W - 1);
        source.y = CLAMP(source.y, 0, VRAM_H - 1);
        source.w = CLAMP(source.w, 0, SDL_min(VRAM_W - source.x, VRAM_W - dst_x));
        source.h = CLAMP(source.h, 0, SDL_min(VRAM_H - source.y, VRAM_H - dst_y));
        if (source.w <= 0 || source.h <= 0) {
            return;
        }
        WARNF("clamping invalid VRAM copy at (%d,%d %dx%d)", rect->x, rect->y, rect->w, rect->h);
    }
    MoveVramRegion((SDL_Rect){dst_x, dst_y, source.w, source.h}, source);
}

]=] "wrapped and overlapping VRAM copies")

patch_psyz_renderer_section(gpu_source
    "    u8* map = SDL_MapGPUTransferBuffer(device, vtx_transfer, true);"
    "    const SDL_GPUColorTargetInfo target = {\n        .texture = fb_index >= 0"
    [=[    if (!UploadTriangleData(cmd, vertex_buf, n_vertices, index_buf, n_indices, batch_has_texture)) {
        return;
    }

]=] "shared triangle uploads")

set(triangle_runs [=[    const float grid_scale_x = GetDrawGridXScale();
    const float render_scale = (float)internal_res;
    const PsyzSize target_size =
        fb_index >= 0 ? canvas_size : (PsyzSize){VRAM_W, VRAM_H};
    const PsyzSize scaled_size = {target_size.w * (int)internal_res,
                                  target_size.h * (int)internal_res};
    const SDL_Rect target_rect = {0, 0, scaled_size.w, scaled_size.h};
    const SDL_GPUViewport viewport = {
        .x = (float)offset.x * render_scale * (1.0f - grid_scale_x),
        .w = (float)scaled_size.w * grid_scale_x,
        .h = (float)scaled_size.h,
        .max_depth = 1.0f,
    };
    SDL_Rect clip = ScaleRect(fb_index >= 0 ? canvas_rect : scissor_rect, (int)internal_res);
    const float offset_ubo[6] = {
        (float)offset.x, (float)offset.y,
        0.5f / (render_scale * grid_scale_x), 0.5f / render_scale,
        (float)target_size.w, (float)target_size.h,
    };
    if (!SDL_GetRectIntersection(&clip, &target_rect, &clip) ||
        !RenderTriangleRuns(cmd, &target, &viewport, &clip, offset_ubo, scaled_size)) {
        Draw_ResetBuffer();
        return;
    }
]=])
patch_psyz_renderer_section(gpu_source
    "    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);\n    const float grid_scale_x"
    "    if (fb_index >= 0) {\n        InvalidateCanvas(&layout, fb_index, scissor_rect);"
    "${triangle_runs}" "ordered triangle runs")

file(CONFIGURE OUTPUT "${psyz_gpu_build}/sdl3_gpu.c"
    CONTENT "#line 1 \"psyz/src/platform/sdl3_gpu.c\"\n${gpu_source}" @ONLY)

get_target_property(psyz_sources psyz SOURCES)
if(NOT "src/platform/sdl3_gpu.c" IN_LIST psyz_sources)
    message(FATAL_ERROR "Could not replace the PsyZ Windows renderer source.")
endif()
list(REMOVE_ITEM psyz_sources "src/platform/sdl3_gpu.c")
set_property(TARGET psyz PROPERTY SOURCES "${psyz_sources};${psyz_gpu_build}/sdl3_gpu.c")
set_property(SOURCE "${psyz_gpu_build}/sdl3_gpu.c"
    TARGET_DIRECTORY psyz APPEND PROPERTY INCLUDE_DIRECTORIES
        "${psyz_platform}" "${CMAKE_CURRENT_LIST_DIR}")
