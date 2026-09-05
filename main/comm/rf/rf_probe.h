#ifndef RF_PROBE_H
#define RF_PROBE_H

/* Bring-up : la nRF24 répond-elle ?
 *
 * Configure les GPIO, ouvre le bus SPI, puis lit CONFIG et RF_SETUP via
 * rf_driver_init() — qui appelle rf_driver_probe() et échoue si la puce ne
 * répond pas. Un module absent ou mal câblé lit 0x00 ou 0xFF sur les deux
 * registres, ce que le probe rejette.
 *
 * Logue en ESP_LOGW (tag "rf_probe") pour rester visible même à niveau WARN.
 * Demande une console : CONFIG_ESP_CONSOLE_UART_DEFAULT, actif sur les deux
 * moitiés Niphargus.
 *
 * Gardé par CONFIG_KASE_NRF_PROBE (default n). Diagnostic de banc uniquement —
 * il n'installe aucune tâche et ne relaie rien. Le vrai lien radio est le
 * ressort de B3/B4 (docs/superpowers/specs/2026-08-19-niphargus-firmware-design.md).
 */
void rf_probe_run(void);

#endif /* RF_PROBE_H */
