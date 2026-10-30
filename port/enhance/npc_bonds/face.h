#ifndef HMBTN_NPC_BONDS_FACE_H
#define HMBTN_NPC_BONDS_FACE_H

#include <stddef.h>
#include <stdint.h>

#define HM_NPC_BONDS_FACE_SIZE 18
#define HM_NPC_BONDS_FACE_PIXELS (HM_NPC_BONDS_FACE_SIZE * HM_NPC_BONDS_FACE_SIZE)

/* Decode one original face.bin bank into a transparent 18x18 RGB image. */
int hm_npc_bonds_decode_face_bank(const uint8_t *bank, size_t bank_size,
                                  uint32_t pixels[HM_NPC_BONDS_FACE_PIXELS]);

#endif
