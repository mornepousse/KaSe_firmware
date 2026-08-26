/* Décodage des clics SPDT de la souris Conchodytes.
 *
 * Volontairement PUR : des niveaux en entrée, un état en sortie, aucune
 * dépendance ESP-IDF. La lecture des GPIO vit dans app/mouse_task.c. C'est ce
 * qui rend ce raisonnement testable sur l'hôte (test/test_mouse_input.c), y
 * compris sur les cas que le banc ne produit pas à la demande.
 *
 * Le montage : COM à la masse, NO et NC tirés chacun au 3,3 V par 10 k
 * (R105-R107 et R108-R110 sur la carte). Le firmware lit LES DEUX contacts du
 * même bouton, et c'est cet appariement qui supprime le rebond.
 *
 * ⚠ Croiser les paires — le NO d'un bouton avec le NC d'un autre — donne un
 * firmware qui semble marcher et produit des clics fantômes. Le brochage est
 * verrouillé par test/test_conchodytes_pins.c pour cette raison.
 */
#pragma once
#include <stdbool.h>

typedef enum {
    /* NO haut, NC bas : le contact mobile est collé sur NC. */
    MOUSE_CONTACT_RELEASED = 0,
    /* NO bas, NC haut : le contact mobile est collé sur NO. */
    MOUSE_CONTACT_PRESSED,
    /* Les deux hauts : le contact mobile n'est collé sur RIEN. C'est la
     * fenêtre de rebond, et elle est OBSERVABLE — c'est tout l'intérêt du
     * SPDT sur un simple interrupteur.
     *
     * ⚠ Sa DURÉE n'est pas mesurée. La campagne du 2026-08-25 a été faite en
     * croyant scruter à 1 kHz, alors que CONFIG_FREERTOS_HZ vaut 100 par
     * défaut : vTaskDelay(1) donnait 10 ms, pas 1. Sur 24 fronts, seuls 4
     * échantillons ambigus ont été vus — la plupart des rebonds sont donc
     * tombés ENTRE deux mesures, ce qui borne leur durée à moins de 10 ms
     * sans la chiffrer. Ce qui EST établi : zéro front parasite sur ces
     * 24 transitions. À reprendre avec un tick à 1 kHz. */
    MOUSE_CONTACT_BOUNCING,
    /* Les deux bas : les deux contacts fermés en même temps, ce qu'un
     * inverseur ne fait pas. Cablâge douteux ou broche en court. */
    MOUSE_CONTACT_IMPOSSIBLE,
} mouse_contact_t;

/* Lit l'état du contact à partir des deux niveaux, sans mémoire. */
mouse_contact_t mouse_contact_decode(int no_level, int nc_level);

/* Rend l'état suivant du bouton. Sur BOUNCING comme sur IMPOSSIBLE on garde
 * `prev` : ne rien conclure vaut mieux qu'inventer un front. C'est là, et
 * nulle part ailleurs, que se joue l'anti-rebond — pas de compteur, pas de
 * délai, pas de constante à régler. */
bool mouse_button_next(bool prev, mouse_contact_t contact);
