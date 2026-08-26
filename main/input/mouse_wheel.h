/* Décodage en quadrature de la molette Conchodytes.
 *
 * L'encodeur est optique : LD1 éclaire une roue à 60 fentes, LQ1 (double
 * phototransistor) rend deux voies déphasées d'un quart de période. L'état est
 * `(A << 1) | B`, et un pas valide ne fait changer QU'UNE voie à la fois.
 *
 * Pur, sans dépendance ESP-IDF : la lecture des GPIO vit dans
 * app/mouse_task.c. Testé sur l'hôte par test/test_mouse_input.c.
 *
 * ⚠ CE DÉCODAGE N'A JAMAIS TOURNÉ SUR SILICIUM, et ne le pourra pas sur la v1 :
 * LQ1 y est câblé à l'envers (alimentation et masse inversées), et surtout il
 * n'expose qu'UNE sortie DATA là où la quadrature en demande deux. Voir
 * Conchodytes/NOTES-V2.md §1bis. Ce fichier reste juste dans son domaine — il
 * décode une quadrature correcte — mais le matériel v1 ne lui en fournira
 * jamais. À reconfronter au schéma v2 quand celui-ci sera tranché.
 */
#pragma once
#include <stdint.h>

/* Rend +1, -1, ou 0. Le 0 couvre deux cas distincts : aucun changement, et
 * transition impossible (les deux voies changées dans le même intervalle,
 * donc un pas raté). Rendre 0 plutôt qu'un sens deviné est délibéré — se
 * tromper de sens fait reculer la page, ne rien faire ne coûte qu'un cran. */
int8_t mouse_wheel_step(uint8_t prev_ab, uint8_t cur_ab);
