/* HID report dedup / commit — audit F1 (stuck modifier after a dropped key-up).
 *
 * The behaviour under test is not "does it deduplicate": it is **what happens
 * when the transport refuses the report**. The producer must not record it as
 * sent, so the very next attempt still goes out. Committing unconditionally is
 * what turns a single dropped key-up into a modifier stuck forever on the host.
 */
#include "test_framework.h"
#include "../main/input/hid_dedup.h"

#define MIN_MS 8

/* Modifier byte for Left GUI (Super) — bit 3, i.e. HID 0xE3. */
#define MOD_LGUI 0x08

static const uint8_t KC_NONE[6] = {0, 0, 0, 0, 0, 0};

static void test_first_report_always_sent(void)
{
    hid_dedup_t d = {0};
    TEST_ASSERT(hid_dedup_should_send(&d, KC_NONE, 0, 0, MIN_MS),
                "virgin state: nothing was ever committed, so send");
}

static void test_state_change_is_never_rate_limited(void)
{
    hid_dedup_t d = {0};
    hid_dedup_commit(&d, KC_NONE, MOD_LGUI, 100);       /* Super down @100 */

    /* Super up 1 ms later: a different report, well inside the 8 ms window.
     * It must still be sent — rate-limiting a state change would itself stick
     * the modifier. */
    TEST_ASSERT(hid_dedup_should_send(&d, KC_NONE, 0, 101, MIN_MS),
                "modifier release inside the window is still sent");

    /* Same for a keycode change with an unchanged modifier. */
    const uint8_t kc_k[6] = {0x0E, 0, 0, 0, 0, 0};
    TEST_ASSERT(hid_dedup_should_send(&d, kc_k, MOD_LGUI, 101, MIN_MS),
                "keycode change inside the window is still sent");
}

static void test_identical_report_is_throttled_then_released(void)
{
    hid_dedup_t d = {0};
    const uint8_t kc_k[6] = {0x0E, 0, 0, 0, 0, 0};
    hid_dedup_commit(&d, kc_k, MOD_LGUI, 1000);

    TEST_ASSERT(!hid_dedup_should_send(&d, kc_k, MOD_LGUI, 1000, MIN_MS),
                "identical at t+0: throttled");
    TEST_ASSERT(!hid_dedup_should_send(&d, kc_k, MOD_LGUI, 1007, MIN_MS),
                "identical at t+7ms: still throttled");
    TEST_ASSERT(hid_dedup_should_send(&d, kc_k, MOD_LGUI, 1008, MIN_MS),
                "identical at t+8ms: window elapsed, sent");
}

/* THE regression test for F1. */
static void test_dropped_report_is_retried_not_swallowed(void)
{
    hid_dedup_t d = {0};

    /* Super pressed, accepted by the transport → committed. */
    hid_dedup_commit(&d, KC_NONE, MOD_LGUI, 500);

    /* Super released. The report is built and offered to the transport... */
    TEST_ASSERT(hid_dedup_should_send(&d, KC_NONE, 0, 501, MIN_MS),
                "release is offered to the transport");

    /* ...and the transport DROPS it (USB endpoint busy / queue full).
     * No commit happens — that is the whole contract. */

    /* Next cycle, 1 ms later, same release report. It must go out again:
     * the host still has Super down. Committing on the failed attempt would
     * make this look like a duplicate inside the 8 ms window and swallow it. */
    TEST_ASSERT(hid_dedup_should_send(&d, KC_NONE, 0, 502, MIN_MS),
                "dropped release is re-offered on the next cycle");

    /* And it stays offered for as long as it keeps failing, however fast the
     * cycles come — the state on the host has not changed. */
    TEST_ASSERT(hid_dedup_should_send(&d, KC_NONE, 0, 502, MIN_MS),
                "still offered on a same-millisecond retry");

    /* Once it finally lands, the throttle applies again. */
    hid_dedup_commit(&d, KC_NONE, 0, 503);
    TEST_ASSERT(!hid_dedup_should_send(&d, KC_NONE, 0, 504, MIN_MS),
                "after a successful send, identical reports throttle again");
}

static void test_commit_records_the_full_report(void)
{
    hid_dedup_t d = {0};
    const uint8_t kc_a[6] = {0x04, 0, 0, 0, 0, 0};
    const uint8_t kc_b[6] = {0x05, 0, 0, 0, 0, 0};

    hid_dedup_commit(&d, kc_a, MOD_LGUI, 10);
    TEST_ASSERT(!hid_dedup_should_send(&d, kc_a, MOD_LGUI, 11, MIN_MS),
                "committed report is recognised as identical");
    TEST_ASSERT(hid_dedup_should_send(&d, kc_b, MOD_LGUI, 11, MIN_MS),
                "a different keycode is not confused with it");
    TEST_ASSERT(hid_dedup_should_send(&d, kc_a, 0, 11, MIN_MS),
                "a different modifier is not confused with it");
}

static void test_clock_wraps_do_not_stick_the_throttle(void)
{
    hid_dedup_t d = {0};
    /* Commit just before the 32-bit millisecond counter wraps. */
    hid_dedup_commit(&d, KC_NONE, MOD_LGUI, 0xFFFFFFFCu);
    /* 8 ms later the counter has wrapped to 4. Unsigned arithmetic must still
     * see 8 ms of elapsed time, not a huge negative one. */
    TEST_ASSERT(hid_dedup_should_send(&d, KC_NONE, MOD_LGUI, 4u, MIN_MS),
                "throttle releases across a counter wrap");
}

void test_hid_dedup(void)
{
    printf("\n-- hid_dedup (F1: dropped report must not be committed) --\n");
    test_first_report_always_sent();
    test_state_change_is_never_rate_limited();
    test_identical_report_is_throttled_then_released();
    test_dropped_report_is_retried_not_swallowed();
    test_commit_records_the_full_report();
    test_clock_wraps_do_not_stick_the_throttle();
}
