# Audit fiabilité clavier — 2026-07-07

Focus **fiabilité du clavier en tant que clavier** : scan → traitement → HID, +
stabilité/boot. Le wireless (lien RF) est hors périmètre (choix utilisateur).

**Méthode** : 3 audits en parallèle (scan→HID, logique de traitement, stabilité/
boot/hot-path), chacun croisé avec l'audit précédent [`CODE_AUDIT_2026-07-04.md`]
pour ne pas redoubler du déjà-corrigé. Findings recoupés ensuite dans le code.
`✅ code` = vérifié ligne par ligne moi-même ; `agent` = raisonnement d'agent
détaillé, à re-vérifier au moment du fix.

Légende sévérité : 🔴 CRITIQUE · 🟠 ÉLEVÉ · 🟡 MOYEN · ⚪ FAIBLE.

---

## 1. Chemin scan → sortie HID (synchronisation + envoi)

### 🟠 F1 — Report HID jeté si l'endpoint USB est occupé, sans retry → touche répétée `✅ code`
`main/comm/hid_transport.c:18,27` · `main/input/hid_report.c` (sender)
`send_usb_keyboard` fait `if (tud_hid_ready()) tud_hid_keyboard_report(...)`. En
Full-Speed l'EP IN reste busy jusqu'au prochain SOF (≤1 ms). Le sender draine la
queue en boucle serrée : s'il y a ≥2 messages, le 2e part alors que l'EP est busy
→ `tud_hid_ready()==false` → **report jeté, aucun retry, aucun ré-essai**.
**Scénario** : relâchement quasi-simultané de 2 touches → le report « tout
relâché » (keycodes=0) est le 2e, il est droppé → plus aucune touche ne change →
il n'est jamais réémis → **touche répétée en boucle sur l'hôte** jusqu'à la
frappe suivante.
**Fix** : ne pas jeter — re-essayer (attendre l'EP prêt : `tud_task`/petit délai,
ou ré-enfiler en tête). Faire remonter l'échec au lieu de l'ignorer.

### 🟠 F4 — `send_tap` court-circuite la queue → appels TinyUSB réentrants `✅ code`
`main/input/keyboard_task.c:31-39` (`send_tap` → `hid_send_keyboard` direct)
Le chemin normal passe par `hid_queue` → un seul consommateur (`hid_sender_task`,
prio 4). Mais `send_tap` (tap-dance, leader, macro) appelle `hid_send_keyboard`
**en direct** depuis `vTaskKeyboard` (prio 3), hors queue, et son `vTaskDelay(20ms)`
yield → le sender (prio 4) préempte en plein `tud_hid_keyboard_report` → **appels
concurrents/réentrants** sur l'API TinyUSB (non ré-entrante).
**Scénario** : touche tenue (report en vol via sender) + tap-dance qui résout →
« B down/up » émis en direct, entrelacé avec le sender → report perdu ou EP
corrompu → touche manquée/inversée (stuck).
**Fix** : router **tout** l'envoi HID par la queue+sender (supprimer les envois
directs), ou un mutex commun autour de chaque appel TinyUSB.

### 🟠 F3 — `stat_matrix_changed` effacé après lecture → touche manquée `✅ code`
`main/input/keyboard_task.c:116-125`
```
if (stat_matrix_changed == 1) {
    build_keycode_report();   // lit l'état
    stat_matrix_changed = 0;  // clear APRÈS
```
Un callback scan (prio 5) qui tombe entre `build` et le `clear` pose un nouveau
`=1` + de nouvelles données ; le `clear` l'efface → l'itération suivante voit `==0`
et saute le bloc → **edge perdu → touche manquée** en frappe rapide.
**Fix** : test-and-clear du flag **avant** de lire (clear puis build).

