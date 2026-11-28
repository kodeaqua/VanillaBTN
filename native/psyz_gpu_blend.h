/* SPDX-License-Identifier: MPL-2.0 */

enum { VRAM_RAW_TRANSFER = 4 };

static bool EnsureBlendBackground(int width, int height) {
    if (blend_background && blend_background_size.w >= width && blend_background_size.h >= height) {
        return true;
    }
    const SDL_GPUTextureCreateInfo info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = (Uint32)width,
        .height = (Uint32)height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &info);
    if (!texture) {
        ERRORF("blend background creation failed: %s", SDL_GetError());
        return false;
    }
    if (blend_background) {
        SDL_ReleaseGPUTexture(device, blend_background);
    }
    blend_background = texture;
    blend_background_size = (PsyzSize){width, height};
    return true;
}

static bool TriangleReadsTarget(int first) {
    enum { SEMI_TRANSPARENT_ALPHA = 0x80 };
    return (vram_mask_flags & VRAM_MASK_CHECK) ||
           vertex_buf[index_buf[first]].a == SEMI_TRANSPARENT_ALPHA;
}

static SDL_Rect BlendTriangleBounds(int first, const SDL_Rect *clip, float scale_x, float scale_y,
                                    float offset_x, float offset_y) {
    const Vertex *v = &vertex_buf[index_buf[first]];
    int left = v->x, right = v->x, top = v->y, bottom = v->y;
    for (int i = 1; i < 3; ++i) {
        v = &vertex_buf[index_buf[first + i]];
        left = SDL_min(left, v->x);
        right = SDL_max(right, v->x);
        top = SDL_min(top, v->y);
        bottom = SDL_max(bottom, v->y);
    }
    const int clip_right = clip->x + clip->w;
    const int clip_bottom = clip->y + clip->h;
    /* The extra pixel covers the polygon center shift and viewport rounding. */
    left = (int)SDL_clamp(SDL_floorf(left * scale_x + offset_x) - 1.0f, clip->x, clip_right);
    right = (int)SDL_clamp(SDL_ceilf(right * scale_x + offset_x) + 1.0f, clip->x, clip_right);
    top = (int)SDL_clamp(SDL_floorf(top * scale_y + offset_y) - 1.0f, clip->y, clip_bottom);
    bottom = (int)SDL_clamp(SDL_ceilf(bottom * scale_y + offset_y) + 1.0f, clip->y, clip_bottom);
    return (SDL_Rect){left, top, right - left, bottom - top};
}

static void CopyTextureRegion(SDL_GPUCopyPass *pass, SDL_GPUTexture *source,
                              SDL_GPUTexture *destination, SDL_Rect area, SDL_Point origin) {
    const SDL_GPUTextureLocation src = {
        .texture = source, .x = (Uint32)area.x, .y = (Uint32)area.y};
    const SDL_GPUTextureLocation dst = {
        .texture = destination, .x = (Uint32)origin.x, .y = (Uint32)origin.y};
    SDL_CopyGPUTextureToTexture(pass, &src, &dst, (Uint32)area.w, (Uint32)area.h, 1, false);
}

static void CopyTargetRegion(SDL_GPUCopyPass *pass, SDL_GPUTexture *source,
                             SDL_GPUTexture *destination, SDL_Rect area) {
    CopyTextureRegion(pass, source, destination, area, (SDL_Point){area.x, area.y});
}

static bool UploadTriangleData(SDL_GPUCommandBuffer *cmd, const Vertex *vertices, Uint32 n_vertices,
                               const u16 *indices, Uint32 n_indices, bool sync_vram_sample) {
    u8 *map = SDL_MapGPUTransferBuffer(device, vtx_transfer, true);
    if (!map) {
        ERRORF("SDL_MapGPUTransferBuffer: %s", SDL_GetError());
        return false;
    }
    const Uint32 vtx_size = n_vertices * sizeof(*vertices);
    const Uint32 idx_size = n_indices * sizeof(*indices);
    const Uint32 idx_offset = MAX_VERTEX_COUNT * sizeof(Vertex);
    memcpy(map, vertices, vtx_size);
    memcpy(map + idx_offset, indices, idx_size);
    SDL_UnmapGPUTransferBuffer(device, vtx_transfer);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTransferBufferLocation vtx_src = {.transfer_buffer = vtx_transfer};
    const SDL_GPUBufferRegion vtx_dst = {.buffer = vbuf, .size = vtx_size};
    const SDL_GPUTransferBufferLocation idx_src = {.transfer_buffer = vtx_transfer,
                                                   .offset = idx_offset};
    const SDL_GPUBufferRegion idx_dst = {.buffer = ibuf, .size = idx_size};
    SDL_UploadToGPUBuffer(copy, &vtx_src, &vtx_dst, true);
    SDL_UploadToGPUBuffer(copy, &idx_src, &idx_dst, true);
    if (sync_vram_sample && vram_dirty.w > 0 && vram_dirty.h > 0) {
        CopyTargetRegion(copy, vram_render, vram_sample, vram_dirty);
        ResetVramDirty();
    }
    SDL_EndGPUCopyPass(copy);
    return true;
}

