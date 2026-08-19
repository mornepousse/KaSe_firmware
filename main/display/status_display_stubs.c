/*
 * Bouchons status_display_* — rôle clavier SANS écran (Niphargus gauche,
 * KASE_NIPHAR_MASTER : CONFIG_KASE_HAS_DISPLAY=n via son override Kconfig,
 * cf. main/Kconfig.projbuild).
 *
 * Les appels status_display_* sont dispersés dans le chemin de frappe
 * (main/input/matrix_scan.c, main/input/keyboard_actions.c) et dans
 * main/comm/cdc/cdc_binary_cmds.c, sans gate CONFIG_KASE_HAS_DISPLAY —
 * ces trois fichiers sont compilés pour tout rôle KEYBOARD (ou pour tous
 * les rôles), écran ou pas. Y semer des #if CONFIG_KASE_HAS_DISPLAY
 * laisserait du bruit permanent dans du code chaud pour un seul cas de
 * board. Ce fichier de bouchons centralise le problème à la place : il se
 * supprime d'un bloc le jour où l'architecture display change (ou si
 * Niphargus gauche gagne un écran).
 *
 * Précédent dans ce dépôt : main/comm/cdc/cdc_dongle_stubs.c bouchonne déjà
 * status_display_update_layer_name() pour CONFIG_KASE_DEVICE_ROLE_DONGLE,
 * pour la même raison. Rôles KEYBOARD et DONGLE sont mutuellement exclusifs
 * (choice KASE_DEVICE_ROLE dans main/Kconfig.projbuild) : les deux fichiers
 * de bouchons ne sont jamais compilés ensemble, donc pas de double
 * définition possible.
 *
 * Compilé uniquement quand CONFIG_KASE_DEVICE_ROLE_KEYBOARD &&
 * !CONFIG_KASE_HAS_DISPLAY (main/CMakeLists.txt). Bouchons silencieux :
 * status_display_notify_keypress() est appelé à chaque frappe depuis
 * matrix_scan.c (chemin chaud) — pas de ESP_LOGx ici.
 */

void status_display_update_layer_name(void) { }
void status_display_notify_keypress(void) { }
void status_display_refresh_all(void) { }
void status_display_notify_display_key(void) { }
