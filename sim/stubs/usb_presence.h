#pragma once
typedef enum { KBD_OUT_USB, KBD_OUT_RF } kbd_route_t;
kbd_route_t kbd_active_route(void);
