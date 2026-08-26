/*
 * Bouchons propres au rôle souris (Conchodytes).
 *
 * cdc_binary_cmds.c compile inconditionnellement `bin_cmd_monitor`, qui
 * rapporte le compte de frappes et les mots par minute. Ces deux grandeurs
 * viennent de input/key_stats.c et input/key_features.c, qu'une souris ne
 * compile pas — elle n'a pas de touches.
 *
 * La souris partage déjà cdc_niphar_slave_stubs.c pour tout ce qui manque des
 * deux côtés (affichage, BLE, moteur keymap). Ces deux symboles-ci ne peuvent
 * PAS y aller : l'esclave Niphargus a une matrice et les définit pour de vrai,
 * ce qui donnerait un doublon au lien. D'où ce fichier séparé, compilé pour le
 * seul rôle souris.
 *
 * Modelé sur comm/cdc/cdc_dongle_stubs.c : on ne fournit que les symboles que
 * l'éditeur de liens a réellement réclamés, ajoutés un par un.
 */

#include <stdint.h>

/* Aucune frappe n'est jamais comptée sur une souris. Les commandes CDC qui
 * lisent cette valeur rapporteront zéro — ce qui est la vérité, pas un
 * bouchon qui ment. */
uint32_t key_stats_total = 0;

/* Idem : pas de mots par minute sans touches. Même choix que le dongle, qui
 * rend 0 depuis comm/rf/dongle_state.c. */
uint16_t wpm_get(void) { return 0; }
