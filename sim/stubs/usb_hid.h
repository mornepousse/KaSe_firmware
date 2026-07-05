#pragma once
#include <stdint.h>
#define HID_LED_CAPS_LOCK 0x02
extern uint8_t hid_led_state;
uint8_t keyboard_get_usb_bl_state(void);
