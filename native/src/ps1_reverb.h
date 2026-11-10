#ifndef VANILLABTN_PS1_REVERB_H
#define VANILLABTN_PS1_REVERB_H
#include <stdint.h>

/* The largest SDK preset reserves 0x18040 bytes at the top of SPU RAM.
 * Addresses below are halfword indices into this shared tail region. */
#define PS1_REVERB_WORDS (0x18040 / 2)
enum {
    RV_DAPF1, RV_DAPF2, RV_VIIR, RV_VCOMB1, RV_VCOMB2, RV_VCOMB3, RV_VCOMB4,
    RV_VWALL, RV_VAPF1, RV_VAPF2, RV_MLSAME, RV_MRSAME, RV_MLCOMB1, RV_MRCOMB1,
    RV_MLCOMB2, RV_MRCOMB2, RV_DLSAME, RV_DRSAME, RV_MLDIFF, RV_MRDIFF,
    RV_MLCOMB3, RV_MRCOMB3, RV_MLCOMB4, RV_MRCOMB4, RV_DLDIFF, RV_DRDIFF,
    RV_MLAPF1, RV_MRAPF1, RV_MLAPF2, RV_MRAPF2, RV_VLIN, RV_VRIN
};
typedef struct {
    uint16_t registers[32];
    int16_t ram[PS1_REVERB_WORDS];
    unsigned base, cursor, history_position, phase;
    int16_t input_history[64][2], output_history[64][2];
    int hifi_mode; /* 0 = authentic 22.05kHz alternating, 1 = hi-fi 44.1kHz stereo */
} Ps1Reverb;

int ps1_reverb_set_registers(Ps1Reverb *reverb, const uint16_t registers[32],
                            unsigned bytes, int clear);
int ps1_reverb_set_preset(Ps1Reverb *reverb, int type, int clear);
void ps1_reverb_set_echo(Ps1Reverb *reverb, int type, int delay, int feedback);
void ps1_reverb_set_hifi(Ps1Reverb *reverb, int enabled);
int  ps1_reverb_get_hifi(const Ps1Reverb *reverb);
/* One channel per 44.1 kHz clock, each at 22.05 kHz. Writes may be disabled
 * independently of RAM reads/output, matching the SPU master reverb bit. */
int16_t ps1_reverb_step(Ps1Reverb *reverb, unsigned channel, int16_t input, int write_enable);
void ps1_reverb_clock(Ps1Reverb *reverb, int32_t left, int32_t right,
                       int write_enable, int16_t output[2]);
#endif
