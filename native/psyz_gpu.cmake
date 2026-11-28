# Compile PsyZ with the patched Windows pixel shader without editing the checkout.
set(psyz_platform "${VANILLABTN_PSYZ_SOURCE_DIR}/psyz/src/platform")
set(psyz_gpu_source "${psyz_platform}/sdl3_gpu.c")
set(psyz_upstream_shader "${psyz_platform}/shaders/psx_frag_dxil.h")
set(psyz_patched_shader "${CMAKE_CURRENT_LIST_DIR}/shaders/psx_frag.dxil")
set(psyz_gpu_build "${CMAKE_CURRENT_BINARY_DIR}/psyz-win-gpu")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${psyz_gpu_source}" "${psyz_upstream_shader}" "${psyz_patched_shader}")

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

file(READ "${psyz_gpu_source}" gpu_source)
string(REPLACE "#include \"shaders/psx_frag_dxil.h\"" "#include \"vanillabtn_psx_frag_dxil.h\""
    patched_gpu_source "${gpu_source}")
get_target_property(psyz_sources psyz SOURCES)
if(gpu_source STREQUAL patched_gpu_source OR NOT "src/platform/sdl3_gpu.c" IN_LIST psyz_sources)
    message(FATAL_ERROR "Could not substitute the PsyZ Windows pixel shader.")
endif()
file(CONFIGURE OUTPUT "${psyz_gpu_build}/sdl3_gpu.c"
    CONTENT "#line 1 \"psyz/src/platform/sdl3_gpu.c\"\n${patched_gpu_source}" @ONLY)
list(REMOVE_ITEM psyz_sources "src/platform/sdl3_gpu.c")
set_property(TARGET psyz PROPERTY SOURCES "${psyz_sources};${psyz_gpu_build}/sdl3_gpu.c")
set_property(SOURCE "${psyz_gpu_build}/sdl3_gpu.c"
    TARGET_DIRECTORY psyz APPEND PROPERTY INCLUDE_DIRECTORIES "${psyz_platform}")
