#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Lien radio inter-moitiés du Niphargus — brick B3.
 *
 * La moitié DROITE émet sa demi-matrice, la GAUCHE l'écoute. La droite n'a ni
 * moteur keymap ni sortie HID : la spec la décrit comme « un scanner qui
 * remonte sa matrice brute ». La gauche fusionne ce qu'elle reçoit avec son
 * propre balayage — colonnes 0-6 pour elle, 7-13 pour la droite, d'où
 * KEYMAP_COLS = 2 × MATRIX_COLS sur le maître.
 *
 * Canal 0x4F (2479 MHz), adresse KaSe.03 — voir le plan de canaux dans
 * rf_slot.h. Trame : PKT_TYPE_HEARTBEAT, qui porte déjà le bitmap de
 * demi-matrice, la jauge batterie et un numéro de séquence.
 *
 * ⚠ Ce module ne traite PAS le risque R1 — la gauche ne peut pas écouter
 * pendant qu'elle émet vers le dongle. La bascule PRX/PTX est l'étape
 * suivante ; on prouve d'abord que le lien porte, sinon un échec de bascule
 * serait indiscernable d'un lien qui ne marche pas. */

/* Émetteur — moitié droite. Initialise la radio en PTX sur le canal du lien.
 * Retourne false si la radio ne répond pas. */
bool half_link_tx_init(void);

/* Émet l'état courant de la demi-matrice. Le numéro de séquence est géré en
 * interne. Retourne true si le paquet a été acquitté par la gauche. */
bool half_link_tx_matrix(const uint8_t *bitmap);

/* Récepteur — moitié gauche. Initialise la radio en PRX et démarre la tâche
 * d'écoute, qui journalise chaque matrice reçue. */
bool half_link_rx_start(void);
