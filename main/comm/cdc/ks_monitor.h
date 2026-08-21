#ifndef KS_MONITOR_H
#define KS_MONITOR_H

#include <stdint.h>

#define KS_MONITOR_FMT   0x01
#define KS_MONITOR_SIZE  28      /* packed wire size, bytes */

/* flags bitfield */
#define KS_MON_F_HAS_RF   (1u << 0)
#define KS_MON_F_LINK_KBD   (1u << 1)
#define KS_MON_F_LINK_MOUSE   (1u << 2)
#define KS_MON_F_USB      (1u << 3)
#define KS_MON_F_BT_CONN  (1u << 4)

/* Live monitoring snapshot. RF/battery fields are 0 when !(flags & HAS_RF). */
typedef struct {
    uint8_t  fmt;           /* = KS_MONITOR_FMT */
    uint8_t  flags;
    uint32_t uptime_s;
    uint16_t heap_free_kb;
    int8_t   temp_c;        /* INT8_MIN = no sensor */
    uint8_t  layer_idx;
    uint8_t  wpm;
    uint32_t keys_total;
    uint8_t  sig_kbd, sig_mouse;
    uint16_t age_kbd_ms, age_mouse_ms;
    uint8_t  batt_kbd_dv, batt_kbd_soc, batt_kbd_chg;
    uint8_t  batt_mouse_dv, batt_mouse_soc, batt_mouse_chg;
    uint8_t  bt_slot;
} ks_monitor_t;

/* Pure: pack m into buf (>= KS_MONITOR_SIZE) as little-endian wire bytes.
 * Returns KS_MONITOR_SIZE. */
uint16_t ks_monitor_encode(uint8_t *buf, const ks_monitor_t *m);

#endif /* KS_MONITOR_H */
