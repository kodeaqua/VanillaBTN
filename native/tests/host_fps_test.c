#include "host_pacing.h"
#include "sdk_sync.h"
#include "config.h"
#include <libetc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "Assertion failed at line %d: %s\n", __LINE__, #cond); \
    exit(1); \
} } while (0)

static uint64_t mock_time_ns;
static uint64_t mock_clock_now(void *ctx) { (void)ctx; return mock_time_ns; }
static void mock_clock_wait(uint64_t deadline, void *ctx) { (void)ctx; mock_time_ns = deadline; }

static int subtick_calls = 0;
static uint64_t mock_subtick(uint64_t deadline, void *ctx) {
    int *counter = (int *)ctx;
    if (counter) (*counter)++;
    return deadline;
}

static void test_pacing_config_init(void) {
    HostPacing pacing;
    DisplayConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* 1. Default / 0 = Follow monitor Hz */
    cfg.fps_limit = 0;
    host_pacing_init(&pacing, &cfg, 60.0f);
    CHECK(pacing.target_fps == 60);
    CHECK(pacing.frame_interval_ns == 1000000000ULL / 60);

    host_pacing_init(&pacing, &cfg, 144.0f);
    CHECK(pacing.target_fps == 144);
    CHECK(pacing.frame_interval_ns == 1000000000ULL / 144);

    /* 2. Unlocked = -1 */
    cfg.fps_limit = -1;
    host_pacing_init(&pacing, &cfg, 60.0f);
    CHECK(pacing.target_fps == 0);
    CHECK(pacing.frame_interval_ns == 0);

    /* 3. Explicit 30 FPS */
    cfg.fps_limit = 30;
    host_pacing_init(&pacing, &cfg, 144.0f);
    CHECK(pacing.target_fps == 30);
    CHECK(pacing.frame_interval_ns == 1000000000ULL / 30);

    /* 4. NULL safety */
    host_pacing_init(&pacing, NULL, 0.0f);
    CHECK(pacing.target_fps == 60);
}

static void test_fps_limit_30_cadence(void) {
    HostPacing pacing;
    DisplayConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.fps_limit = 30;
    host_pacing_init(&pacing, &cfg, 60.0f);

    uint64_t tick_interval_ns = 1000000000ULL / 60; /* ~16.666 ms */
    uint64_t cur_time = 0;

    /* Simulate 60 simulation ticks (1 second of authoritative game time) */
    for (int tick = 0; tick < 60; ++tick) {
        cur_time = (uint64_t)tick * tick_interval_ns;
        host_pacing_record_tick(&pacing);

        if (host_pacing_should_present(&pacing, cur_time)) {
            host_pacing_record_presentation(&pacing, cur_time);
        }
    }

    CHECK(pacing.simulation_ticks == 60);
    CHECK(pacing.frames_presented == 30);
}

static void test_fps_limit_60_cadence(void) {
    HostPacing pacing;
    DisplayConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.fps_limit = 60;
    host_pacing_init(&pacing, &cfg, 60.0f);

    uint64_t tick_interval_ns = 1000000000ULL / 60;
    for (int tick = 0; tick < 60; ++tick) {
        uint64_t cur_time = (uint64_t)tick * tick_interval_ns;
        host_pacing_record_tick(&pacing);

        if (host_pacing_should_present(&pacing, cur_time)) {
            host_pacing_record_presentation(&pacing, cur_time);
        }
    }

    CHECK(pacing.simulation_ticks == 60);
    CHECK(pacing.frames_presented == 60);
}

