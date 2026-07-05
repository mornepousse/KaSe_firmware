#pragma once
#include <stdbool.h>
#include <stdint.h>
bool hid_bluetooth_is_initialized(void);
bool hid_bluetooth_is_connected(void);
bool hid_bluetooth_is_pairing(void);
uint8_t bt_get_active_slot(void);
