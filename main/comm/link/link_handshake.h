/* Poignée de main du lien inter-moitiés Niphargus.
 *
 * LINK_5V_EN pilote un load switch SiP32431 avec un pull-down 100 k : le 5 V est
 * MORT par défaut. Émetteur et récepteur doivent tous deux fermer leur switch
 * pour qu'un courant passe, donc un branchement à chaud ne peut pas produire
 * d'étincelle. Une moitié à batterie vide n'est pas réveillable par le TRRS —
 * assumé au design matériel.
 *
 * Logique pure : l'horloge et les événements sont passés en argument, aucune
 * GPIO n'est touchée ici. L'appelant traduit les actions en gpio_set_level().
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define LINK_HS_PROBE_TIMEOUT_MS  200   /* attente d'un ACK après la sonde */
#define LINK_HS_PEER_TIMEOUT_MS   500   /* silence du pair toléré, lien établi */

typedef enum {
    LINK_HS_IDLE = 0,   /* 5 V mort, rien en cours */
    LINK_HS_PROBING,    /* sonde envoyée, on attend l'ACK */
    LINK_HS_UP,         /* pair reconnu, 5 V fermé */
} link_hs_state_t;

typedef enum {
    LINK_HS_EV_TICK = 0,     /* passage du temps, rien d'autre */
    LINK_HS_EV_USB_PRESENT,  /* le câble hôte vient d'apparaître */
    LINK_HS_EV_USB_GONE,     /* le câble hôte a disparu */
    LINK_HS_EV_PEER_ACK,     /* la moitié d'en face a répondu à la sonde */
    LINK_HS_EV_PEER_FRAME,   /* trame valide reçue du pair (garde le lien vivant) */
} link_hs_event_t;

typedef enum {
    LINK_HS_ACT_NONE = 0,
    LINK_HS_ACT_SEND_PROBE,   /* émettre la sonde « t'es bien ma moitié ? » */
    LINK_HS_ACT_ENABLE_5V,    /* fermer le load switch */
    LINK_HS_ACT_DISABLE_5V,   /* rouvrir le load switch */
} link_hs_action_t;

typedef struct {
    link_hs_state_t state;
    uint32_t        since_ms;    /* entrée dans l'état courant */
    uint32_t        last_peer_ms;/* dernier signe de vie du pair */
    bool            usb;         /* câble hôte présent */
    bool            en_5v;       /* état commandé du load switch */
} link_hs_t;

static inline void link_hs_init(link_hs_t *h)
{
    h->state = LINK_HS_IDLE;
    h->since_ms = 0;
    h->last_peer_ms = 0;
    h->usb = false;
    h->en_5v = false;
}

static inline bool link_hs_5v_enabled(const link_hs_t *h) { return h->en_5v; }

static inline link_hs_action_t link_hs_step(link_hs_t *h, link_hs_event_t ev,
                                            uint32_t now_ms)
{
    /* Perte de l'USB : on coupe partout, immédiatement. */
    if (ev == LINK_HS_EV_USB_GONE) {
        h->usb = false;
        if (h->en_5v) {
            h->en_5v = false;
            h->state = LINK_HS_IDLE;
            h->since_ms = now_ms;
            return LINK_HS_ACT_DISABLE_5V;
        }
        h->state = LINK_HS_IDLE;
        h->since_ms = now_ms;
        return LINK_HS_ACT_NONE;
    }

    if (ev == LINK_HS_EV_USB_PRESENT) h->usb = true;
    if (ev == LINK_HS_EV_PEER_ACK || ev == LINK_HS_EV_PEER_FRAME)
        h->last_peer_ms = now_ms;

    switch (h->state) {
    case LINK_HS_IDLE:
        /* On ne sonde que si on a du courant à donner. */
        if (ev == LINK_HS_EV_USB_PRESENT) {
            h->state = LINK_HS_PROBING;
            h->since_ms = now_ms;
            return LINK_HS_ACT_SEND_PROBE;
        }
        return LINK_HS_ACT_NONE;

    case LINK_HS_PROBING:
        if (ev == LINK_HS_EV_PEER_ACK) {
            h->state = LINK_HS_UP;
            h->since_ms = now_ms;
            h->en_5v = true;
            return LINK_HS_ACT_ENABLE_5V;
        }
        if ((uint32_t)(now_ms - h->since_ms) >= LINK_HS_PROBE_TIMEOUT_MS) {
            h->state = LINK_HS_IDLE;
            h->since_ms = now_ms;
        }
        return LINK_HS_ACT_NONE;

    case LINK_HS_UP:
        if ((uint32_t)(now_ms - h->last_peer_ms) >= LINK_HS_PEER_TIMEOUT_MS) {
            h->en_5v = false;
            h->state = LINK_HS_IDLE;
            h->since_ms = now_ms;
            return LINK_HS_ACT_DISABLE_5V;
        }
        return LINK_HS_ACT_NONE;

    default:
        /* h->state hors énumération : contrat d'appel violé (struct sur pile
         * non passée par link_hs_init(), mémoire arbitraire). Ce module est le
         * dernier rempart avant le GPIO du load switch, et link_hs_5v_enabled()
         * est public — un appelant qui l'interroge dans cet état ne doit
         * jamais lire un `true` de poubelle alors qu'aucun ACT_ENABLE_5V n'a
         * été émis. Ici, la propriété « 5 V éteint sauf preuve du contraire »
         * prime sur le diagnostic : on retombe du côté sûr plutôt que de
         * préserver une valeur inconnue.
         */
        h->state = LINK_HS_IDLE;
        h->en_5v = false;
        return LINK_HS_ACT_NONE;
    }
    return LINK_HS_ACT_NONE;
}
