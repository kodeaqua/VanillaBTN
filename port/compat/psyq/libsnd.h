#ifndef HMBTN_COMPAT_LIBSND_H
#define HMBTN_COMPAT_LIBSND_H

#include "compat/types.h"

/* Native call contracts inferred from the source. Implementations are required
 * at link time; declarations do not pretend to play or complete audio. */
void SsInit(void);
void SsStart(void);
void SsEnd(void);
void SsSetTableSize(void *table, s32 sequences, s32 tracks);
void SsSetTickMode(s32 mode);
s32 SsSetReservedVoice(s32 voices);
void SsSetSerialAttr(s32 channel, s32 attribute, s32 enabled);
void SsSetSerialVol(s32 channel, s32 left, s32 right);
void SsSetMVol(s32 left, s32 right);
void SsSetRVol(s32 left, s32 right);
void SsSetStereo(void);
void SsSetMono(void);
s32 SsSeqOpen(const u32 *data, s32 bank);
/* Native asset bounds, supplied by the guarded original sound loader. */
void hm_sound_sequence_register(const void *data, u32 bytes);
void SsSeqClose(s32 sequence);
void SsSeqPlay(s32 sequence, s32 mode, s32 repeats);
void SsSeqStop(s32 sequence);
void SsSeqPause(s32 sequence);
void SsSeqReplay(s32 sequence);
void SsSeqSetVol(s32 sequence, s32 left, s32 right);
s32 SsVabOpenHead(const void *header, s32 bank);
void SsVabClose(s32 bank);
s32 SsVabTransBodyPartly(const void *data, s32 bytes, s32 bank);
s32 SsVabTransCompleted(s32 mode);
void SsUtAllKeyOff(s32 mode);
void SsUtKeyOff(s32 voice, s32 bank, s32 program, s32 tone, s32 note);
s32 SsUtKeyOffV(s32 voice);
s32 SsUtKeyOn(s32 bank, s32 program, s32 tone, s32 note, s32 fine, s32 left, s32 right);
s32 SsUtKeyOnV(s32 voice, s32 bank, s32 program, s32 tone, s32 note, s32 fine, s32 left, s32 right);
void SsVoKeyOff(s32 voice_bank, s32 mode);
void SsUtSetVVol(s32 voice, s32 left, s32 right);
void SsUtReverbOn(void);
void SsUtReverbOff(void);
s32 SsUtSetReverbType(s32 type);
void SsUtSetReverbDepth(s32 left, s32 right);
void SsUtSetReverbFeedback(s32 feedback);
void SsUtSetReverbDelay(s32 delay);

#endif
