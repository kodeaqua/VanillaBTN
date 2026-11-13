#include "host_audio_policy.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "host audio policy assertion failed at line %d: %s\n", \
            __LINE__, #condition); exit(1); \
} } while (0)

static void test_default_is_bit_exact(void) {
    int16_t samples[] = {INT16_MIN, -12345, -1, 0, 1, 12345, INT16_MAX};
    int16_t original[sizeof(samples) / sizeof(samples[0])];
    memcpy(original, samples, sizeof(samples));
    host_audio_apply_volume(samples, 7, 100, false);
    CHECK(memcmp(samples, original, sizeof(samples)) == 0);
    host_audio_apply_volume(NULL, 7, 50, false);
    host_audio_apply_volume(samples, 0, 0, true);
    CHECK(memcmp(samples, original, sizeof(samples)) == 0);
}

static void test_master_volume_and_focus_mute(void) {
    int16_t samples[] = {INT16_MIN, -10000, -1, 0, 1, 10000, INT16_MAX};
    host_audio_apply_volume(samples, 7, 50, false);
    CHECK(samples[0] == -16384 && samples[1] == -5000);
    CHECK(samples[2] == 0 && samples[3] == 0 && samples[4] == 0);
    CHECK(samples[5] == 5000 && samples[6] == 16383);
    host_audio_apply_volume(samples, 7, 100, true);
    for (int i = 0; i < 7; ++i) CHECK(samples[i] == 0);
    samples[0] = INT16_MAX;
    host_audio_apply_volume(samples, 7, 0, false);
    CHECK(samples[0] == 0);
    samples[0] = INT16_MIN;
    host_audio_apply_volume(samples, 7, 300, false);
    CHECK(samples[0] == INT16_MIN); /* Defensive range handling */
}

static void test_music_onset_reserves_only_at_first_pcm(void) {
    HostAudioMusicOnset onset = {0};
    const size_t tick_bytes = 735 * 2 * sizeof(int16_t);
    CHECK(host_audio_music_onset_padding(&onset, false, false, 0, tick_bytes) == 0);
    CHECK(host_audio_music_onset_padding(&onset, true, false, 0, tick_bytes) == 0);
    CHECK(!onset.primed); /* Music may spend several ticks loading or fading. */
    CHECK(host_audio_music_onset_padding(&onset, true, true, 0, tick_bytes) == tick_bytes * 2);
    CHECK(onset.primed);
    CHECK(host_audio_music_onset_padding(&onset, true, true, 0, tick_bytes) == 0);
    CHECK(host_audio_music_onset_padding(&onset, false, false, 0, tick_bytes) == 0);
    CHECK(!onset.primed);
    CHECK(host_audio_music_onset_padding(&onset, true, true, (int)tick_bytes,
                                         tick_bytes) == tick_bytes);
    CHECK(host_audio_music_onset_padding(&onset, true, true, 0, tick_bytes) == 0);
    CHECK(host_audio_music_onset_padding(&onset, false, false, 0, tick_bytes) == 0);
    CHECK(host_audio_music_onset_padding(&onset, true, true, (int)(tick_bytes * 3),
                                         tick_bytes) == 0);
    CHECK(host_audio_music_onset_padding(NULL, true, true, 0, tick_bytes) == 0);
    CHECK(host_audio_music_onset_padding(&onset, true, true, -1, tick_bytes) == 0);
}

