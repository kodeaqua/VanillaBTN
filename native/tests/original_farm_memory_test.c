#include "event_script.h"
#include "sdk_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static Ps1Video video;
static u32 tail[2] = {22, 33};

int main(void) {
    u32 first = 11, buffer[3] = {0};
    sdk_gpu_bind(&video);
    CHECK(ps1_video_upload(&video, 0, 0, 2, 1, (u16 *)&first, 2));
    vram_stash_rect = (RECT){0, 0, 2, 1};
    vram_stash_tail = (u8 *)tail;
    /* One word in VRAM, two words in a 64-bit host buffer. */
    vram_chunk_offsets[0] = 0;
    vram_chunk_offsets[1] = 12;
    chunk_checksums[0] = 66;
    CHECK(event_chunk_read_back((u8 *)buffer, 0, 0, 0) == (u8 *)buffer);
    const u32 expected[3] = {11, 22, 33};
    CHECK(memcmp(buffer, expected, sizeof(expected)) == 0);
    /* A chunk wholly in the tail must preserve the complete borrowed pointer. */
    vram_chunk_offsets[0] = 4;
    chunk_checksums[0] = 55;
    CHECK(event_chunk_read_back((u8 *)buffer, 0, 0, 0) == (u8 *)tail);
    sdk_gpu_bind(NULL);
    return 0;
}