### 🟠 F2 — Lecture déchirée des tableaux de scan (préemption prio5 > prio3) `✅ code`
`main/input/matrix_scan.c:83-119` (écriture) vs `key_processor.c:283-332` (`build_keycode_report`, lecture multi-passes)
Le callback scan (prio 5) écrit `MATRIX_STATE`, `current_press_row/col/stat[]`,
`keycodes[]` **sans section critique** ; `build_keycode_report` relit ces tableaux
sur plusieurs passes. Le callback préempte au milieu (d'abord tout à
`INVALID_KEY_POS`, puis re-remplit) → passes qui lisent un mélange de 2 snapshots
→ keycode incohérent. **Pas d'OOB** (indices bornés) mais report erroné.
**Fix** : snapshot atomique (double-buffer) ou section critique courte autour de
la lecture. Corroboré par les 2 volets (aussi listé « B1 »).

### 🟡 F5 — Report perdu silencieusement si queue pleine / re-push raté `agent`
`main/input/hid_report.c:167-177`
Retour de `xQueueSend` (l.177) ignoré → queue pleine (32) après 5 ms → report
clavier droppé sans log ; si key-up → stuck. + chemin combine-souris qui `return`
sans jamais enfiler le message clavier ; + re-push `SendToFront` timeout 0 qui
peut perdre le message. Faible probabilité (queue profonde), vrai chemin.
**Fix** : traiter l'échec d'enqueue (au minimum logguer + ne pas perdre un key-up).

