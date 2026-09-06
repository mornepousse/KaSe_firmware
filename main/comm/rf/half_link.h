#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "rf_packet.h"   /* bitmap de demi-matrice + ses accesseurs */

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

/* ── État de la moitié distante, vu par le maître (logique pure) ────────────
 *
 * Testée host dans test/test_half_state.c, sur le modèle des inlines de
 * comm/usb/usb_presence.h.
 *
 * L'état est ABSOLU, pas différentiel : chaque trame porte la matrice entière,
 * donc une trame perdue se rattrape à la suivante sans accumulation ni dérive.
 * C'est ce qui rend le lien tolérant aux pertes que R1 mesure.
 *
 * ⚠ Le repli sur silence est la partie dangereuse. Une moitié qui sort de
 * portée ou dont la pile meurt laisserait l'hôte sur le dernier état reçu — et
 * si c'était « Maj enfoncée », il le reste. On relâche donc au bout d'un
 * silence, mais UNIQUEMENT ce que cette moitié tenait : rf_slot.h prévient
 * qu'un relâchement mal ciblé serait pire que le mal. */
typedef struct {
    uint8_t  bitmap[RF_HALF_BITMAP_BYTES];  /* dernier état reçu */
    uint32_t derniere_ms;                   /* quand il l'a été */
    bool     vivant;                        /* false = silence déjà constaté */
} half_state_t;

/* Une trame vient d'arriver : elle remplace l'état précédent. */
static inline void half_state_recu(half_state_t *st, const uint8_t *bitmap,
                                   uint32_t now_ms)
{
    memcpy(st->bitmap, bitmap, RF_HALF_BITMAP_BYTES);
    st->derniere_ms = now_ms;
    st->vivant = true;
}

/* Appelé périodiquement. Retourne true UNE SEULE FOIS, au moment où le silence
 * dépasse le délai : c'est le signal « relâche ce que cette moitié tenait ».
 * Les appels suivants retournent false tant qu'aucune trame n'est revenue —
 * sinon le moteur relâcherait à chaque cycle des touches déjà relâchées. */
static inline bool half_state_timeout(half_state_t *st, uint32_t now_ms,
                                      uint32_t delai_ms)
{
    if (!st->vivant) return false;
    if ((uint32_t)(now_ms - st->derniere_ms) < delai_ms) return false;
    memset(st->bitmap, 0, RF_HALF_BITMAP_BYTES);
    st->vivant = false;
    return true;
}

/* La touche (row, col) de la moitié distante est-elle enfoncée ? Coordonnées
 * LOCALES à cette moitié ; le décalage vers les colonnes 7-13 de la keymap est
 * la responsabilité de l'appelant. */
static inline bool half_state_pressed(const half_state_t *st, uint8_t row,
                                      uint8_t col)
{
    return rf_bitmap_get(st->bitmap, row, col);
}

/* Émetteur — moitié droite. Initialise la radio en PTX sur le canal du lien.
 * Retourne false si la radio ne répond pas. */
bool half_link_tx_init(void);

/* Émet l'état courant de la demi-matrice. Le numéro de séquence est géré en
 * interne. Retourne true si le paquet a été acquitté par la gauche. */
bool half_link_tx_matrix(const uint8_t *bitmap);

/* La touche (row, col) de la moitié DISTANTE est-elle enfoncée ? Coordonnées
 * locales à cette moitié — l'appelant décale vers les colonnes 7-13. Retourne
 * toujours false si le lien n'est pas compilé ou s'est tu. */
bool half_link_remote_pressed(uint8_t row, uint8_t col);

/* L'état distant a-t-il changé depuis le dernier appel ? Retourne true UNE
 * fois par changement, et consomme le drapeau. Permet à la tâche clavier de
 * n'agir que sur du nouveau, sans jamais interférer avec le chemin local qui a
 * sa propre émission. */
bool half_link_remote_changed(void);

/* Récepteur — moitié gauche. Initialise la radio en PRX et démarre la tâche
 * d'écoute, qui journalise chaque matrice reçue. */
bool half_link_rx_start(void);
