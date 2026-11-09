#include "ps1_xa_filter.h"

/* Hardware numeric coefficients, indexed by output phase then history tap.
 * https://psx-spx.consoledev.net/cdromformat/#25-point-zigzag-interpolation
 * Zero padding preserves physical ring indexing; each product is truncated. */
static const int16_t coefficients[7][29] = {
    {
        0, 0, 0, 0, 0, -2, 10, -34,
        65, -84, 52, 9, -266, 1024, -2680, 9036,
        26516, -6016, 3021, -1571, 848, -365, 107, 10,
        -16, 17, -8, 3, -1,
    },
    {
        0, 0, 0, -2, 0, 3, -19, 60,
        -75, 162, -227, 306, -67, -615, 3229, 29883,
        -4532, 2488, -1471, 882, -424, 166, -27, 5,
        6, -8, 3, -1, 0,
    },
    {
        0, 0, -1, 3, -2, -5, 31, -74,
        179, -402, 689, -926, 1272, -1446, 31033, -1446,
        1272, -926, 689, -402, 179, -74, 31, -5,
        -2, 3, -1, 0, 0,
    },
    {
        0, -1, 3, -8, 6, 5, -27, 166,
        -424, 882, -1471, 2488, -4532, 29883, 3229, -615,
        -67, 306, -227, 162, -75, 60, -19, 3,
        0, -2, 0, 0, 0,
    },
    {
        -1, 3, -8, 17, -16, 10, 107, -365,
        848, -1571, 3021, -6016, 26516, 9036, -2680, 1024,
        -266, 9, 52, -84, 65, -34, 10, -1,
        0, 1, 0, 0, 0,
    },
    {
        2, -8, 16, -35, 43, 26, -235, 635,
        -1352, 2810, -5882, 21472, 15367, -4681, 2062, -839,
        347, -68, -23, 70, -35, 17, -5, 0,
        0, 0, 0, 0, 0,
    },
    {
        -5, 17, -35, 70, -23, -68, 347, -839,
        2062, -4681, 15367, 21472, -5882, 2810, -1352, 635,
        -235, 26, 43, -35, 16, -8, 2, 0,
        0, 0, 0, 0, 0,
    },
};

int ps1_xa_filter_push(Ps1XaFilter *filter, int16_t left, int16_t right,
                        int16_t output[7][2]) {
    filter->ring[filter->position][0] = left;
    filter->ring[filter->position][1] = right;
    filter->position = (filter->position + 1) & 31;
    if (++filter->count != 6) return 0;
    filter->count = 0;
    for (unsigned phase = 0; phase < 7; phase++) {
        for (unsigned channel = 0; channel < 2; channel++) {
            int32_t sum = 0;
            for (unsigned tap = 1; tap <= 29; tap++) {
                int sample = filter->ring[(filter->position - tap) & 31][channel];
                sum += (sample * coefficients[phase][tap - 1]) >> 15;
            }
            if (sum > 32767) sum = 32767;
            if (sum < -32768) sum = -32768;
            output[phase][channel] = (int16_t)sum;
        }
    }
    return 7;
}
