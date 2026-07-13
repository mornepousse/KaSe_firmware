# Dongle : répartition + fiabilité frappe des halfs — 2026-07-13

Synthèse d'une session brainstorming (archi dongle) → debug (frappe halfs).

---

## 1. Décision d'architecture — « focus keyboard »

Le dongle portait **trois identités contradictoires** (d'où « le grand n'importe
quoi sur la répartition ») :

| # | Identité | Chemin | Cerveau keymap | Statut |
|---|---|---|---|---|
| I | **Cerveau du split** — halfs = scanners bruts (`PKT_TYPE_KEY`), dongle merge + keymap → USB HID | A | dans le dongle | **VIVANT** |
| II | **Répéteur bête** — clavier complet envoie le HID final (`PKT_TYPE_HIDREPORT`), dongle relaie | B | dans le clavier | expérimental (V2D relay) |
| III | **Co-proc sécurité** — OpenPGP/SEC keyboard-agnostic | — | — | gelé (`KASE_SEC_NONE`) |

Les trois demandent des répartitions **opposées** et sont compilées ensemble.

**Fait matériel** : le dongle est un **ESP32-S3-WROOM-2** (même SoC que le
clavier). Il n'est donc pas sous-capable en CPU/flash ; ce qu'il n'a pas =
matrice/écran/batterie. Ce qu'il a en plus = 2 radios NRF24 + USB natif.

### Cause du lag « la clé PGP fait tout laguer »
Structurel, pas un bug ponctuel : `openpgp_crypto.c` garde **WiFi allumé en
permanence** (TRNG + ESP-NOW), sur le même MCU que l'USB full-speed + les 2 NRF24
+ l'interface **CCID** dans le composite USB. Crypto+WiFi+smartcard USB vs frappe
temps-réel se battent pour le CPU/les endpoints → jitter continu.

### Décisions actées
- ✅ **Focus keyboard.** Dongle = récepteur clavier **pur** (identité I). La
  sécu/PGP sort du chemin de frappe.
- 🔮 **Futur** : ESP-NOW **intermittent** (réveil périodique au lieu de WiFi
  always-on) pour rendre du temps CPU. Pas maintenant.
- Identité II (relay HID / `KBD_WIRELESS`) et III (sécu) : hors du produit
  courant. À retirer / laisser gelées — pas à faire cohabiter avec la frappe.

---

## 2. Bug de frappe des halfs — investigation

Symptôme (les **deux** côtés) : « il faut appuyer plusieurs fois pour avoir un
caractère, ou on en a plusieurs (uuu) ». Frappe « très dure ».

### Preuves live (console half gauche, slot=L, MAC b8:f8:62:e2:d1:08, eink=0)
- **RF innocenté** : `ack=100% maxrt=0` en continu → les paquets half→dongle ne
  se perdent pas. La piste « perte RF » est **morte**.

### Deux bugs distincts identifiés

**Bug #1 — sleep/wake avale les frappes après inactivité (CONFIRMÉ, half non-eink)**
- Seuil : light-sleep après **15 s** d'inactivité (`HALF_POWER_T_SLEEP_MS`).
- Au sommeil, `half_sleep_enter()` fait `half_scan_stop_for_sleep()` →
  **détruit la tâche `keyboard_button`** ; au réveil `half_scan_restart_after_wake()`
  la **recrée from scratch** (log : `kbd task exit` … `KeyBoard Button Version: 1.0.0`).
- Pendant quiesce→sleep→wake→ré-init (~50 ms) le scan est mort → la 1ʳᵉ frappe
  (sert à réveiller via GPIO) + les suivantes pendant la ré-init sont **perdues**.
- N'affecte QUE les halfs non-eink (le half droit e-ink a le light-sleep gated off).

**Bug #2 — frappe continue non fiable des DEUX côtés (scan/timing, à confirmer)**
- Comme le half droit (e-ink, ne dort pas) galère aussi, la cause commune n'est
  PAS le sleep → c'est le **scan/debounce**, partagé par les deux firmwares.
