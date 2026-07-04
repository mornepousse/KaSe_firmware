# Refonte de l'écran OLED I2C (V2 / V2D) — Design

**Date :** 2026-07-04
**Périmètre :** backend OLED SSD1306 128×64, partagé par `kase_v2` et `kase_v2_debug` (V2D).
**Hors périmètre :** V1 (écran rond, `round_backend` — inchangé), dongle, halves.

## 1. Contexte & objectif

L'affichage OLED actuel (« Layout D — Card », `main/display/oled/oled_backend.c`, ~498 l.)
est un **dashboard unique et statique** : tous les `lv_obj_t` en globals de module, la
logique d'état mêlée au rendu LVGL, tout affiché en même temps sur un seul écran, la
zone tama coincée à droite. Fonctionnel mais figé et non extensible.

L'utilisateur veut une **refonte totale** : nouveau look, nouveau contenu, et une
**navigation multi-écrans**. Objectif d'ingénierie : séparer **la décision** (quel écran
afficher) **du rendu** (dessin LVGL) pour que la logique de navigation soit **pure et
testable en host** (même contrat TDD que le reste du repo), et que chaque écran soit un
module isolé à responsabilité unique.

## 2. Décisions validées (brainstorming)

- **Refonte totale** : table rase du Layout D.
- **Navigation hybride** : contextuelle par défaut (l'écran réagit) + un keycode
  `K_DISP_NEXT` pour choisir/cycler l'écran de repos.
- **Écrans** : `SPLASH` (boot), `HOME`, `LAYER`, `STATS`, `TAMA`.
- **Style** : graphique / icônes (icônes + barres + sparklines + sprite tama comme art).
- **Version firmware** : affichée sur le **boot splash** (~2s), pas sur les écrans permanents.
- **Architecture** : écrans modulaires + un screen-manager, la machine à états de nav
  **pure** (zéro LVGL).

## 3. Architecture

```
main/display/oled/
├── oled_backend.c        # MINCE : implémente display_backend_t, traduit les appels
│                         #   vtable en events pour oled_nav, délègue le rendu au manager.
├── oled_nav.c / .h       # ⭐ MACHINE À ÉTATS PURE — zéro LVGL, zéro hardware.
├── oled_screens.c / .h   # Manager : registre des écrans + glue LVGL (build/update/destroy).
└── screens/
    ├── screen_splash.c   # nom KaSe + version firmware
    ├── screen_home.c     # statut d'un coup d'œil
    ├── screen_layer.c    # nom de couche en gros (flash)
    ├── screen_stats.c    # KPM/WPM + sparkline + compteur jour
    └── screen_tama.c     # sprite OpenCritter + stats du pet (réutilise tama_render.c)
```

**Conservé tel quel :** le vtable `display_backend_t`, `status_display.c` (coordinateur),
`i2c_oled_display.c` (driver panel), `tama_render.c`, les assets icônes (`assets/`).
**Remplacé :** tout l'intérieur monolithique de `oled_backend.c`.

### 3.1 Interface d'un écran (`oled_screen_t`)

```c
typedef struct {
    void (*build)(lv_obj_t *parent);   /* crée ses lv_obj dans le conteneur fourni */
    void (*update)(void);              /* rafraîchit ses données (lock LVGL déjà tenu) */
    void (*destroy)(void);             /* détruit ses lv_obj + reset ses pointeurs */
} oled_screen_t;
```

Chaque écran ne connaît que ses propres objets. Un écran doit pouvoir être compris et
modifié sans lire les autres. Le manager garde un tableau indexé par l'enum d'écran.

### 3.2 Machine à états pure (`oled_nav`)

Aucune dépendance LVGL/hardware — uniquement des IDs d'écran (enum) et le temps (ms).

```c
typedef enum { OLED_SCR_SPLASH, OLED_SCR_HOME, OLED_SCR_LAYER,
               OLED_SCR_STATS,  OLED_SCR_TAMA,  OLED_SCR_COUNT } oled_screen_id_t;

typedef enum { OLED_EV_BOOT, OLED_EV_LAYER_CHANGED, OLED_EV_DISP_KEY,
               OLED_EV_ACTIVITY } oled_nav_event_t;

void             oled_nav_init(uint32_t now_ms);          /* → SPLASH jusqu'à now+2000 */
void             oled_nav_event(oled_nav_event_t ev, uint32_t now_ms);
oled_screen_id_t oled_nav_active(uint32_t now_ms);        /* écran à afficher maintenant */
void             oled_nav_set_tama_enabled(bool en);       /* idle→TAMA seulement si activé */
```

État interne : `resting` (défaut `HOME`), `flash_until_ms`, `splash_until_ms`,
`last_activity_ms`, `tama_enabled`.

`oled_nav_active(now)` (priorité décroissante) :
1. `now < splash_until_ms` → `SPLASH`
2. `now < flash_until_ms` → `LAYER`
3. `tama_enabled && (now - last_activity_ms) >= IDLE_MS(30000)` → `TAMA`
4. sinon → `resting`

Transitions :
- `BOOT` → `splash_until_ms = now + 2000`.
- `LAYER_CHANGED` → `flash_until_ms = now + 2500`.
- `DISP_KEY` → `resting = cycle(resting)` sur `{HOME, STATS, TAMA}` ; `flash_until_ms = 0`
  (coupe le flash) ; `last_activity_ms = now`.
- `ACTIVITY` (frappe/souris) → `last_activity_ms = now`. **Ne change jamais l'écran.**

Note : `LAYER` et `SPLASH` ne font pas partie du cycle manuel (transitoires). Le cycle
manuel = `HOME → STATS → TAMA → HOME`.

### 3.3 Flux de rendu

`oled_backend` mappe la vtable existante vers des events + un tick :
- `refresh_all` / init → `oled_nav_init(now)` + `oled_screens_reset()`.
- `update_layer` → `oled_nav_event(LAYER_CHANGED, now)`.
- `notify_keypress` / `notify_mouse` → `oled_nav_event(ACTIVITY, now)` (+ compteur KPM/tama).
- **nouveau** `notify_display_key` (via keycode `K_DISP_NEXT`) → `oled_nav_event(DISP_KEY, now)`.
- `update` (tick périodique) → sous lock LVGL : `id = oled_nav_active(now)` ; si
  `id != current` → `screens[current].destroy()` puis `screens[id].build(container)` ;
  puis `screens[id].update()`.

Le manager ne (re)construit un écran que quand il change → pas de churn LVGL par tick.

## 4. Les écrans (style graphique / icônes, 128×64)

### SPLASH (boot, ~2s → HOME)
```
+----------------------+
|                      |
|       K a S e        |
|                      |
|       v4.0.0         |   version = source de la commande CDC get-version (git describe)
|                      |
+----------------------+
```

### HOME (repos par défaut)
```
+----------------------+
| [USB] BT2       CAP  |   icône chemin (USB/BLE/RF) + slot BT + caps
| .------------------. |
| |      BASE        | |   couche active (cadre)
| '------------------' |
| .:iII|Ii:. 142k  [M] |   mini-sparkline KPM + indicateur souris
+----------------------+
```
Données : `hid_bluetooth_*`, chemin (`kbd_active_route` en wireless / `usb_bl_state` sinon),
`bt_get_active_slot`, `default_layout_names[current_layout]`, `hid_led_state` (caps),
activité souris, fenêtre KPM.

### LAYER (flash 2.5s au changement de couche)
```
+----------------------+
|  ####  ##   ####     |
|  #     # #  #        |   nom de couche en GROS
|  ###   # #  # ##     |
|  #     #  # ####     |
|            > layer 2 |   sous-titre : index de couche
+----------------------+
```
Données : `default_layout_names[current_layout]`, `current_layout`.

### STATS
```
+----------------------+
| [kb] 142     ^58 wpm |   KPM + WPM
| .:iII|II|Ii:.        |   sparkline temps réel (fenêtre 60s)
| ==============       |   barre KPM
| oooooo...  1.2M keys |   compteur TOTAL de frappes
+----------------------+
```
Données : fenêtre KPM (existante), WPM (`wpm_record_keypress`), **total de frappes**
(`key_stats` total en NVS — existe déjà), historique pour la sparkline (downsampling de la
fenêtre KPM). Helpers de calcul purs → testables.
Note : pas de compteur « par jour » — le clavier n'a pas d'horloge temps réel. Un compteur
de **session** (depuis le boot, RAM) serait un ajout trivial si voulu ; un vrai « /jour »
nécessiterait une source d'heure (hors périmètre).

### TAMA (écran dédié)
```
+----------------------+
| hunger [###..] lv3   |   faim + niveau
|                      |
|       (o_o)          |   sprite OpenCritter (grand, centré, rendu par tama_render)
|       /|_|\          |
| happy  [####.] 8420t |   bonheur + tama-time
+----------------------+
```
Données : `tama_engine_get_state/stats/critter` via `tama_render`.

## 5. Keycode `K_DISP_NEXT`

Nouveau keycode d'action interne (plage OSM/OSL/… `0x3000-0x3DFF`, cf. `key_definitions.h`).
Détecté dans `key_processor` (comme les autres fonctions internes) → appelle
`status_display`/backend `notify_display_key`. Un seul keycode qui cycle l'écran de repos.
Le **lock** (figer, supprimer même le flash de couche) est un raffinement ultérieur, hors
de ce design.

## 6. Tests

**Host (TDD) — `test/test_oled_nav.c`, module `oled_nav.c` linké réel :**
- Boot → `SPLASH` jusqu'à +2000ms → `HOME`.
- `LAYER_CHANGED` → `LAYER` jusqu'à +2500ms → `resting`.
- `DISP_KEY` cycle `HOME→STATS→TAMA→HOME` et coupe un flash en cours.
- Idle ≥ 30s → `TAMA` si activé ; retour `resting` sur `ACTIVITY` ; jamais `TAMA` si tama off.
- Croisements : flash pendant idle (flash gagne) ; `DISP_KEY` pendant splash/flash.
- Helpers purs (fenêtre KPM, downsampling sparkline, WPM, formatage version) : tests dédiés.

Chaque test doit **mordre** (sabotage prod → rouge → revert), norme du repo.

**Non testable host** (LVGL/hardware) → board build (`kase_v2`) + smoke-test hardware sur
V2D : chaque `screen_*.c` (build/update/destroy) et le driver. Ajouter les points OLED au
`docs/HARDWARE_SMOKE_TEST.md` (les 5 écrans, le keycode, le flash de couche, l'idle→tama).

## 7. Non-objectifs (YAGNI)

- Pas de lock d'écran (raffinement futur).
- Pas de refonte du round display V1.
- Pas de nouveaux écrans au-delà des 5 (clock, heatmap… plus tard si besoin).
- Pas de configuration des écrans via CDC (l'ordre/contenu est en dur pour l'instant).

## 8. Risques

- **RAM/flash** : 5 écrans construits à la demande (un seul vivant à la fois) → empreinte
  LVGL maîtrisée. Vérifier à `check.sh --board kase_v2`.
- **Churn LVGL** : le manager ne rebuild qu'au changement d'écran ; attention à ne pas
  build/destroy en boucle si deux états oscillent (les timers l'empêchent par construction).
- **`lv_obj_is_valid`** : après un `destroy`/`display_clear_screen`, respecter la règle
  du repo (valider avant tout accès LVGL).
