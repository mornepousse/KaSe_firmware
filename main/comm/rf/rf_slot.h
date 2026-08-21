/* Les deux slots RF du dongle.
 *
 * Historiquement c'étaient les deux moitiés d'un même clavier, et le dongle
 * réconciliait leurs demi-matrices. Ce n'est plus le cas : la moitié maître du
 * Niphargus lui envoie du HID déjà fini, et le second slot appartient à la
 * souris Conchodytes. Les deux appareils n'ont plus rien en commun.
 *
 * D'où ce module minuscule, dont l'unique raison d'être est de rendre cette
 * séparation impossible à oublier : **la perte d'un slot ne relâche que ce que
 * ce slot tenait**. Sous l'ancienne lecture, relâcher tout le clavier à la perte
 * de l'une ou l'autre moitié était juste ; aujourd'hui ce serait un bug — une
 * souris qui sort de portée effacerait la frappe en cours.
 *
 * Le repli lui-même reste nécessaire : un lien qui se tait laisse l'hôte sur le
 * dernier rapport reçu. Si c'était « Super enfoncé », il le reste.
 *
 * Les clés NVS d'appairage gardent leurs noms d'origine (`mac_left` /
 * `mac_right`) : les renommer désapparierait le matériel déjà appairé pour un
 * gain purement cosmétique. Le slot 0x01 y désigne le clavier, le 0x02 la souris.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define RF_SLOT_KBD    0u   /* moitié maître Niphargus — envoie du HID fini */
#define RF_SLOT_MOUSE  1u   /* Conchodytes */
#define RF_SLOT_COUNT  2u

typedef enum {
    RF_SAFE_NONE = 0,
    RF_SAFE_RELEASE_KEYS,      /* rapport clavier vide */
    RF_SAFE_RELEASE_BUTTONS,   /* rapport souris à zéro */
} rf_safe_action_t;

typedef struct {
    uint32_t last_rx_ms;
    bool     up;
} rf_slot_link_t;

/* Tout paquet reçu — battement, état, rapport HID — vaut preuve de vie. */
static inline void rf_slot_link_rx(rf_slot_link_t *l, uint32_t now_ms)
{
    if (l == NULL) return;
    l->last_rx_ms = now_ms;
    l->up = true;
}

/* À appeler à chaque tour de la boucle RF. Rend l'action de repli à effectuer,
 * une seule fois par perte : la boucle tourne toutes les 10 ms, un déclenchement
 * répété noierait l'endpoint HID de rapports vides.
 *
 * La soustraction est non signée à dessein : le compteur de millisecondes
 * repasse par zéro après ~49 jours, ce qu'un dongle branché en permanence
 * atteint. */
static inline rf_safe_action_t rf_slot_link_check(rf_slot_link_t *l, uint8_t slot,
                                                  uint32_t now_ms, uint32_t timeout_ms)
{
    if (l == NULL || !l->up) return RF_SAFE_NONE;
    if ((uint32_t)(now_ms - l->last_rx_ms) < timeout_ms) return RF_SAFE_NONE;
    l->up = false;
    return (slot == RF_SLOT_MOUSE) ? RF_SAFE_RELEASE_BUTTONS : RF_SAFE_RELEASE_KEYS;
}
