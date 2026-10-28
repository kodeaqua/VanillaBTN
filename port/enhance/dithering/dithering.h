#ifndef HMBTN_ENH_DITHERING_H
#define HMBTN_ENH_DITHERING_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(HMBTN_ENH_DITHERING) && HMBTN_ENH_DITHERING

void hm_dither_set_enabled(int enabled);
int hm_dither_enabled(void);

#else

static inline void hm_dither_set_enabled(int enabled) { (void)enabled; }
static inline int hm_dither_enabled(void) { return 1; }

#endif

#ifdef __cplusplus
}
#endif

#endif
