/*
 * État du dongle — ce qu'il lui reste à porter.
 *
 * Ce fichier s'appelait dongle_engine_state.c et fabriquait les globales que
 * matrix_scan.c fournit sur un clavier : MATRIX_STATE, keycodes[],
 * current_press_*, stat_matrix_changed. Il n'existait que pour faire tourner le
 * moteur keymap sur des demi-matrices reçues par radio — l'architecture A.
 *
 * Le Niphargus envoie du HID déjà fini, donc le dongle ne décode plus aucune
 * matrice et ne fait plus tourner de moteur. Voir
 * docs/superpowers/specs/2026-08-19-dongle-role-niphargus-design.md
 *
 * Ne restent ici que trois choses qui ont encore un sens sur un répéteur :
 * le mode de sortie HID, la couche courante rapportée au moniteur, et le cache
 * des batteries des deux slots — la part « il sait et il rapporte » de son rôle.
 */
#include <stdint.h>
#include <stdbool.h>
#include "esp_timer.h"

/* Mode de sortie HID (0 = USB). Le dongle n'a pas de BLE, mais hid_transport.c
 * lit cette globale sur tous les rôles. */
uint8_t usb_bl_state = 0;

/* Couche courante — rapportée telle quelle au moniteur CDC. Le dongle n'ayant
 * plus de moteur, elle ne change jamais de son fait ; elle reste exposée pour
 * que le logiciel de contrôle voie un champ cohérent. */
uint8_t current_layout = 0;

/* ── Cache des batteries des deux slots ───────────────────────────────────
 * Indexé comme les slots RF : 0 = clavier, 1 = souris (comm/rf/rf_slot.h).
 * Alimenté par la trame d'état reçue de chaque appareil, lu par la commande CDC
 * BATTERY. C'est la supervision : le dongle sait et rapporte, il ne décide rien.
 *
 * La trame d'état ne porte que la tension — quatre octets était une contrainte
 * de conception, pas un oubli. L'état de charge et la charge en cours restent
 * donc à 0xFF « inconnu » plutôt que devinés. */
typedef struct {
    uint8_t  batt_dV;     /* 0xFF = inconnu */
    uint8_t  soc_pct;     /* 0xFF = inconnu */
    uint8_t  charging;    /* 0xFF = inconnu */
    uint32_t last_ms;     /* 0 = jamais vu */
} dongle_batt_t;

static dongle_batt_t s_batt[2] = {
    { 0xFF, 0xFF, 0xFF, 0 },
    { 0xFF, 0xFF, 0xFF, 0 },
};

void dongle_cache_set_battery(uint8_t slot,
                              uint8_t batt_dV, uint8_t soc_pct, uint8_t charging)
{
    if (slot > 1) return;
    s_batt[slot].batt_dV  = batt_dV;
    s_batt[slot].soc_pct  = soc_pct;
    s_batt[slot].charging = charging;
    s_batt[slot].last_ms  = (uint32_t)(esp_timer_get_time() / 1000);
}

void dongle_cache_get_battery(uint8_t slot,
                              uint8_t *batt_dV, uint8_t *soc_pct,
                              uint8_t *charging, uint32_t *age_ms_out)
{
    uint8_t s = (slot > 1) ? 1 : slot;
    *batt_dV  = s_batt[s].batt_dV;
    *soc_pct  = s_batt[s].soc_pct;
    *charging = s_batt[s].charging;
    if (s_batt[s].last_ms == 0) {
        *age_ms_out = 0xFFFFFFFFu;
    } else {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        *age_ms_out = now - s_batt[s].last_ms;
    }
}

/* ── Champs du moniteur CDC ───────────────────────────────────────────────
 * Le dongle ne compte aucune frappe : il relaie du HID déjà fini, sans jamais
 * regarder ce qu'il contient. Ces deux valeurs restent exposées, à zéro, plutôt
 * que d'amputer le format de trame du moniteur — le logiciel de contrôle lit
 * ainsi les mêmes champs quel que soit l'appareil au bout du câble. */
uint32_t key_stats_total = 0;

uint16_t wpm_get(void) { return 0; }
