/*
 * Stubs for symbols that cdc_binary_cmds.c references but are not compiled in
 * the Niphargus right-half (slave) role: no display backend, no BLE.
 *
 * Compiled only when CONFIG_KASE_DEVICE_ROLE_NIPHAR_SLAVE=y. Provides
 * no-op implementations so the link succeeds. The corresponding KS_CMD_*
 * commands are inert on this board — the right half has no CDC config role
 * (it is a matrix/display scanner, not the USB endpoint).
 *
 * Modeled on comm/cdc/cdc_dongle_stubs.c: only the symbols the linker
 * actually demanded are provided here, added one at a time.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Engine input-state globals. On the KEYBOARD role these are defined in
 * input/matrix_scan.c (KASE_HAS_LOCAL_MATRIX); on the dongle role, in
 * comm/rf/dongle_engine_state.c. The Niphargus right half compiles neither
 * (no keymap engine, no RF RX stack) but cdc_binary_cmds.c references them
 * unconditionally, so they need a home here. */
volatile bool matrix_test_mode = false;
volatile uint32_t matrix_test_last_activity_ms = 0;
uint8_t current_layout = 0;
void layer_changed(void) { /* no display/link to notify on this role yet */ }

/* ── status_display ─────────────────────────────────────────── */
void status_display_update_layer_name(void) { /* no display backend on this role */ }
