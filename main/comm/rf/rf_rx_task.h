#ifndef RF_RX_TASK_H
#define RF_RX_TASK_H

#include <stdint.h>
#include <stdbool.h>

/* Start RF radios + rx task. Returns false if neither radio is present. */
bool rf_rx_start(void);

/* Diagnostic de lien exposé par CDC.
 *
 * Les deux slots ne sont plus deux moitiés d'un même clavier : le premier porte
 * le clavier (moitié maître Niphargus), le second la souris Conchodytes. Voir
 * comm/rf/rf_slot.h. */
typedef struct {
    bool link_kbd, link_mouse;
    /* Âge du dernier paquet reçu, quel qu'il soit — pas seulement des battements :
     * un lien actif n'en envoie plus. */
    uint32_t age_kbd_ms, age_mouse_ms;
    uint32_t pkt_rx_kbd, pkt_rx_mouse;
    uint32_t pkt_dup_kbd, pkt_dup_mouse;
    /* Dernier link_q annoncé par chaque slot. 0 si rien reçu encore
     * (conservateur = meilleur score de retransmission). */
    uint8_t link_q_kbd;
    uint8_t link_q_mouse;
} rf_link_status_t;

void rf_rx_get_status(rf_link_status_t *out);

/* Copie les MAC WiFi appairées des deux slots (copie vivante : chargée au boot,
 * rafraîchie à chaque appairage réussi), pour ne jamais dépendre d'un cache NVS
 * périmé. MAC toute à zéro = ce slot n'est pas appairé. */
void rf_rx_copy_peer_macs(uint8_t mac_kbd[6], uint8_t mac_mouse[6]);

/* Signal quality derivation — pure function, host-testable.
 * Returns 0..255 link quality (255 = best, 0 = link down/timed out).
 * See rf_rx_task.c for the age/retry mapping. */
uint8_t rf_signal_q255(bool link_up, uint32_t hb_age_ms, uint8_t link_q);

/* Begin a pairing window (called from the CDC KS_CMD_RF_PAIR_START handler).
 * reset=1 → efface d'abord les MAC appairées et paired_count en NVS. Bascule la
 * radio 1 sur le rendez-vous d'appairage (RF_PAIR_ADDR/RF_PAIR_CHANNEL) en PRX et
 * ouvre une fenêtre pilotée par rf_rx_task. Rend le set_id calculé et le
 * paired_count courant. */
bool rf_rx_pair_start(uint8_t reset, uint16_t *set_id_out, uint8_t *paired_count_out);

#endif /* RF_RX_TASK_H */
