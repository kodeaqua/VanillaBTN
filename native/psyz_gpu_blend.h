/* SPDX-License-Identifier: MPL-2.0 */

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

static bool TriangleMayBlend(int first) {
    enum { SEMI_TRANSPARENT_ALPHA = 0x80 };
    return vertex_buf[index_buf[first]].a == SEMI_TRANSPARENT_ALPHA;
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

static bool RenderTriangleRuns(SDL_GPUCommandBuffer *cmd, const SDL_GPUColorTargetInfo *target,
                               const SDL_GPUViewport *viewport, const SDL_Rect *clip,
                               const float offset_ubo[6], PsyzSize target_size) {
    enum { TRIANGLE_INDEX_COUNT = 3 };
    int first_blend = 0;
    while (first_blend < n_indices && !TriangleMayBlend(first_blend)) {
        first_blend += TRIANGLE_INDEX_COUNT;
    }
    if (first_blend < n_indices && !EnsureBlendBackground(target_size.w, target_size.h)) {
        return false;
    }

    const float scale_x = viewport->w / offset_ubo[4];
    const float scale_y = viewport->h / offset_ubo[5];
    const float offset_x = viewport->x + offset_ubo[0] * scale_x;
    const float offset_y = viewport->y + offset_ubo[1] * scale_y;
    const SDL_GPUBufferBinding vb = {.buffer = vbuf};
    const SDL_GPUBufferBinding ib = {.buffer = ibuf};
    for (int first = 0; first < n_indices;) {
        int end = first;
        SDL_Rect background = {0};
        /* A triangle may reuse the snapshot only if earlier draws left its pixels alone. */
        while (end < n_indices && TriangleMayBlend(end)) {
            SDL_Rect bounds = BlendTriangleBounds(end, clip, scale_x, scale_y, offset_x, offset_y);
            if (SDL_HasRectIntersection(&background, &bounds)) {
                break;
            }
            SDL_GetRectUnion(&background, &bounds, &background);
            end += TRIANGLE_INDEX_COUNT;
        }
        while (end < n_indices && !TriangleMayBlend(end)) {
            end += TRIANGLE_INDEX_COUNT;
        }

        SDL_GPUTexture *snapshot = vram_sample;
        if (background.w > 0 && background.h > 0) {
            snapshot = blend_background;
            const Uint32 x = (Uint32)background.x;
            const Uint32 y = (Uint32)background.y;
            const SDL_GPUTextureLocation src = {.texture = target->texture, .x = x, .y = y};
            const SDL_GPUTextureLocation dst = {.texture = snapshot, .x = x, .y = y};
            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
            SDL_CopyGPUTextureToTexture(copy, &src, &dst, (Uint32)background.w,
                                        (Uint32)background.h, 1, false);
            SDL_EndGPUCopyPass(copy);
        }

        const SDL_GPUTextureSamplerBinding samplers[] = {
            {.texture = vram_sample, .sampler = vram_sampler},
            {.texture = snapshot, .sampler = vram_sampler},
        };
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, target, 1, NULL);
        SDL_SetGPUViewport(pass, viewport);
        SDL_SetGPUScissor(pass, clip);
        SDL_BindGPUGraphicsPipeline(pass, pipe_tri);
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_BindGPUFragmentSamplers(pass, 0, samplers, SDL_arraysize(samplers));
        SDL_PushGPUVertexUniformData(cmd, 0, offset_ubo, 6 * sizeof(*offset_ubo));
        SDL_DrawGPUIndexedPrimitives(pass, (Uint32)(end - first), 1, (Uint32)first, 0, 0);
        SDL_EndGPURenderPass(pass);
        first = end;
    }
    return true;
}
