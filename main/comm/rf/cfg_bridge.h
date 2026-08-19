#pragma once
#include <stdint.h>
#include <stdbool.h>

/* True if a KS command must be handled by the dongle itself; false if it is
 * config-class and must be forwarded to the paired smart keyboard.
 *
 * Pure function — no side effects, no ESP-IDF dependencies. */
bool cfg_is_dongle_local(uint8_t cmd_id);

/* -------------------------------------------------------------------------
 * KS-frame chunking for ESP-NOW (max payload ~250 bytes)
 * ----------------------------------------------------------------------- */

#define CFG_CHUNK_PAYLOAD 200          /* KS-frame bytes per ESP-NOW chunk */
/* Max reassembled KS/KR frame. Must hold the largest forwarded response — the
 * board layout JSON is ~4.5KB (GET_LAYOUT_JSON), so 512 was far too small and
 * silently dropped it (cfg_chunk_count returns 0 > CFG_FRAME_MAX). 6144 = 31
 * chunks (< 256 idx limit) with headroom. Sizes the reasm + redirect buffers. */
#define CFG_FRAME_MAX     6144
#define CFG_CHUNK_HDR     2            /* [idx][total]                      */

/* How many chunks a frame of frame_len bytes needs (>=1).
 * Returns 0 if frame_len==0 or frame_len > CFG_FRAME_MAX. */
uint8_t cfg_chunk_count(uint16_t frame_len);

/* Build chunk number `idx` of `frame` into out
 * (out must hold CFG_CHUNK_HDR + CFG_CHUNK_PAYLOAD bytes).
 * Layout: out[0]=idx, out[1]=total, out[2..]=slice.
 * Returns the total out length, or 0 on bad args. */
uint16_t cfg_chunk_make(const uint8_t *frame, uint16_t frame_len,
                        uint8_t idx, uint8_t *out);

/* Reassembly accumulator. Zero-initialize before first use. */
typedef struct {
    uint8_t  total;          /* expected chunk count (0 = not yet known)       */
    uint8_t  got;            /* distinct chunks received so far                 */
    uint8_t  seen[256 / 8]; /* bitmap of received idx (256 bits, 32 bytes)     */
    uint16_t len;            /* bytes written so far (high-water mark)          */
    uint8_t  buf[CFG_FRAME_MAX]; /* reassembled frame                          */
} cfg_reasm_t;

/* Feed one chunk (chunk[0]=idx, chunk[1]=total, chunk[2..]=slice).
 * On the chunk that completes the frame, returns true and sets *out_len to
 * the full frame length (frame data available in st->buf).
 * Returns false while incomplete or on a malformed / inconsistent chunk.
 * Idempotent on duplicates (duplicate → false, no state change). */
bool cfg_reasm_add(cfg_reasm_t *st, const uint8_t *chunk,
                   uint16_t chunk_len, uint16_t *out_len);

/* -------------------------------------------------------------------------
 * Networking bridge — the ESP-NOW transport went away with the half firmware
 * (the split halves are redesigned in the Niphargus repo). The chunk/reassembly
 * helpers above are pure and stay; only these two dongle-side predicates remain,
 * as no-ops, because the CDC dispatcher calls them on every frame.
 * ----------------------------------------------------------------------- */
#ifndef TEST_HOST
#if CONFIG_KASE_DEVICE_ROLE_DONGLE
/* Always false: no transport to a remote keyboard exists. */
bool cfg_bridge_have_smart_kbd(void);

/* No-op: kept so cdc_binary_protocol.c links unchanged. */
void cfg_bridge_forward_frame(const uint8_t *frame, uint16_t len);
#endif /* CONFIG_KASE_DEVICE_ROLE_DONGLE */
#endif /* !TEST_HOST */
