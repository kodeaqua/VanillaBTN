/* Included after the original save-image types and size constants. */
#include <libetc.h>
#include <stddef.h>

/* Original PS1 BSS placed these separate objects directly next to one another.
 * Native linkers and sanitizers cannot treat them as one contiguous buffer. */
typedef union {
    u8 bytes[SAVE_FILE_BYTES];
    struct {
        u8 header[0x80];
        IconFrame frames[3];
        u8 slot_record[0x80];
        u8 image_data[SAVE_FILE_BYTES - 0x280];
    } parts;
} hm_save_workspace_storage;
static hm_save_workspace_storage hm_save_workspace;
_Static_assert(sizeof(hm_save_workspace) == SAVE_FILE_BYTES, "save workspace byte extent");
_Static_assert(offsetof(hm_save_workspace_storage, parts.frames) == 0x80, "save icon frame offset");
_Static_assert(offsetof(hm_save_workspace_storage, parts.slot_record) == 0x200,
               "save slot record offset");
_Static_assert(offsetof(hm_save_workspace_storage, parts.image_data) == 0x280,
               "save payload offset");

#define save_file_header hm_save_workspace.bytes
#define save_icon_frame_0 hm_save_workspace.parts.frames[0]
#define save_icon_frame_1 hm_save_workspace.parts.frames[1]
#define save_icon_frame_2 hm_save_workspace.parts.frames[2]
#define save_slot_record hm_save_workspace.parts.slot_record
#define save_image_data hm_save_workspace.parts.image_data