- Suspect matériel-timing : les halfs sont en **ROW2COL active-high** avec
  **`BOARD_MATRIX_SETTLING_US = 0` et `RECOVERY_US = 0`** (aucun délai de
  settling/décharge). Le V1 a explicitement 20 µs / 50 µs (anti-ghosting). Lecture
  des colonnes avant charge/décharge complète → **misreads** = press ratés + ghosts.

  | Board | SETTLING | RECOVERY | DEBOUNCE |
  |---|---|---|---|
  | V1 | 20 µs | 50 µs | 5 |
  | V2 | 0 | 0 | 3 |
  | halfs | **0** | **0** | 3 |

- Debounce 3 ticks = **3 ms** (bas pour du mécanique ; V1 = 5).
- Composant `keyboard_button` : callback appelé **inline dans la tâche de scan**
  → le TX RF SPI se fait dans le thread de scan (contention `half_spi_lock`
  possible, à vérifier).
- `half_diff_emit` + l'event model du composant (`KBD_EVENT_PRESSED` fire sur
  press ET release) sont **corrects** — écartés.

### Gap de test repéré
`test/test_half_matrix_diff.c` ne couvre QUE le cas où `key_release_data` est
fourni. Pas de test « une touche quitte `key_data` sans release » (asymétrie du
chemin release). À couvrir si on touche `half_diff_emit`.

---

## 2bis. Résultat instrumentation (edges loggés sur le half gauche)

Half gauche flashé avec un log par edge (`tx_key_event`) + par callback. Frappe
mesurée en direct :

- **Scan SAIN** : chaque appui physique = **1 DN + 1 UP nets** sur la bonne
  coordonnée. Aucun chatter, aucun ghost, aucun double. → **settling=0 / debounce
  RÉFUTÉS** comme cause. NE PAS toucher au settling.
- **RF sain** : `ack~100%`.
- **Sleep coupable, prouvé** : light-sleep toutes les 15 s d'idle ; au réveil,
  scan « detect held key » one-shot (sans debounce) qui a **détecté 2 touches
  fantômes** (`r=0c=4`, `r=1c=0`) tenues ~600 ms alors que la touche tapée était
  `r=2c=4`, ET la frappe de réveil est avalée.

→ « appuyer plusieurs fois » = 1re frappe post-pause perdue au réveil ;
« caractères en trop » = fantômes injectés au réveil.

