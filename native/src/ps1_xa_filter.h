#ifndef VANILLABTN_PS1_XA_FILTER_H
#define VANILLABTN_PS1_XA_FILTER_H
#include <stdint.h>

/* Seven CD-ROM interpolation phases per six 37.8 kHz inputs. Feed each
 * 18.9 kHz sample twice. History persists across sectors and mix calls. */
typedef struct {
    int16_t ring[32][2];
    unsigned position, count;
} Ps1XaFilter;
int ps1_xa_filter_push(Ps1XaFilter *filter, int16_t left, int16_t right,
                        int16_t output[7][2]);
#endif
