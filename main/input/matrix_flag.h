/* Drapeau « la matrice a changé » — protocole producteur/consommateur.
 *
 * Producteurs : le callback de scan matrice (priorité 5, `matrix_scan.c`) sur les
 * claviers, et la réception RF (`rf_rx_task.c`) sur le dongle. Consommateur : la
 * boucle clavier (priorité 3, `keyboard_task.c`). Le producteur préempte le
 * consommateur à tout instant.
 *
 * ── La règle, et pourquoi elle est structurelle ──────────────────────────────
 *
 * Le consommateur doit effacer le drapeau AVANT de lire l'état de la matrice,
 * jamais après. Le code faisait l'inverse (audit F3) :
 *
 *     if (stat_matrix_changed == 1) {
 *         build_keycode_report();     // lit l'état — c'est long
 *         stat_matrix_changed = 0;    // efface APRÈS
 *
 * Un producteur qui tombe entre les deux pose un `1` et de nouvelles données ; le
 * `0` les efface. L'itération suivante voit `0` et saute le bloc : le front est
 * perdu, et rien ne le réémettra jamais — la touche n'arrive pas à l'hôte.
 *
 * En effaçant d'abord, le pire cas devient « relire un état déjà traité », que la
 * déduplication de `send_hid_key()` absorbe sans rien émettre. Perdre un appui,
 * non. L'échange est donc franchement favorable.
 *
 * `matrix_flag_take()` fait le test et l'effacement ensemble : l'ordre fautif
 * n'est plus exprimable au site d'appel. C'est là tout l'intérêt de passer par
 * cette fonction plutôt que de corriger deux lignes sur place.
 *
 * ── Pourquoi ni atomique ni section critique ─────────────────────────────────
 *
 * L'écriture d'un `uint8_t` est atomique sur ESP32 : aucun mot ne peut être vu à
 * moitié écrit. Le seul entrelacement qui nous menaçait était producteur-entre-
 * lecture-et-effacement, et l'ordre suffit à le neutraliser. Une section critique
 * coûterait de la latence sur le chemin de frappe sans rien apporter de plus.
 *
 * Reste une course théorique : un producteur qui pose `1` exactement pendant que
 * `matrix_flag_take()` écrit `0` verrait son signal perdu. La fenêtre est de
 * quelques instructions, contre plusieurs centaines de microsecondes pour la
 * lecture complète de l'état — et le producteur suivant, une milliseconde plus
 * tard, reposera le drapeau. C'est la différence entre un front perdu à chaque
 * frappe rapide et un front perdu jamais observé.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Producteur : signale que l'état de la matrice a changé. */
static inline void matrix_flag_signal(volatile uint8_t *flag)
{
    *flag = 1;
}

/* Consommateur : test-and-clear. Renvoie true s'il y a du travail, et le drapeau
 * est alors déjà remis à zéro — un producteur qui arrive pendant la lecture qui
 * suit sera donc vu au tour d'après. */
static inline bool matrix_flag_take(volatile uint8_t *flag)
{
    if (*flag == 0) return false;
    *flag = 0;
    return true;
}
