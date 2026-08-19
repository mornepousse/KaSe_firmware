/* Dedup / commit state of the HID keyboard report producer.
 *
 * Split out as pure logic so the rule that matters is host-testable:
 * **a report is only ever marked as sent once the transport accepted it.**
 *
 * Why (audit F1, docs/CODE_AUDIT_2026-07-07.md): the producer suppresses a
 * report identical to the previous one. If it commits a report that was in fact
 * dropped (USB endpoint busy, queue full), the host keeps the old state while
 * the firmware believes it is in sync — and because every later attempt looks
 * like a duplicate, the correction is never retransmitted. On a key-up that
 * means a **stuck modifier** (Super held forever after a shortcut) until the
 * key is pressed again.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t  last_kc[6];   /* last COMMITTED keycodes */
    uint8_t  last_mod;     /* last COMMITTED modifier byte */
    uint32_t last_ms;      /* when it was committed */
    bool     valid;        /* false until the first commit */
} hid_dedup_t;

/* True when the report must be handed to the transport.
 * A report that differs from the last committed one ALWAYS goes out (a state
 * change must never be rate-limited). An identical one is suppressed only while
 * it is younger than min_interval_ms — that throttles repeats, nothing else. */
static inline bool hid_dedup_should_send(const hid_dedup_t *d, const uint8_t kc[6],
                                         uint8_t mod, uint32_t now_ms,
                                         uint32_t min_interval_ms)
{
    if (!d->valid) return true;
    if (mod != d->last_mod) return true;
    if (memcmp(kc, d->last_kc, 6) != 0) return true;
    return (uint32_t)(now_ms - d->last_ms) >= min_interval_ms;
}

/* Record a report as sent. MUST only be called once the transport accepted it.
 * Committing a dropped report is exactly the bug described in the header
 * comment, so callers pass the transport's success flag, never a bare "we
 * tried". */
static inline void hid_dedup_commit(hid_dedup_t *d, const uint8_t kc[6],
                                    uint8_t mod, uint32_t now_ms)
{
    memcpy(d->last_kc, kc, 6);
    d->last_mod = mod;
    d->last_ms  = now_ms;
    d->valid    = true;
}
