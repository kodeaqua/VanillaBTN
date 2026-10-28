#ifndef HMBTN_COMPAT_INLINE_C_H
#define HMBTN_COMPAT_INLINE_C_H

#include "psx/gte_core.h"
#include "enhance/pgxp/pgxp.h"
#include <libgte.h>
#include <string.h>

extern hm_gte_state hm_gte;

static inline s16 hm_gte_half(s32 value) {
    u16 bits = (u16)(u32)value;
    return bits <= INT16_MAX ? (s16)bits : (s16)(-1 - (s32)(UINT16_MAX - bits));
}

static inline void gte_ldv0(const SVECTOR *v) {
    hm_gte.vector[0][0] = v->vx;
    hm_gte.vector[0][1] = v->vy;
    hm_gte.vector[0][2] = v->vz;
}

static inline void gte_ldv3(const SVECTOR *a, const SVECTOR *b, const SVECTOR *c) {
    const SVECTOR *v[3] = {a, b, c};
    for (u32 i = 0; i < 3; ++i) {
        hm_gte.vector[i][0] = v[i]->vx;
        hm_gte.vector[i][1] = v[i]->vy;
        hm_gte.vector[i][2] = v[i]->vz;
    }
}

static inline void gte_ldlv0(const s32 *v) {
    for (u32 i = 0; i < 3; ++i)
        hm_gte.vector[0][i] = hm_gte_half(v[i]);
}

static inline void gte_ldlvl(const VECTOR *v) {
    hm_gte.ir[0] = hm_gte_half(v->vx);
    hm_gte.ir[1] = hm_gte_half(v->vy);
    hm_gte.ir[2] = hm_gte_half(v->vz);
}

static inline void gte_ldclmv(const s16 *column) {
    for (u32 i = 0; i < 3; ++i)
        hm_gte.ir[i] = column[i * 3];
}

static inline void gte_stclmv(s16 *column) {
    for (u32 i = 0; i < 3; ++i)
        column[i * 3] = hm_gte.ir[i];
}

static inline void hm_gte_store_word(void *destination, u32 value) {
    memcpy(destination, &value, sizeof(value));
}

static inline void gte_stlvl(void *v) {
    for (u32 i = 0; i < 3; ++i)
        hm_gte_store_word((u8 *)v + i * 4, (u32)(s32)hm_gte.ir[i]);
}

static inline void gte_stlvnl(void *v) {
    for (u32 i = 0; i < 3; ++i)
        hm_gte_store_word((u8 *)v + i * 4, (u32)hm_gte.mac[i + 1]);
}

static inline void gte_stflg(void *flags) { hm_gte_store_word(flags, hm_gte.flags); }
static inline void gte_stopz(void *value) { hm_gte_store_word(value, (u32)hm_gte.mac[0]); }
static inline void gte_stotz(void *value) { hm_gte_store_word(value, hm_gte.otz); }
static inline void gte_stszotz(void *value) { hm_gte_store_word(value, hm_gte.sz[3] / 4); }

static inline u32 hm_gte_xy(u32 index) {
    return (u32)(u16)hm_gte.sx[index] | ((u32)(u16)hm_gte.sy[index] << 16);
}

static inline void gte_stsxy(void *value) {
    u32 xy = hm_gte_xy(2);
    hm_gte_store_word(value, xy);
    hm_pgxp_store_vertex(value, xy, hm_gte.sxf[2], hm_gte.syf[2], hm_gte.szf[3]);
}
static inline void gte_stsxy2(void *value) { gte_stsxy(value); }

static inline void hm_gte_packet_xy(void *packet, u32 stride) {
    for (u32 i = 0; i < 3; ++i) {
        void *dest = (u8 *)packet + 8 + i * stride;
        u32 xy = hm_gte_xy(i);
        hm_gte_store_word(dest, xy);
        hm_pgxp_store_vertex(dest, xy, hm_gte.sxf[i], hm_gte.syf[i], hm_gte.szf[i + 1]);
    }
}

#define gte_stsxy3_f3(p) hm_gte_packet_xy((p), 4)
#define gte_stsxy3_f4(p) hm_gte_packet_xy((p), 4)
#define gte_stsxy3_g3(p) hm_gte_packet_xy((p), 8)
#define gte_stsxy3_g4(p) hm_gte_packet_xy((p), 8)
#define gte_stsxy3_ft3(p) hm_gte_packet_xy((p), 8)
#define gte_stsxy3_ft4(p) hm_gte_packet_xy((p), 8)
#define gte_stsxy3_gt3(p) hm_gte_packet_xy((p), 12)
#define gte_stsxy3_gt4(p) hm_gte_packet_xy((p), 12)

static inline void gte_SetRotMatrix(const MATRIX *matrix) {
    memcpy(hm_gte.matrix[0], matrix->m, sizeof(matrix->m));
}
static inline void gte_SetLightMatrix(const MATRIX *matrix) {
    memcpy(hm_gte.matrix[1], matrix->m, sizeof(matrix->m));
}
static inline void gte_SetColorMatrix(const MATRIX *matrix) {
    memcpy(hm_gte.matrix[2], matrix->m, sizeof(matrix->m));
}
static inline void gte_SetTransMatrix(const MATRIX *matrix) {
    memcpy(hm_gte.translation[0], matrix->t, sizeof(matrix->t));
}
static inline void gte_ReadRotMatrix(MATRIX *matrix) {
    memcpy(matrix->m, hm_gte.matrix[0], sizeof(matrix->m));
    memcpy(matrix->t, hm_gte.translation[0], sizeof(matrix->t));
}
static inline void gte_SetBackColor(s32 r, s32 g, s32 b) {
    hm_gte.translation[1][0] = hm_gte_word((s64)r * 16);
    hm_gte.translation[1][1] = hm_gte_word((s64)g * 16);
    hm_gte.translation[1][2] = hm_gte_word((s64)b * 16);
}

static inline void gte_OuterProduct12(const VECTOR *a, const VECTOR *b, VECTOR *output) {
    VECTOR first = *a, second = *b;
    hm_gte.matrix[0][0][0] = hm_gte_half(first.vx);
    hm_gte.matrix[0][1][1] = hm_gte_half(first.vy);
    hm_gte.matrix[0][2][2] = hm_gte_half(first.vz);
    gte_ldlvl(&second);
    hm_gte_command(&hm_gte, 0x4B78000C);
    gte_stlvnl(output);
}

static inline MATRIX *gte_MulMatrix0(const MATRIX *a, const MATRIX *b, MATRIX *output) {
    MATRIX first = *a, second = *b;
    gte_SetRotMatrix(&first);
    for (u32 column = 0; column < 3; ++column) {
        gte_ldclmv(&second.m[0][column]);
        hm_gte_command(&hm_gte, 0x4A49E012);
        gte_stclmv(&output->m[0][column]);
    }
    return output;
}

#endif