static void RenderTargetTriangles(SDL_GPUCommandBuffer *cmd, const SDL_GPUColorTargetInfo *target,
                                  const SDL_GPUViewport *viewport, const SDL_Rect *clip,
                                  const float offset_ubo[6], SDL_Rect background, Uint32 flags,
                                  Uint32 first, Uint32 count) {
    SDL_GPUTexture *snapshot = vram_sample;
    if (background.w > 0 && background.h > 0) {
        snapshot = blend_background;
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        CopyTargetRegion(copy, target->texture, snapshot, background);
        SDL_EndGPUCopyPass(copy);
    }
    const SDL_GPUTextureSamplerBinding samplers[] = {
        {.texture = vram_sample, .sampler = vram_sampler},
        {.texture = snapshot, .sampler = vram_sampler},
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, target, 1, NULL);
    if (viewport) {
        SDL_SetGPUViewport(pass, viewport);
    }
    SDL_SetGPUScissor(pass, clip);
    SDL_BindGPUGraphicsPipeline(pass, pipe_tri);
    const SDL_GPUBufferBinding vb = {.buffer = vbuf};
    const SDL_GPUBufferBinding ib = {.buffer = ibuf};
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_BindGPUFragmentSamplers(pass, 0, samplers, SDL_arraysize(samplers));
    SDL_PushGPUVertexUniformData(cmd, 0, offset_ubo, 6 * sizeof(*offset_ubo));
    SDL_PushGPUFragmentUniformData(cmd, 0, &flags, sizeof(flags));
    SDL_DrawGPUIndexedPrimitives(pass, count, 1, first, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static bool RenderTriangleRuns(SDL_GPUCommandBuffer *cmd, const SDL_GPUColorTargetInfo *target,
                               const SDL_GPUViewport *viewport, const SDL_Rect *clip,
                               const float offset_ubo[6], PsyzSize target_size) {
    enum { TRIANGLE_INDEX_COUNT = 3 };
    int first_read = 0;
    while (first_read < n_indices && !TriangleReadsTarget(first_read)) {
        first_read += TRIANGLE_INDEX_COUNT;
    }
    if (first_read < n_indices && !EnsureBlendBackground(target_size.w, target_size.h)) {
        return false;
    }

    const float scale_x = viewport->w / offset_ubo[4];
    const float scale_y = viewport->h / offset_ubo[5];
    const float offset_x = viewport->x + offset_ubo[0] * scale_x;
    const float offset_y = viewport->y + offset_ubo[1] * scale_y;
    for (int first = 0; first < n_indices;) {
        int end = first;
        SDL_Rect background = {0};
        /* A triangle may reuse the snapshot only if earlier draws left its pixels alone. */
        while (end < n_indices && TriangleReadsTarget(end)) {
            SDL_Rect bounds = BlendTriangleBounds(end, clip, scale_x, scale_y, offset_x, offset_y);
            if (SDL_HasRectIntersection(&background, &bounds)) {
                break;
            }
            SDL_GetRectUnion(&background, &bounds, &background);
            end += TRIANGLE_INDEX_COUNT;
        }
        while (end < n_indices && !TriangleReadsTarget(end)) {
            end += TRIANGLE_INDEX_COUNT;
        }

        RenderTargetTriangles(cmd, target, viewport, clip, offset_ubo, background, vram_mask_flags,
                              (Uint32)first, (Uint32)(end - first));
        first = end;
    }
    return true;
}

/* Transfer coordinates are absolute and the source is staged in vram_sample. */
static bool RenderMaskedTransfer(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *texture, PsyzSize size,
                                 SDL_Rect destination, SDL_Rect source) {
    const bool check = (vram_mask_flags & VRAM_MASK_CHECK) != 0;
    if (check && !EnsureBlendBackground(size.w, size.h)) {
        return false;
    }
    const int x = destination.x, y = destination.y;
    const int right = x + destination.w, bottom = y + destination.h;
    const int u = source.x, v = source.y;
    const int u_end = u + source.w, v_end = v + source.h;
    const Vertex quad[] = {
        {.x = x, .y = y, .u = u, .v = v},
        {.x = right, .y = y, .u = u_end, .v = v},
        {.x = x, .y = bottom, .u = u, .v = v_end},
        {.x = right, .y = bottom, .u = u_end, .v = v_end},
    };
    static const u16 indices[] = {0, 1, 2, 1, 3, 2};
    if (!UploadTriangleData(cmd, quad, SDL_arraysize(quad), indices, SDL_arraysize(indices),
                            false)) {
        return false;
    }

    const SDL_GPUColorTargetInfo target = {
        .texture = texture,
        .load_op = SDL_GPU_LOADOP_LOAD,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    const float offset_ubo[6] = {0.0f, 0.0f, 0.0f, 0.0f, (float)size.w, (float)size.h};
    RenderTargetTriangles(cmd, &target, NULL, &destination, offset_ubo,
                          check ? destination : (SDL_Rect){0}, vram_mask_flags | VRAM_RAW_TRANSFER,
                          0, SDL_arraysize(indices));
    return true;
}

static bool UploadMaskedImage(SDL_GPUTexture *texture, PsyzSize size, SDL_Rect destination,
                              int scale, int pitch, int sx, int sy) {
    SDL_GPUCommandBuffer *cmd = AcquireCmd();
    if (!cmd) {
        return false;
    }
    const SDL_Rect staging = {0, 0, destination.w, destination.h};
    UploadImage(vram_sample, 0, 0, pitch, sx, sy, staging.w, staging.h);
    MarkVramDirty(staging);
    const PsyzSize scaled = {size.w * scale, size.h * scale};
    return RenderMaskedTransfer(cmd, texture, scaled, ScaleRect(destination, scale), staging);
}

enum { MAX_VRAM_PARTS = 4 };

typedef struct {
    SDL_Rect area;
    SDL_Point offset;
} VramPart;

static bool ValidVramTransfer(SDL_Rect area) {
    return area.x >= 0 && area.x < VRAM_W && area.y >= 0 && area.y < VRAM_H && area.w > 0 &&
           area.w <= VRAM_W && area.h > 0 && area.h <= VRAM_H;
}

static int SplitVramRect(SDL_Rect area, VramPart parts[MAX_VRAM_PARTS]) {
    int count = 0;
    for (int y = 0; y < area.h;) {
        int top = (area.y + y) % VRAM_H;
        int height = SDL_min(area.h - y, VRAM_H - top);
        for (int x = 0; x < area.w;) {
            int left = (area.x + x) % VRAM_W;
            int width = SDL_min(area.w - x, VRAM_W - left);
            assert(count < MAX_VRAM_PARTS);
            parts[count++] = (VramPart){{left, top, width, height}, {x, y}};
            x += width;
        }
        y += height;
    }
    return count;
}

static SDL_Rect StageVramParts(SDL_GPUCopyPass *copy, const VramPart *parts, int count,
                               SDL_Rect source) {
    if (count == 1) {
        CopyTargetRegion(copy, vram_render, vram_sample, parts[0].area);
        return parts[0].area;
    }
    for (int i = 0; i < count; ++i) {
        CopyTextureRegion(copy, vram_render, vram_sample, parts[i].area, parts[i].offset);
    }
    const SDL_Rect packed = {0, 0, source.w, source.h};
    MarkVramDirty(packed);
    return packed;
}

static void PublishVramWrite(const VideoLayout *layout, SDL_Rect area) {
    MarkVramDirty(area);
    SyncNativeVramToScaled(area.x, area.y, area.w, area.h);
    for (int i = 0; layout->native && i < fb_origin_count; ++i) {
        const SDL_Rect fb = GetFbRect(i);
        if (SDL_HasRectIntersection(&area, &fb) && PrepareCanvas(layout, i)) {
            CopyVramToCanvas(i, area);
        }
    }
}

static bool UploadVramRegion(SDL_Rect area, int pitch, SDL_Point offset) {
    if (vram_mask_flags) {
        return UploadMaskedImage(vram_render, (PsyzSize){VRAM_W, VRAM_H}, area, 1, pitch, offset.x,
                                 offset.y);
    }
    UploadImage(vram_render, area.x, area.y, pitch, offset.x, offset.y, area.w, area.h);
    return true;
}

static void UploadWrappedVram(const VideoLayout *layout, SDL_Rect area) {
    VramPart parts[MAX_VRAM_PARTS];
    const int count = SplitVramRect(area, parts);
    for (int i = 0; i < count; ++i) {
        if (layout->native && (vram_mask_flags & VRAM_MASK_CHECK)) {
            SyncDirtyCanvases(&parts[i].area);
        }
        if (UploadVramRegion(parts[i].area, area.w, parts[i].offset)) {
            PublishVramWrite(layout, parts[i].area);
        }
    }
}

static bool CopyVramBlock(SDL_GPUCommandBuffer *cmd, SDL_Rect destination, SDL_Rect source) {
    VramPart src[MAX_VRAM_PARTS], dst[MAX_VRAM_PARTS];
    const int src_count = SplitVramRect(source, src);
    const int dst_count = SplitVramRect(destination, dst);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_Rect staged = StageVramParts(copy, src, src_count, source);
    const bool masked = vram_mask_flags != 0;
    if (masked) {
        SDL_EndGPUCopyPass(copy);
    }
    for (int i = 0; i < dst_count; ++i) {
        const SDL_Rect read = {staged.x + dst[i].offset.x, staged.y + dst[i].offset.y,
                               dst[i].area.w, dst[i].area.h};
        if (!masked) {
            const SDL_Point origin = {dst[i].area.x, dst[i].area.y};
            CopyTextureRegion(copy, vram_sample, vram_render, read, origin);
        } else if (!RenderMaskedTransfer(cmd, vram_render, (PsyzSize){VRAM_W, VRAM_H}, dst[i].area,
                                         read)) {
            return false;
        }
    }
    if (!masked) {
        SDL_EndGPUCopyPass(copy);
    }
    return true;
}

static bool VramPartsOverlap(const VramPart *src, int src_count, const VramPart *dst,
                             int dst_count) {
    for (int i = 0; i < src_count; ++i) {
        for (int j = 0; j < dst_count; ++j) {
            const SDL_Rect a = src[i].area, b = dst[j].area;
            if (SDL_HasRectIntersection(&a, &b)) {
                return true;
            }
        }
    }
    return false;
}

static void CopyVramInOrder(SDL_GPUCommandBuffer *cmd, SDL_Rect destination, SDL_Rect source) {
    enum { COPY_BLOCK_WIDTH = 128 };
    /* Stage both sides of a wrapped source block before writing any of its destination. */
    for (int y = 0; y < source.h; ++y) {
        for (int x = 0; x < source.w; x += COPY_BLOCK_WIDTH) {
            const int width = SDL_min(COPY_BLOCK_WIDTH, source.w - x);
            const SDL_Rect src = {source.x + x, source.y + y, width, 1};
            const SDL_Rect dst = {destination.x + x, destination.y + y, width, 1};
            if (!CopyVramBlock(cmd, dst, src)) {
                return;
            }
        }
    }
}

static void MoveVramRegion(SDL_Rect destination, SDL_Rect source) {
    if (SDL_RectsEqual(&source, &destination) && !(vram_mask_flags & VRAM_MASK_SET)) {
        return;
    }
    Draw_FlushBuffer();
    const VideoLayout layout = ResolveVideoLayout();
    VramPart src[MAX_VRAM_PARTS], dst[MAX_VRAM_PARTS];
    const int src_count = SplitVramRect(source, src);
    const int dst_count = SplitVramRect(destination, dst);
    if (layout.native) {
        for (int i = 0; i < src_count; ++i) {
            SyncDirtyCanvases(&src[i].area);
        }
        for (int i = 0; i < dst_count && (vram_mask_flags & VRAM_MASK_CHECK); ++i) {
            SyncDirtyCanvases(&dst[i].area);
        }
    } else {
        LeaveNative();
    }
    SDL_GPUCommandBuffer *cmd = AcquireCmd();
    if (!cmd) {
        return;
    }
    if (!SDL_RectsEqual(&source, &destination) &&
        VramPartsOverlap(src, src_count, dst, dst_count)) {
        CopyVramInOrder(cmd, destination, source);
    } else {
        CopyVramBlock(cmd, destination, source);
    }
    for (int i = 0; i < dst_count; ++i) {
        PublishVramWrite(&layout, dst[i].area);
    }
}
