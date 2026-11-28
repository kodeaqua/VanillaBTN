# Compile PsyZ with the patched Windows pixel shader without editing the checkout.
set(psyz_platform "${VANILLABTN_PSYZ_SOURCE_DIR}/psyz/src/platform")
set(psyz_gpu_source "${psyz_platform}/sdl3_gpu.c")
set(psyz_gpu_common "${psyz_platform}/sdl3_common.h")
set(psyz_upstream_shader "${psyz_platform}/shaders/psx_frag_dxil.h")
set(psyz_patched_shader "${CMAKE_CURRENT_LIST_DIR}/shaders/psx_frag.dxil")
set(psyz_gpu_build "${CMAKE_CURRENT_BINARY_DIR}/psyz-win-gpu")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${psyz_gpu_source}" "${psyz_gpu_common}" "${psyz_upstream_shader}" "${psyz_patched_shader}")

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
string(REPLACE "${texture_overlap}" "${wrapped_overlap}" patched_gpu_common "${gpu_common}")
if(gpu_common STREQUAL patched_gpu_common)
    message(FATAL_ERROR "Could not patch PsyZ wrapped texture dependencies.")
endif()

# Raw textured polygons copy RGB555 texels without shading or dithering.
string(REPLACE "(isGouraud || (isTextured && isShadeTex))"
    "(isTextured ? isShadeTex : isGouraud)" gpu_common_with_raw "${patched_gpu_common}")
if(patched_gpu_common STREQUAL gpu_common_with_raw)
    message(FATAL_ERROR "Could not patch PsyZ raw texture dithering.")
endif()
file(CONFIGURE OUTPUT "${psyz_gpu_build}/vanillabtn_sdl3_common.h"
    CONTENT "#line 1 \"psyz/src/platform/sdl3_common.h\"\n${gpu_common_with_raw}" @ONLY)

file(READ "${psyz_gpu_source}" gpu_source)
string(REPLACE "#include \"shaders/psx_frag_dxil.h\"" "#include \"vanillabtn_psx_frag_dxil.h\""
    patched_gpu_source "${gpu_source}")
string(REPLACE "#include \"sdl3_common.h\"" "#include \"vanillabtn_sdl3_common.h\""
    gpu_source_with_common "${patched_gpu_source}")
get_target_property(psyz_sources psyz SOURCES)
if(gpu_source STREQUAL patched_gpu_source OR patched_gpu_source STREQUAL gpu_source_with_common OR
   NOT "src/platform/sdl3_gpu.c" IN_LIST psyz_sources)
    message(FATAL_ERROR "Could not substitute the PsyZ Windows renderer patches.")
endif()

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
string(REPLACE "\r\n" "\n" gpu_source_with_common "${gpu_source_with_common}")
string(REPLACE "${line_vertex}" "${line_vertex_with_endpoints}"
    gpu_source_with_lines "${gpu_source_with_common}")
if(gpu_source_with_common STREQUAL gpu_source_with_lines)
    message(FATAL_ERROR "Could not patch PsyZ axis-aligned line endpoints.")
endif()

# Reject lines beyond the PS1 span before expanding them into geometry.
set(line_axis [=[                if ((dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy)) {]=])
set(bounded_line_axis [=[                enum { MAX_LINE_WIDTH = 1023, MAX_LINE_HEIGHT = 511 };
                const int width = abs(dx);
                const int height = abs(dy);
                if (width > MAX_LINE_WIDTH || height > MAX_LINE_HEIGHT) {
                    continue;
                }
                if (width >= height) {]=])
string(REPLACE "${line_axis}" "${bounded_line_axis}"
    gpu_source_with_line_limits "${gpu_source_with_lines}")
if(gpu_source_with_lines STREQUAL gpu_source_with_line_limits)
    message(FATAL_ERROR "Could not patch PsyZ line size limits.")
endif()

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
string(REPLACE "${line_end_color}"
    "                const int end_color = (dx != 0 || dy != 0) ? s + 1 : s;\n${point_line_color}"
    gpu_source_with_point_lines "${gpu_source_with_line_limits}")
if(gpu_source_with_line_limits STREQUAL gpu_source_with_point_lines)
    message(FATAL_ERROR "Could not patch PsyZ one-pixel line colors.")
endif()

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
string(REPLACE "${polygon_tpage}" "${polygon_tpage_latch}"
    gpu_source_with_tpage "${gpu_source_with_point_lines}")
if(gpu_source_with_point_lines STREQUAL gpu_source_with_tpage)
    message(FATAL_ERROR "Could not patch PsyZ polygon texture page state.")
endif()

# A VRAM fill must execute after the primitives already queued before it.
set(clear_image [=[    r = To15BitChannel(r);
    g = To15BitChannel(g);
    b = To15BitChannel(b);]=])
string(REPLACE "${clear_image}" "    Draw_FlushBuffer();\n${clear_image}"
    gpu_source_with_ordered_fill "${gpu_source_with_tpage}")
if(gpu_source_with_tpage STREQUAL gpu_source_with_ordered_fill)
    message(FATAL_ERROR "Could not patch PsyZ VRAM fill order.")
endif()
file(CONFIGURE OUTPUT "${psyz_gpu_build}/sdl3_gpu.c"
    CONTENT "#line 1 \"psyz/src/platform/sdl3_gpu.c\"\n${gpu_source_with_ordered_fill}" @ONLY)
list(REMOVE_ITEM psyz_sources "src/platform/sdl3_gpu.c")
set_property(TARGET psyz PROPERTY SOURCES "${psyz_sources};${psyz_gpu_build}/sdl3_gpu.c")
set_property(SOURCE "${psyz_gpu_build}/sdl3_gpu.c"
    TARGET_DIRECTORY psyz APPEND PROPERTY INCLUDE_DIRECTORIES "${psyz_platform}")