### 🟡 F7 — Bascule transport / BLE non prêt → release droppé → stuck `agent`
`main/comm/hid_transport.c:45-59`
Si `usb_bl_state==1` (BLE) mais `bt_ready()==false`, `hid_send_keyboard` ne fait
rien → key-up perdu → stuck sur l'hôte précédent. Bascule USB↔BLE touches tenues,
ou déconnexion BLE en frappe. Partiellement by-design (cf. commentaire d'en-tête).

### 🟡 B2 — `hid_sender_task` lancée même si queue/mutex NULL → abort si OOM `agent`
`main/input/hid_report.c:248-258`
Création de la tâche non gatée sur le succès de `xQueueCreate`/`xSemaphoreCreate`
→ `xQueueReceive(NULL,…)` → `configASSERT` → abort. OOM au boot, peu probable.
**Fix** : gater la création de la tâche sur queue+mutex non-NULL.

### ⚪ F6 — Descripteur clavier « boot » avec Report IDs → décalage pré-OS `agent`
`main/comm/usb/usb_hid.c:72-90`
`HID_ITF_PROTOCOL_KEYBOARD` (boot) mais report descriptor préfixé de Report IDs
(1=kbd, 2=mouse). Un hôte en boot protocol (BIOS/UEFI/GRUB) lit le Report ID comme
l'octet modifier → touches décalées **avant l'OS**. Aucun impact une fois l'OS
chargé, pas de warning noyau Linux. Latent, à confirmer sur vraie machine BIOS.

### ⚪ Divers scan→HID `agent`
- **VID/PID = `0xCafe:0x4001`** (`boards/kase_v2/board.h:69-70`) = valeurs
  d'exemple TinyUSB. **Ne cause pas** de warning noyau à l'énumération (descripteur
  CDC union présent). Problème d'identité/collision, pas de fiabilité.
- **`#define NKRO`** (`keyboard_config.h:15`) **sans effet** : descripteur = boot
  keyboard 6-key, callback plafonne à 6 (`matrix_scan.c:95`) → 7e touche+ ignorée
  silencieusement (**6KRO réel**, pas NKRO). Trompeur.

---

## 2. Logique de traitement (couches / tap-hold / combo / leader)

### 🔴 CR-1 — LT couche hors-borne → lecture keymap OOB + couche bloquée `agent` *(keymap pathologique)*
`main/input/tap_hold.c:83` (state=HOLD posé avant le bornage) · `:60-79` (`recompute_lt_layer` sans reclamp) · `:107-112`
`e->state = TH_HOLD` est posé **avant** le clamp `if (layer < LAYERS)`, donc une LT
OOB devient une entrée HOLD ; `recompute_lt_layer` fait `current_layout =
K_LT_LAYER(top)` sans reclamper → couche OOB. `deactivate_hold` d'une LT OOB saute
le recompute → reste bloqué. **Step 4 lit alors `keymaps[≥LAYERS][r][c]` = OOB**
(l'UB que C1 prétendait fermer). **Précondition** : keymap contenant `LT(couche≥10)`,
uploadable via CDC **sans validation firmware** — donc pas déclenché avec une keymap
saine, mais un upload malveillant/buggé l'ouvre.
**Fix de fond** : valider les bornes de couche (MO/TO/LT/OSL/LM < LAYERS) dans le
handler keymap CDC. **Fix local** : border en tête d'`activate_hold` + reclamp dans
`recompute_lt_layer` + recompute aussi au deactivate OOB.

### 🟠 EL-1 — Modifier coupé quand une de deux tap-hold même-mod relâchée (home-row mods) `agent (fort)`
`main/input/tap_hold.c:23` (masque) · `:87,114` (`|=`) · `:106,114` (`&= ~`)
`active_hold_mods` est un simple bitmask OR/AND-NOT **sans refcount**. Deux holds
portant le **même bit** : relâcher l'un fait `&= ~mod` et **coupe le modifier alors
que l'autre est tenu**. Aggravé : `K_MT_MOD` masqué à `0x0F` (`key_definitions.h:441`)
→ seuls les mods **gauche** représentables → une paire miroir de home-row mods
(Shift gauche sur F + Shift droit sur J) s'effondre sur le **même bit** → collision
**garantie**, pas exotique.
**Scénario** : F=`MT(LSFT,f)`, J=`MT(LSFT,j)` ; tenir F puis J (mods=LSFT) ;
relâcher F → `&= ~LSFT` → **Shift tombe** bien que J tenu → caractères suivants
non-shiftés.
**Fix** : refcount par bit (compteur par modifier) **ou** recompute du masque à
partir des entrées HOLD (comme la couche l'est déjà via `recompute_lt_layer`).

### 🟠 EL-2 — tap-dance : tap-count gonflé en rollover → danse mal résolue `✅ code`
`main/input/key_processor.c:221-224` (appel non gardé) · `tap_dance.c:82-92`
`tap_dance_on_press` **non gardé par `is_new_press`** (contrairement à
`K_SEC_CONFIRM`/`K_DISP_NEXT`). `build_keycode_report` re-tourne sur **tout**
changement matrice → une touche tap-dance encore tenue est re-« pressée »
logiquement à chaque autre frappe → `tap_count++` + `last_tap_ms` rafraîchi → le
timeout HOLD ne se déclenche jamais, la danse résout sur la mauvaise action.
**Fix** : garder l'appel derrière `is_new_press(row,col)` (couvre aussi MO-1).

### 🟡 MO-1 — caps-word / layer-lock / leader re-déclenchés si touche tenue `✅ code`
`main/input/key_processor.c:226,228,244`
Même racine que EL-2 : `caps_word_toggle()`, `leader_start()`, `layer_lock_toggle()`
non gardés `is_new_press` → rejoués à chaque rebuild tant que la touche est tenue
(caps-word **clignote** ON/OFF ; leader reset son buffer ; lock/unlock répétés).
Impact réel seulement si l'utilisateur **tient** ces touches (conçues tap-and-release).
**Fix** : même garde `is_new_press` qu'EL-2 (une correction couvre les deux).

### 🟡 MO-2 — Résultat de combo non maintenu (émis 1 cycle) `agent` *(peut-être by-design)*
`main/input/combo.c:158-178`
Le keycode combo n'est (re)poussé que sur `both && !combo_active[i]` → un seul
cycle, puis disparaît alors que les deux touches restent tenues. Sans effet pour
les combos-tap (J+K=Esc) ; casse un combo mappé sur un keycode à **maintenir**
(flèche pour répétition, modifier). À confirmer côté produit (QMK maintient).

### (à fermer) M6 — MO non restauré si MO mappé sur sa propre couche `agent` *(keymap-dépendant)*
`main/input/key_processor.c:57` (`apply_momentary_layer`) · `:496-512` (teardown Step 10)
Si la position `MO_Lx` porte encore un keycode MO sur la couche x, le 2ᵉ cycle
refait `last_layer = current_layout = x` → `last_layer == current_layout` → Step 10
sauté → **couche jamais restaurée** (idem LM → mods coincés). Non déclenché sur les
keymaps par défaut (K_NO/K_TRNS sur la touche MO). **Fix one-liner** :
`if (layer <= 9 && layer != current_layout)` dans `apply_momentary_layer` — un MO
vers la couche déjà active est un no-op, donc pas d'effet de bord. Même précondition
que CR-1 → le vrai correctif de fond est la **validation des couches côté CDC**.

---

## 3. Stabilité / boot / hot-path

### 🟠 A1 — Compteur boot-crash remis à 0 trop tôt → safe-mode + rollback OTA aveugles aux crashs runtime `✅ code`
`main/main.c:344-345` (`boot_crash_count=0` + `esp_ota_mark_app_valid_cancel_rollback`) vs `:153` (seuil safe-mode)
Le reset du compteur et l'annulation du rollback OTA se font **en fin d'`app_main`**
(~1-2 s), donc **avant** que les tâches fault-prone ne tournent (scan sur keymap
uploadée, LVGL, HID). Un crash **runtime** déterministe (3 s après boot) ne fait
jamais monter le compteur → `safe_mode` **jamais activé** pour cette classe, et une
image OTA qui boote-OK mais crashe au runtime est **verrouillée sans rollback** →
boot loop permanent. Le safe-mode ne couvre que les crashs *avant* la ligne 344.
**Fix** : différer `boot_crash_count=0` + `mark_app_valid` après **N secondes de
fonctionnement sain** (depuis la boucle main / une petite tâche de santé), pas en
fin d'`app_main`.

### 🟡 B3 — Safe-mode pas « collant » `agent`
`main/main.c:153-160, 344`
À l'entrée en safe-mode le compteur est remis à 0, et un boot safe réussi le remet
aussi à 0 → le boot suivant repart en mode normal. Si le crash normal est
déterministe : 3 boots ratés → 1 boot safe → 3 ratés → … Dépanne mais n'offre pas
d'état stable. Couplé à A1, ne se déclenche de toute façon pas sur les crashs runtime.

### ⚪ Divers stabilité `agent`
- **`ESP_LOGW` dans le callback de scan** (`matrix_scan.c:64`) — viole la règle
  « pas de log dans les callbacks de scan ». Rare (timeout test-mode) ; le
  `ks_respond()` (CDC TX) émis depuis `kbd_task` en test-mode peut bloquer la boucle.
- **`keyboard_task_handle` non `volatile`** (`keyboard_task.c:27`) — fenêtre où le
  callback tourne avant que le handle soit renseigné → notification perdue,
  rattrapée par le timeout 10 ms. Bénin.
- **`cpu_time_logger_task`** tourne en prod (diagnostic dev), `ESP_LOGI` /5 s. Bruit.

### Vérifié SAIN (ne pas re-signaler)
ISR gptimer propre (`IRAM_ATTR`, ne fait que set-bit) ; pas de `malloc` en
hot-path ; bornage des indices de scan (pas d'OOB) ; `hid_report_mutex` sans
deadlock ; ordre d'init sans usage-avant-init ; toutes les tâches yield (pas de
famine TWDT) ; détection de release par **position physique** (une LT/MT relâchée
sur une autre couche est bien désactivée) ; bornes combo/tap_dance/leader sûres ;
`tap_hold_tick`/`interrupt` sans double-activation.

---

## Priorisation proposée

**Round 1 — casse la frappe NORMALE (TDD, test qui mord d'abord) :**
- EL-1 (home-row mods) · EL-2 + MO-1 (garde `is_new_press`) · F1 + F4 (sérialiser
  HID + retry EP) · F3 (test-and-clear flag).

**Round 2 — filet de sécurité + robustesse :**
- A1 (différer reset/mark-valid) · validation des bornes de couche CDC (ferme
  CR-1 + M6 d'un coup) · F2 (snapshot/section critique) · F5/F7/B2.

**À discuter / différer :** MO-2 (peut-être by-design), F6 (pré-OS), B3, divers ⚪.

**Note transversale** : plusieurs 🔴/🟠 keymap-dépendants (CR-1, M6) ont la même
racine — le handler keymap CDC **ne valide pas** les bornes de couche des keycodes
uploadés. Un seul garde de validation à l'upload neutralise toute cette classe.
