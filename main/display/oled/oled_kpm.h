#pragma once
/* oled_kpm.h — Fenêtre glissante KPM (keys per minute) partagée entre
 * l'écran HOME (barre KPM) et l'écran STATS (sparkline).
 *
 * Modèle : fenêtre circulaire de OLED_KPM_WINDOW secondes.  Chaque seconde,
 * oled_kpm_tick() pousse le compteur de frappes dans l'historique et le
 * remet à zéro.  oled_kpm_value() renvoie la somme des 60 dernières
 * secondes (= KPM sur la minute glissante).
 */
#include <stdint.h>

#define OLED_KPM_WINDOW     60u    /* secondes dans la fenêtre glissante */
#define OLED_KPM_SAMPLE_MS  1000u  /* durée d'un échantillon (ms) */
#define OLED_KPM_MAX        400u   /* plafond pour la barre / sparkline */

/* Remet tout à zéro (appelé depuis oled_screens_reset). */
void oled_kpm_reset(void);

/* Incrémente le compteur courant (appelé sur chaque frappe). */
void oled_kpm_notify_keypress(void);

/* Avance l'horloge de la fenêtre (appelé depuis oled_screens_tick).
 * Ne fait rien si moins d'une seconde s'est écoulée depuis le dernier tick. */
void oled_kpm_tick(uint32_t now_ms);

/* Renvoie le KPM courant (somme des OLED_KPM_WINDOW derniers échantillons). */
uint32_t oled_kpm_value(void);

/* Renvoie un pointeur vers l'historique circulaire brut (OLED_KPM_WINDOW entrées).
 * Utile pour la sparkline dans l'écran STATS.  Lecture seule ; valide jusqu'au
 * prochain appel à oled_kpm_reset(). */
const uint32_t *oled_kpm_history(void);
