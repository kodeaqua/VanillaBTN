#ifndef VANILLABTN_COMPAT_LIBSND_H
#define VANILLABTN_COMPAT_LIBSND_H

#include "sdk_types.h"

#define SS_SERIAL_A 0
#define SS_SERIAL_B 1

#define SS_MIX 0
#define SS_REV 1

#define SS_OFF 0
#define SS_ON 1

#define SSPLAY_PAUSE 0
#define SSPLAY_PLAY 1
#define SSPLAY_INFINITY 0

void SsInit(void);
void SsSetTableSize(char *table, int s_max, int t_max);
void SsSetTickMode(long tick_mode);
char SsSetReservedVoice(char voices);
void SsStart(void);
void SsEnd(void);
void SsSetMVol(int voll, int volr);
void SsSetRVol(int voll, int volr);
void SsSetStereo(void);
void SsSetMono(void);
void SsSetSerialAttr(char s_num, char attr, char mode);
void SsSetSerialVol(short s_num, short voll, short volr);

short SsVabOpenHead(u_char *addr, int vabid);
short SsVabTransBodyPartly(u_char *addr, u_long msize, int vabid);
short SsVabTransCompleted(short immediateFlag);
void SsVabClose(int vabid);

short SsSeqOpen(u_long *addr, int vab_id);
void SsSeqPlay(int seq_access_num, char play_mode, short l_count);
void SsSeqPause(int seq_access_num);
void SsSeqReplay(int seq_access_num);
void SsSeqStop(int seq_access_num);
void SsSeqSetVol(int seq_access_num, int voll, int volr);
void SsSeqClose(int seq_access_num);

short SsUtKeyOn(int vabId, int prog, int tone, int note, int fine, int voll, int volr);
short SsUtKeyOff(int voice, int vabId, int prog, int tone, int note);
short SsUtKeyOnV(int voice, int vabId, int prog, int tone, int note, int fine, int voll, int volr);
short SsUtKeyOffV(int voice);
void SsUtAllKeyOff(int mode);
short SsUtSetVVol(int voice, int voll, int volr);
void SsVoKeyOff(long vab_pro, long pitch);

void SsUtReverbOn(void);
void SsUtReverbOff(void);
short SsUtSetReverbType(short type);
void SsUtSetReverbDepth(short ldepth, short rdepth);
void SsUtSetReverbFeedback(short feedback);
void SsUtSetReverbDelay(short delay);

#endif