static void test_audio_queue_does_not_replay_stale_samples(void) {
    const int tick_bytes = HOST_AUDIO_TICK_FRAMES * 2 * (int)sizeof(int16_t);
    int first = -1;

    CHECK(host_audio_queue_frames(0, HOST_AUDIO_TICK_FRAMES, &first) == 735);
    CHECK(first == 0);
    CHECK(host_audio_queue_frames(3 * tick_bytes, HOST_AUDIO_MAX_FRAMES, &first) == 735);
    CHECK(first == 2205); /* Keep newest tick from a four-tick catch-up. */
    CHECK(host_audio_queue_frames(4 * tick_bytes, HOST_AUDIO_TICK_FRAMES, &first) == 0);
    CHECK(host_audio_queue_frames(4 * tick_bytes - 1, HOST_AUDIO_TICK_FRAMES, &first) == 0);
    CHECK(host_audio_queue_frames(0, HOST_AUDIO_MAX_FRAMES, &first) == HOST_AUDIO_MAX_FRAMES);
    CHECK(first == 0);
    CHECK(host_audio_queue_frames(-1, HOST_AUDIO_TICK_FRAMES, &first) == 0);
    CHECK(first == 0);
    CHECK(host_audio_queue_frames(0, 0, &first) == 0);
    CHECK(host_audio_queue_frames(0, HOST_AUDIO_TICK_FRAMES, NULL) == 0);

    /* A music-onset reserve must be counted before submitting actual music. */
    CHECK(host_audio_queue_frames(2 * tick_bytes, HOST_AUDIO_MAX_FRAMES, &first) == 1470);
    CHECK(first == 1470);

    /* Even repeated catch-up bursts cannot grow the SDL queue past its cap. */
    int queued = 0;
    for (int i = 0; i < 100; ++i) {
        int consumed = queued > tick_bytes / 2 ? tick_bytes / 2 : queued;
        queued -= consumed;
        int count = host_audio_queue_frames(queued, HOST_AUDIO_MAX_FRAMES, &first);
        CHECK(count >= 0 && first + count == HOST_AUDIO_MAX_FRAMES);
        queued += count * 2 * (int)sizeof(int16_t);
        CHECK(queued <= HOST_AUDIO_QUEUE_LIMIT_FRAMES * 2 * (int)sizeof(int16_t));
    }
}

static void test_audio_clock_matches_host_elapsed_time(void) {
    HostAudioClock clock = {0};
    const uint64_t tick_ns = 1000000000ULL / 60;
    uint64_t now = 1000000000ULL;
    CHECK(host_audio_clock_frames(&clock, now, false) == HOST_AUDIO_TICK_FRAMES);
    CHECK(host_audio_clock_frames(&clock, now, false) == 0);

    /* At a steady 60 Hz, fractional sample remainders do not accumulate
     * long-term speed or pitch drift. */
    unsigned frames = 0;
    for (int i = 0; i < 600; ++i) {
        now += tick_ns + (i % 3 == 0);
        frames += (unsigned)host_audio_clock_frames(&clock, now, false);
    }
    CHECK(frames >= 440990 && frames <= 441010);

    /* A skipped VSync boundary consumes the actual elapsed audio time,
     * rather than permanently losing one tick of PCM. */
    now += 2 * tick_ns;
    CHECK(host_audio_clock_frames(&clock, now, false) >= 1469);
    CHECK(host_audio_clock_frames(&clock, now + tick_ns, false) >= 734);

    /* Interleaved short and long callbacks still cover the same total time. */
    host_audio_clock_reset(&clock);
    now = 1000000000ULL;
    CHECK(host_audio_clock_frames(&clock, now, false) == 735);
    now += 5000000ULL;
    int short_frames = host_audio_clock_frames(&clock, now, false);
    now += 11666667ULL;
    int long_frames = host_audio_clock_frames(&clock, now, false);
    CHECK(short_frames + long_frames == 735);

    /* A huge OS stall is bounded, with no subsequent compensation burst. */
    now += 2000000000ULL;
    CHECK(host_audio_clock_frames(&clock, now, false) == HOST_AUDIO_MAX_FRAMES);
    CHECK(host_audio_clock_frames(&clock, now + tick_ns, false) >= 734);

    /* Turbo retains the original accelerated-tick SPU pacing. */
    now += 4 * tick_ns;
    CHECK(host_audio_clock_frames(&clock, now, true) == 735);
    CHECK(host_audio_clock_frames(&clock, now + tick_ns / 4, true) == 735);
    CHECK(host_audio_clock_frames(&clock, now + tick_ns, false) >= 550);

    host_audio_clock_reset(&clock); /* Pause/resume must not catch up silence. */
    CHECK(host_audio_clock_frames(&clock, now + 10000000000ULL, false) == 735);
    CHECK(host_audio_clock_frames(NULL, now, false) == 735);
}

int main(void) {
    test_default_is_bit_exact();
    test_master_volume_and_focus_mute();
    test_music_onset_reserves_only_at_first_pcm();
    test_audio_queue_does_not_replay_stale_samples();
    test_audio_clock_matches_host_elapsed_time();
    puts("host_audio_policy_test: passed");
    return 0;
}
