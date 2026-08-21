#include "ks_monitor.h"

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

uint16_t ks_monitor_encode(uint8_t *buf, const ks_monitor_t *m)
{
    buf[0]  = m->fmt;
    buf[1]  = m->flags;
    put_u32(&buf[2], m->uptime_s);
    put_u16(&buf[6], m->heap_free_kb);
    buf[8]  = (uint8_t)m->temp_c;
    buf[9]  = m->layer_idx;
    buf[10] = m->wpm;
    put_u32(&buf[11], m->keys_total);
    buf[15] = m->sig_kbd;
    buf[16] = m->sig_mouse;
    put_u16(&buf[17], m->age_kbd_ms);
    put_u16(&buf[19], m->age_mouse_ms);
    buf[21] = m->batt_kbd_dv; buf[22] = m->batt_kbd_soc; buf[23] = m->batt_kbd_chg;
    buf[24] = m->batt_mouse_dv; buf[25] = m->batt_mouse_soc; buf[26] = m->batt_mouse_chg;
    buf[27] = m->bt_slot;
    return KS_MONITOR_SIZE;
}
