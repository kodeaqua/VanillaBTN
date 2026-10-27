#ifndef HMBTN_SAVE_PARTNER_H
#define HMBTN_SAVE_PARTNER_H
#include "farm_state.h"
#include <stddef.h>
typedef struct {
    FarmAnimals animals;
    FarmEstate estate;
    u8 identity[8];
} hm_save_partner;
int hm_save_partner_decode(hm_save_partner *partner, const void *image, size_t bytes);
int hm_save_partner_encode_animals(void *image, size_t bytes, hm_save_partner *partner);
#endif