static void test_decoupled_high_refresh(void) {
    HostPacing pacing;
    DisplayConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.fps_limit = 120; /* 120 Hz presentation on 60 Hz simulation */
    host_pacing_init(&pacing, &cfg, 120.0f);

    uint64_t sim_interval_ns = 1000000000ULL / 60;  /* 16.666 ms */
    uint64_t pres_interval_ns = 1000000000ULL / 120; /* 8.333 ms */

    /* First frame presented at t = 0 */
    CHECK(host_pacing_should_present(&pacing, 0));
    host_pacing_record_presentation(&pacing, 0);

    /* Next simulation deadline is at 16.666 ms */
    uint64_t next_wait = host_pacing_next_wait_deadline(&pacing, 0, sim_interval_ns);
    /* Pacing should request a wake-up at 8.333 ms (the intermediate presentation deadline) */
    CHECK(next_wait == pres_interval_ns);
    CHECK(next_wait < sim_interval_ns);

    /* At t = 8.333 ms, presentation is due */
    CHECK(host_pacing_should_present(&pacing, pres_interval_ns));
    host_pacing_record_presentation(&pacing, pres_interval_ns);

    /* From t = 8.333 ms, next present is at 16.666 ms, which matches sim_interval_ns */
    next_wait = host_pacing_next_wait_deadline(&pacing, pres_interval_ns, sim_interval_ns);
    CHECK(next_wait == sim_interval_ns);
}

static void test_unlocked_pacing(void) {
    HostPacing pacing;
    DisplayConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.fps_limit = -1; /* Unlocked */
    host_pacing_init(&pacing, &cfg, 60.0f);

    CHECK(host_pacing_should_present(&pacing, 0));
    CHECK(host_pacing_should_present(&pacing, 1000));
    CHECK(host_pacing_should_present(&pacing, 2000));

    uint64_t next_wait = host_pacing_next_wait_deadline(&pacing, 100, 16666666ULL);
    CHECK(next_wait == 16666666ULL);
}

static void test_timestamp_boundaries(void) {
    HostPacing pacing;
    DisplayConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.fps_limit = 60;
    host_pacing_init(&pacing, &cfg, 60.0f);
    uint64_t interval = pacing.frame_interval_ns;
    host_pacing_record_presentation(&pacing, UINT64_MAX - interval / 2);

    /* The due timestamp cannot wrap to a small number. */
    CHECK(!host_pacing_should_present(&pacing, UINT64_MAX - interval / 2 + 1));
    CHECK(host_pacing_next_wait_deadline(&pacing,
        UINT64_MAX - interval / 2, UINT64_MAX) == UINT64_MAX);
    CHECK(host_pacing_should_present(&pacing, UINT64_MAX - 1) == false);
    /* A restarted/non-monotonic mock clock should not stall presentation. */
    CHECK(host_pacing_should_present(&pacing, 0));

    host_pacing_record_presentation(&pacing, 2000000000ULL);
    CHECK(!host_pacing_should_present(&pacing, 2000000000ULL));
    CHECK(host_pacing_should_present(&pacing, 2000000000ULL + interval));
    CHECK(host_pacing_next_wait_deadline(&pacing, 2000000000ULL,
        2000000000ULL + 2 * interval) == 2000000000ULL + interval);
}

static void test_sdk_sync_integration(void) {
    mock_time_ns = 1000000ULL;
    CHECK(sdk_sync_bind(mock_clock_now, mock_clock_wait, NULL));

    subtick_calls = 0;
    sdk_sync_subtick_hook(mock_subtick, &subtick_calls);

    /* ResetCallback initializes epoch */
    ResetCallback();

    /* Calling VSync(0) should advance by 1 nominal tick (1/60s) */
    int elapsed = VSync(0);
    CHECK(elapsed >= 0);

    /* Verify simulation clock advances monotonically */
    uint64_t t1 = mock_time_ns;
    VSync(0);
    uint64_t t2 = mock_time_ns;
    CHECK(t2 > t1);
    CHECK(subtick_calls == 0); /* Custom waits intentionally do not call SDL hooks. */

    sdk_sync_subtick_hook(NULL, NULL);
    sdk_sync_bind(NULL, NULL, NULL);
}

int main(void) {
    test_pacing_config_init();
    test_fps_limit_30_cadence();
    test_fps_limit_60_cadence();
    test_decoupled_high_refresh();
    test_unlocked_pacing();
    test_timestamp_boundaries();
    test_sdk_sync_integration();

    printf("host_fps_test: All decoupled FPS and simulation pacing tests passed successfully.\n");
    return 0;
}
