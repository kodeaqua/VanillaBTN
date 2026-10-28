#include "enhance/antialiasing/antialiasing.h"
unsigned hm_antialiasing_samples(int mode) {
    return mode == HM_AA_MSAA2 ? 2 : mode == HM_AA_MSAA4 ? 4 : 1;
}
int hm_antialiasing_cycle(int mode, int direction, unsigned supported) {
    if (mode < 0 || mode >= HM_AA_COUNT) mode = 0;
    supported |= 1; /* Off is always available. */
    for (int i = 0; i < HM_AA_COUNT; ++i) {
        mode = (mode + HM_AA_COUNT + (direction < 0 ? -1 : 1)) % HM_AA_COUNT;
        if (supported & (1u << mode)) return mode;
    }
    return HM_AA_OFF;
}
const char *hm_antialiasing_name(int mode) {
    static const char *const names[] = {"Off", "FXAA", "MSAA 2x", "MSAA 4x"};
    return mode >= 0 && mode < HM_AA_COUNT ? names[mode] : names[0];
}
static s64 edge(const s32 *a, const s32 *b, s64 x, s64 y) {
    return ((s64)b[0] - a[0]) * (y - a[1]) - ((s64)b[1] - a[1]) * (x - a[0]);
}
unsigned hm_antialiasing_coverage(const s32 v[6], int x, int y, unsigned samples) {
    static const int offsets[4][2] = {{96, 32}, {224, 96}, {32, 160}, {160, 224}};
    if (samples != 1 && samples != 2 && samples != 4) return 0;
    if (edge(v, v + 2, v[4], v[5]) <= 0) return 0;
    unsigned mask = 0;
    for (unsigned i = 0; i < samples; ++i) {
        int ox = samples == 1 ? 128 : samples == 2 ? (i ? 192 : 64) : offsets[i][0];
        int oy = samples == 1 ? 128 : samples == 2 ? (i ? 192 : 64) : offsets[i][1];
        int covered = 1;
        for (unsigned e = 0; e < 3; ++e) {
            const s32 *a = v + e * 2, *b = v + ((e + 1) % 3) * 2;
            s64 n = edge(a, b, (s64)x * 256 + ox, (s64)y * 256 + oy);
            int top_left = b[1] < a[1] || (b[1] == a[1] && b[0] > a[0]);
            if (n < 0 || (!n && !top_left)) covered = 0;
        }
        if (covered) mask |= 1u << i;
    }
    return mask;
}