### Décision — OPTION A (2026-07-13)
Light-sleep **désactivé** sur les halfs (fiabilité d'abord). Gate réversible
`HALF_LIGHT_SLEEP_ENABLED 0` dans `half_scan_task.c`. Réactiver une fois le
chemin de réveil réparé (option B : ne pas détruire/recréer le scan, ne pas
émettre les touches détectées au réveil sans repasser par le debounce) — dans le
futur chantier power-save avec ESP-NOW intermittent.

⚠️ Reste ouvert : le half **droit** (e-ink) ne dort pas mais l'utilisateur le dit
aussi problématique → **2ᵉ cause possible côté droit ou dongle**, à instrumenter
séparément (le droit n'a pas été loggé). À creuser après validation de l'option A.

## 2ter. VRAI coupable trouvé — race heartbeat↔edge (capture host)

Capture des events USB côté host (`/dev/input/event9`, script `scratchpad/kbcap.py`),
touche `b` martelée à vitesse uniforme → **HOLD bimodal** :
- taps propres : HOLD 14–200 ms, rpt=0
- taps « collés » : HOLD **250 / 260 / 356 / 454 / 600 / 709 ms**, rpt≥1 → doublons
- valeurs collées **quantifiées à ~100 ms** = la **cadence du heartbeat**.

**Cause racine (prouvée par la quantification)** : `heartbeat_timer_cb`
(`half_scan_task.c`) capturait `s_pressed_bitmap` **hors du lock SPI**, puis prenait
le lock pour envoyer. Entre les deux, le kb_task relâchait une touche (efface le
bitmap + envoie le RELEASE edge **sous le lock, donc avant** le heartbeat). Ordre
sur l'air : `RELEASE`, puis `HB(bitmap=encore pressé)`. Le dongle applique le
release (have=0) puis `hb_reconcile` voit want=1 → **force_press** → touche
re-pressée, collée jusqu'au heartbeat suivant (bitmap=0) → auto-repeat kernel →
**doublons + `Lctrl` collé des secondes**. Frappe TOUTES les touches (pas
seulement home-row mods). RF innocentée (`ack~100%`), scan half sain.

**FIX (2026-07-13)** : capturer le bitmap **sous le lock** (`memcpy` + encode +
send atomiques vis-à-vis du TX des edges). Un `HB(bitmap=pressé)` ne peut alors
plus être envoyé après un RELEASE edge → plus de force_press fantôme. Fix dans
`half_scan_task.c` (les deux halfs). Vérif : re-mesure kbcap → HOLD ne doit plus
se quantifier au heartbeat.

Résidu possible (non observé) : press+release dans le même drain 10 ms du dongle
→ tap net-zéro → drop. Rare (IRQ-driven). À surveiller après le fix heartbeat.

## 2quater. Bug #3 — e-ink refresh affame le scan (right half) + FIX option 1

Après le fix heartbeat, résidu : stuck **longs occasionnels** (400-990 ms, rpt jusqu'à 22),
**uniquement sur le half droit (e-ink)**. Corrélation half↔host prouvée : pendant un stuck,
le half loggait des `HBMAP` (bitmap non-nul) pendant ~1 s **sans aucun key-edge** → le
**scan du half lui-même** croit la touche tenue ~1 s → ce n'est ni le dongle ni la réco.
Cause : `eink: eink_push: BUSY cleared after 1500 ms` — le refresh e-ink (1,5 s) contend le
bus SPI / CPU et **affame la tâche de scan** (prio 5), qui rate les releases.

**FIX option 1 (2026-07-13, choisi par l'user)** : différer le refresh e-ink tant que le
clavier tape. `half_kbd_idle_ms()` exposé par `half_scan_task`; dans `eink_lvgl_task`, on
n'appelle `lv_timer_handler()` (render+flush) que si idle ≥ `EINK_TYPING_DEFER_MS` (300 ms).
Les invalidations LVGL restent en attente → le dashboard se met à jour à la 1ʳᵉ pause. Le
dashboard est cosmétique, sa latence est sans importance. À vérifier : re-mesure kbcap sur
le half droit → plus de stuck longs pendant une rafale.

Note : un refresh démarré dans une pause de 300 ms peut encore chevaucher une reprise de
frappe (le refresh dure 1,5 s, non abortable). Rare (refreshs dashboard peu fréquents). Si
ça persiste : isoler le refresh (autre cœur) ou réduire encore sa fréquence.

⚠️ **PAS de rewrite from-scratch** : bug #1 (dominant) corrigé+vérifié ; #2, #3 = fixes
locaux ciblés. Réécrire jetterait RF/pairing/CDC/keymap-engine/OTA/sécu validés hardware
pour re-buter sur les mêmes murs matériels (e-ink lent + 2 radios + scan sur 1 MCU).

## 3. Prochaines actions

1. **Trancher bug #2 par instrumentation** (Iron law : preuve avant fix) :
   - Test A/B console (sleep vs scan) — protocole défini, pas encore exécuté.
   - Si scan confirmé : logger chaque edge émis dans `tx_key_event` (ESP_LOGI +
     timestamp), flasher UN half (MAC-gate obligatoire), presser lentement →
     compter les edges par appui physique (multiples = chatter/settling à la
     source ; 1 propre = côté dongle/reconcile).
2. **Bug #1 (sleep)** : soit rallonger le seuil, soit ré-injecter la frappe de
   réveil, soit ne pas détruire/recréer `keyboard_button` (garder l'état).
3. **Bug #2 si settling** : mettre SETTLING/RECOVERY > 0 sur les halfs (cf. V1),
   éventuellement DEBOUNCE 5. Fix minimal, à valider par la même instrumentation
   (doit faire tomber le taux d'erreur).

⚠️ Tout flash sur half : **MAC-gate d'abord** (jamais read-then-proceed).
MACs : left `b8:f8:62:e2:d1:08`, right `b8:f8:62:e2:d8:5c` (e-ink),
V2D `b8:f8:62:e2:e0:d0`, dongle `ac:a7:04:18:81:ec`.
