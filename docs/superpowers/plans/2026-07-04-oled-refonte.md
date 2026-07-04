# Refonte écran OLED I2C (V2/V2D) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remplacer le dashboard OLED monolithique (Layout D) par un système multi-écrans (splash/home/layer/stats/tama) piloté par une machine à états de navigation *pure* et testable, sur V2 + V2D.

**Architecture:** `oled_backend.c` devient mince et traduit la vtable `display_backend_t` en événements pour `oled_nav` (machine à états pure, zéro LVGL). `oled_screens` (manager) interroge `oled_nav_active()` chaque tick et build/update/destroy le module d'écran voulu. Chaque `screens/screen_*.c` dessine ses propres `lv_obj`. Le keycode `K_DISP_NEXT` cycle l'écran de repos.

**Tech Stack:** C, ESP-IDF 5.5, LVGL 8, esp_lvgl_port, SSD1306 128×64 mono. Tests host CMake (`test/`).

## Global Constraints

- Cible : backend OLED partagé `kase_v2` + `kase_v2_debug`. Ne PAS toucher `round/` (V1).
- Mono 128×64 : `lv_color_black()` = pixel ALLUMÉ, `lv_color_white()` = éteint.
- Tout accès LVGL sous `lvgl_port_lock()` / `lvgl_port_unlock()` ; `lv_obj_is_valid()` avant d'accéder à un objet après un `destroy`/`display_clear_screen()`.
- Pas de `malloc` dans les hot paths ; buffers statiques.
- TDD obligatoire pour la logique pure (`oled_nav`, `oled_stats`) : test **d'abord**, et chaque test doit **mordre** (sabotage prod → rouge → revert).
- Tests host ajoutés à `test/CMakeLists.txt` + déclarés dans `test/test_main.c`.
- Après chaque tâche host : `./scripts/check.sh --host-only` vert. Tâches touchant le firmware : `./scripts/check.sh --board kase_v2` vert.
- `key_definitions.h` est partagé avec KaSe_soft : on AJOUTE des defines, on n'en retire pas.
- Version firmware : `esp_app_get_description()->version` (via `#include "esp_app_desc.h"`).

---

## Task 1 : `oled_nav` — scaffolding + splash + resting (pure, TDD)

**Files:**
- Create: `main/display/oled/oled_nav.h`
- Create: `main/display/oled/oled_nav.c`
- Test: `test/test_oled_nav.c`
- Modify: `test/CMakeLists.txt`, `test/test_main.c`

**Interfaces:**
- Produces:
  - `typedef enum { OLED_SCR_SPLASH, OLED_SCR_HOME, OLED_SCR_LAYER, OLED_SCR_STATS, OLED_SCR_TAMA, OLED_SCR_COUNT } oled_screen_id_t;`
  - `typedef enum { OLED_EV_BOOT, OLED_EV_LAYER_CHANGED, OLED_EV_DISP_KEY, OLED_EV_ACTIVITY } oled_nav_event_t;`
  - `void oled_nav_init(uint32_t now_ms);`
  - `void oled_nav_event(oled_nav_event_t ev, uint32_t now_ms);`
  - `oled_screen_id_t oled_nav_active(uint32_t now_ms);`
  - `void oled_nav_set_tama_enabled(bool en);`
  - Timing constants: `OLED_NAV_SPLASH_MS=2000`, `OLED_NAV_FLASH_MS=2500`, `OLED_NAV_IDLE_MS=30000`.

- [ ] **Step 1: Create the header**

Create `main/display/oled/oled_nav.h`:
```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    OLED_SCR_SPLASH, OLED_SCR_HOME, OLED_SCR_LAYER,
    OLED_SCR_STATS,  OLED_SCR_TAMA,  OLED_SCR_COUNT
} oled_screen_id_t;

typedef enum {
    OLED_EV_BOOT, OLED_EV_LAYER_CHANGED, OLED_EV_DISP_KEY, OLED_EV_ACTIVITY
} oled_nav_event_t;

#define OLED_NAV_SPLASH_MS  2000u
#define OLED_NAV_FLASH_MS   2500u
#define OLED_NAV_IDLE_MS    30000u

/* Réinitialise la machine à états. resting=HOME, splash actif jusqu'à now+2000.
   Ne touche PAS l'état tama_enabled (piloté séparément). */
void oled_nav_init(uint32_t now_ms);
void oled_nav_event(oled_nav_event_t ev, uint32_t now_ms);
oled_screen_id_t oled_nav_active(uint32_t now_ms);
void oled_nav_set_tama_enabled(bool en);
```

- [ ] **Step 2: Write the failing test (splash + resting)**

Create `test/test_oled_nav.c`:
```c
#include "test_framework.h"
#include "oled_nav.h"

/* Boot → SPLASH pendant 2s → HOME. */
static void test_nav_boot_splash_then_home(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    TEST_ASSERT_EQ(oled_nav_active(0),    OLED_SCR_SPLASH, "t=0 → SPLASH");
    TEST_ASSERT_EQ(oled_nav_active(1999), OLED_SCR_SPLASH, "t=1999 → SPLASH");
    TEST_ASSERT_EQ(oled_nav_active(2000), OLED_SCR_HOME,   "t=2000 → HOME (splash fini)");
    TEST_ASSERT_EQ(oled_nav_active(9000), OLED_SCR_HOME,   "repos = HOME");
}

void test_oled_nav(void) {
    TEST_SUITE("OLED nav — machine à états pure");
    TEST_RUN(test_nav_boot_splash_then_home);
}
```

Add to `test/test_main.c` (after `extern void test_led_anim_constants(void);`):
```c
extern void test_oled_nav(void);
```
And in `main()` (after `test_led_anim_constants();`):
```c
    test_oled_nav();
```

Add to `test/CMakeLists.txt` in the `add_executable(test_runner ...)` list:
```
    test_oled_nav.c
    ../main/display/oled/oled_nav.c
```
Add the include dir if needed (near other `target_include_directories`): `../main/display/oled`.

- [ ] **Step 3: Run test to verify it fails**

Run: `cd /home/mae/Documents/GitHub/KaSe_firmware && rm -rf test/build && cmake -S test -B test/build >/dev/null && cmake --build test/build 2>&1 | tail -5`
Expected: FAIL to link/compile — `oled_nav.c` empty (undefined `oled_nav_init` etc.).

- [ ] **Step 4: Write minimal implementation**

Create `main/display/oled/oled_nav.c`:
```c
#include "oled_nav.h"

static oled_screen_id_t s_resting;
static uint32_t s_splash_until;
static uint32_t s_flash_until;
static uint32_t s_last_activity;
static bool     s_tama_enabled;

void oled_nav_init(uint32_t now_ms) {
    s_resting       = OLED_SCR_HOME;
    s_splash_until  = now_ms + OLED_NAV_SPLASH_MS;
    s_flash_until   = 0;
    s_last_activity = now_ms;
}

void oled_nav_set_tama_enabled(bool en) { s_tama_enabled = en; }

oled_screen_id_t oled_nav_active(uint32_t now_ms) {
    if (now_ms < s_splash_until) return OLED_SCR_SPLASH;
    if (now_ms < s_flash_until)  return OLED_SCR_LAYER;
    if (s_tama_enabled && (now_ms - s_last_activity) >= OLED_NAV_IDLE_MS)
        return OLED_SCR_TAMA;
    return s_resting;
}

void oled_nav_event(oled_nav_event_t ev, uint32_t now_ms) {
    switch (ev) {
    case OLED_EV_BOOT:          s_splash_until = now_ms + OLED_NAV_SPLASH_MS; break;
    case OLED_EV_LAYER_CHANGED: s_flash_until  = now_ms + OLED_NAV_FLASH_MS;  break;
    case OLED_EV_ACTIVITY:      s_last_activity = now_ms;                     break;
    case OLED_EV_DISP_KEY:      /* filled in Task 3 */                        break;
    }
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build test/build >/dev/null && ./test/build/test_runner 2>&1 | grep -E 'boot_splash|Results'`
Expected: `[test_nav_boot_splash_then_home] OK` and `Results: N passed, 0 failed`.

- [ ] **Step 6: Prove the test bites**

Temporarily change `oled_nav_active` first line to `return OLED_SCR_HOME;`. Rebuild + run: the splash asserts FAIL. Revert.

- [ ] **Step 7: Commit**

```bash
git add main/display/oled/oled_nav.h main/display/oled/oled_nav.c test/test_oled_nav.c test/test_main.c test/CMakeLists.txt .tripwire-testcount
git commit -m 'feat(oled): oled_nav pure state machine — splash + resting (TDD)'
```

---

## Task 2 : `oled_nav` — layer flash + activity

**Files:**
- Modify: `test/test_oled_nav.c`
- (impl already complete for these events — verify via tests)

**Interfaces:**
- Consumes: everything from Task 1.

- [ ] **Step 1: Write failing tests**

Add to `test/test_oled_nav.c` before `void test_oled_nav(void)`:
```c
/* Changement de couche → LAYER 2.5s → retour resting. */
static void test_nav_layer_flash(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    oled_nav_event(OLED_EV_LAYER_CHANGED, 3000);
    TEST_ASSERT_EQ(oled_nav_active(3000), OLED_SCR_LAYER, "flash → LAYER");
    TEST_ASSERT_EQ(oled_nav_active(5499), OLED_SCR_LAYER, "flash actif à +2499ms");
    TEST_ASSERT_EQ(oled_nav_active(5500), OLED_SCR_HOME,  "flash fini → HOME");
}

/* L'activité ne change jamais l'écran. */
static void test_nav_activity_no_switch(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    oled_nav_event(OLED_EV_ACTIVITY, 3000);
    TEST_ASSERT_EQ(oled_nav_active(3000), OLED_SCR_HOME, "activité → reste HOME");
}
```
Register both in `test_oled_nav()` with `TEST_RUN(...)`.

- [ ] **Step 2: Run to verify pass (impl from Task 1 already handles these)**

Run: `cmake --build test/build >/dev/null && ./test/build/test_runner 2>&1 | grep -E 'layer_flash|activity_no_switch|Results'`
Expected: both OK, 0 failed. (If a test fails, the impl has a bug — fix in `oled_nav.c`.)

- [ ] **Step 3: Prove they bite**

Temporarily set `OLED_NAV_FLASH_MS` to `0`. Rebuild: `test_nav_layer_flash` fails. Revert.

- [ ] **Step 4: Commit**

```bash
git add test/test_oled_nav.c .tripwire-testcount
git commit -m 'test(oled_nav): layer flash + activity no-switch'
```

---

## Task 3 : `oled_nav` — keycode cycle + idle→tama + croisements

**Files:**
- Modify: `main/display/oled/oled_nav.c` (fill `OLED_EV_DISP_KEY`)
- Modify: `test/test_oled_nav.c`

**Interfaces:**
- Consumes: Task 1 API.

- [ ] **Step 1: Write failing tests**

Add to `test/test_oled_nav.c`:
```c
/* Keycode cycle : HOME → STATS → TAMA → HOME. */
static void test_nav_dispkey_cycle(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    /* past splash */
    TEST_ASSERT_EQ(oled_nav_active(3000), OLED_SCR_HOME, "repos HOME");
    oled_nav_event(OLED_EV_DISP_KEY, 3000);
    TEST_ASSERT_EQ(oled_nav_active(3000), OLED_SCR_STATS, "cycle → STATS");
    oled_nav_event(OLED_EV_DISP_KEY, 3001);
    TEST_ASSERT_EQ(oled_nav_active(3001), OLED_SCR_TAMA,  "cycle → TAMA");
    oled_nav_event(OLED_EV_DISP_KEY, 3002);
    TEST_ASSERT_EQ(oled_nav_active(3002), OLED_SCR_HOME,  "cycle → HOME");
}

/* Le keycode coupe un flash de couche en cours. */
static void test_nav_dispkey_cuts_flash(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    oled_nav_event(OLED_EV_LAYER_CHANGED, 3000);
    TEST_ASSERT_EQ(oled_nav_active(3100), OLED_SCR_LAYER, "flash actif");
    oled_nav_event(OLED_EV_DISP_KEY, 3100);
    TEST_ASSERT_EQ(oled_nav_active(3100), OLED_SCR_STATS, "keycode coupe le flash → STATS");
}

/* Idle 30s → TAMA (si activé), retour resting à l'activité. */
static void test_nav_idle_tama(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(true);
    oled_nav_event(OLED_EV_ACTIVITY, 3000);           /* dernière activité à 3000 */
    TEST_ASSERT_EQ(oled_nav_active(32999), OLED_SCR_HOME, "avant 30s idle → HOME");
    TEST_ASSERT_EQ(oled_nav_active(33000), OLED_SCR_TAMA, "30s idle → TAMA");
    oled_nav_event(OLED_EV_ACTIVITY, 33000);
    TEST_ASSERT_EQ(oled_nav_active(33001), OLED_SCR_HOME, "activité → retour HOME");
}

/* Pas d'idle→TAMA si tama désactivé. */
static void test_nav_idle_requires_tama(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    oled_nav_event(OLED_EV_ACTIVITY, 0);
    TEST_ASSERT_EQ(oled_nav_active(40000), OLED_SCR_HOME, "tama off → pas de TAMA en idle");
}

/* Le flash de couche bat l'idle. */
static void test_nav_flash_beats_idle(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(true);
    oled_nav_event(OLED_EV_ACTIVITY, 0);
    TEST_ASSERT_EQ(oled_nav_active(35000), OLED_SCR_TAMA, "idle → TAMA");
    oled_nav_event(OLED_EV_LAYER_CHANGED, 35000);
    TEST_ASSERT_EQ(oled_nav_active(35000), OLED_SCR_LAYER, "flash bat l'idle");
}
```
Register all five in `test_oled_nav()`.

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build test/build >/dev/null && ./test/build/test_runner 2>&1 | grep -E 'dispkey|idle|flash_beats|Results'`
Expected: `dispkey_cycle` / `dispkey_cuts_flash` FAIL (DISP_KEY is a no-op). idle tests should already pass (logic from Task 1).

- [ ] **Step 3: Implement DISP_KEY**

In `main/display/oled/oled_nav.c`, add a helper above `oled_nav_event` and fill the case:
```c
static oled_screen_id_t next_resting(oled_screen_id_t r) {
    switch (r) {
    case OLED_SCR_HOME:  return OLED_SCR_STATS;
    case OLED_SCR_STATS: return OLED_SCR_TAMA;
    default:             return OLED_SCR_HOME;   /* TAMA (ou autre) → HOME */
    }
}
```
Replace the `OLED_EV_DISP_KEY` case body with:
```c
    case OLED_EV_DISP_KEY:
        s_resting       = next_resting(s_resting);
        s_splash_until  = 0;   /* couper splash */
        s_flash_until   = 0;   /* couper flash */
        s_last_activity = now_ms;
        break;
```

- [ ] **Step 4: Run to verify all pass**

Run: `cmake --build test/build >/dev/null && ./test/build/test_runner 2>&1 | grep -E 'nav|Results'`
Expected: all `test_nav_*` OK, 0 failed.

- [ ] **Step 5: Prove bite**

Temporarily make `next_resting` always `return OLED_SCR_HOME;`. Rebuild: `dispkey_cycle` fails. Revert.

- [ ] **Step 6: Commit**

```bash
git add main/display/oled/oled_nav.c test/test_oled_nav.c .tripwire-testcount
git commit -m 'feat(oled_nav): keycode cycle + idle→tama + croisements (TDD)'
```

---

## Task 4 : `oled_stats` — helpers purs (sparkline + WPM)

**Files:**
- Create: `main/display/oled/oled_stats.h`, `main/display/oled/oled_stats.c`
- Test: `test/test_oled_stats.c`
- Modify: `test/CMakeLists.txt`, `test/test_main.c`

**Interfaces:**
- Produces:
  - `void oled_sparkline_bars(const uint32_t *hist, int n, uint32_t max, uint8_t *out, int out_n);`
    (downsample `hist[n]` → `out[out_n]` valeurs 0..7, chaque bin = moyenne, échelle sur `max`)
  - `uint32_t oled_wpm_from_kpm(uint32_t kpm);` (= kpm/5, mot standard = 5 frappes)

- [ ] **Step 1: Header**

Create `main/display/oled/oled_stats.h`:
```c
#pragma once
#include <stdint.h>

/* Downsample hist[n] en out[out_n] hauteurs de barres 0..7 (moyenne par bin,
   échelle linéaire sur max ; max=0 → toutes à 0). */
void oled_sparkline_bars(const uint32_t *hist, int n, uint32_t max,
                         uint8_t *out, int out_n);

/* Mots/min ≈ frappes/min ÷ 5. */
uint32_t oled_wpm_from_kpm(uint32_t kpm);
```

- [ ] **Step 2: Failing test**

Create `test/test_oled_stats.c`:
```c
#include "test_framework.h"
#include "oled_stats.h"

static void test_wpm(void) {
    TEST_ASSERT_EQ(oled_wpm_from_kpm(0),   0u,  "0 kpm → 0 wpm");
    TEST_ASSERT_EQ(oled_wpm_from_kpm(300), 60u, "300 kpm → 60 wpm");
    TEST_ASSERT_EQ(oled_wpm_from_kpm(7),   1u,  "7 kpm → 1 wpm (troncature)");
}

static void test_sparkline_scaling(void) {
    uint32_t hist[4] = { 0, 100, 200, 400 };
    uint8_t  out[4]  = {0};
    oled_sparkline_bars(hist, 4, 400, out, 4);
    TEST_ASSERT_EQ(out[0], 0, "0/400 → 0");
    TEST_ASSERT_EQ(out[3], 7, "400/400 → 7 (plein)");
    TEST_ASSERT(out[1] < out[2] && out[2] < out[3], "monotone croissant");
}

static void test_sparkline_max_zero(void) {
    uint32_t hist[2] = { 5, 9 };
    uint8_t  out[2]  = { 3, 3 };
    oled_sparkline_bars(hist, 2, 0, out, 2);
    TEST_ASSERT_EQ(out[0], 0, "max=0 → 0");
    TEST_ASSERT_EQ(out[1], 0, "max=0 → 0");
}

void test_oled_stats(void) {
    TEST_SUITE("OLED stats — helpers purs");
    TEST_RUN(test_wpm);
    TEST_RUN(test_sparkline_scaling);
    TEST_RUN(test_sparkline_max_zero);
}
```
Register in `test_main.c` (`extern void test_oled_stats(void);` + `test_oled_stats();`) and add `test_oled_stats.c` + `../main/display/oled/oled_stats.c` to `test/CMakeLists.txt`.

- [ ] **Step 3: Run → fail**

Run: `rm -rf test/build && cmake -S test -B test/build >/dev/null && cmake --build test/build 2>&1 | tail -3`
Expected: link error (functions undefined).

- [ ] **Step 4: Implement**

Create `main/display/oled/oled_stats.c`:
```c
#include "oled_stats.h"

uint32_t oled_wpm_from_kpm(uint32_t kpm) { return kpm / 5u; }

void oled_sparkline_bars(const uint32_t *hist, int n, uint32_t max,
                         uint8_t *out, int out_n) {
    if (out_n <= 0) return;
    for (int b = 0; b < out_n; b++) {
        if (max == 0 || n <= 0) { out[b] = 0; continue; }
        int start = (int)((long)b * n / out_n);
        int end   = (int)((long)(b + 1) * n / out_n);
        if (end <= start) end = start + 1;
        uint64_t sum = 0; int cnt = 0;
        for (int i = start; i < end && i < n; i++) { sum += hist[i]; cnt++; }
        uint32_t avg = cnt ? (uint32_t)(sum / cnt) : 0;
        uint32_t bar = (avg * 7u) / max;   /* 0..7 */
        out[b] = bar > 7 ? 7 : (uint8_t)bar;
    }
}
```

- [ ] **Step 5: Run → pass**

Run: `cmake --build test/build >/dev/null && ./test/build/test_runner 2>&1 | grep -E 'wpm|sparkline|Results'`
Expected: all OK, 0 failed.

- [ ] **Step 6: Prove bite**

Temporarily change `(avg * 7u) / max` to `0`. Rebuild: `test_sparkline_scaling` fails. Revert.

- [ ] **Step 7: Commit**

```bash
git add main/display/oled/oled_stats.* test/test_oled_stats.c test/test_main.c test/CMakeLists.txt .tripwire-testcount
git commit -m 'feat(oled): oled_stats pure helpers — sparkline + wpm (TDD)'
```

---

## Task 5 : Vtable `notify_display_key` + coordinateur

**Files:**
- Modify: `main/display/display_backend.h` (add callback)
- Modify: `main/display/status_display.h` (declare), `main/display/status_display.c` (implement)
- Modify: `main/display/round/round_backend.c` (add `.notify_display_key = NULL` or a no-op — keep V1 compiling)

**Interfaces:**
- Produces: `void status_display_notify_display_key(void);` (appelle `backend->notify_display_key` si non-NULL).

- [ ] **Step 1: Add the callback to the vtable**

In `main/display/display_backend.h`, add after `notify_keypress`:
```c
    void (*notify_display_key)(void);               /* K_DISP_NEXT — cycle l'écran */
```

- [ ] **Step 2: Coordinator declaration + impl**

In `main/display/status_display.h`, add after `status_display_notify_keypress`:
```c
void status_display_notify_display_key(void);
```
In `main/display/status_display.c`, mirror an existing `notify_*` (find `status_display_notify_keypress` and copy its shape):
```c
void status_display_notify_display_key(void) {
    const display_backend_t *b = display_get_backend();
    if (b && b->notify_display_key) b->notify_display_key();
}
```

- [ ] **Step 3: Keep round backend compiling**

In `main/display/round/round_backend.c`, in the `display_backend_t` initializer, add `.notify_display_key = NULL,` (round display ignores it). If the round backend uses positional init, add a matching no-op or NULL entry.

- [ ] **Step 4: Board build**

Run: `source ~/esp/esp-idf/export.sh && ./scripts/check.sh --board kase_v2 2>&1 | tail -3`
Expected: `✓ check.sh: tout vert`. Also spot-check V1 compiles: `./scripts/check.sh --board kase_v1 2>&1 | tail -3`.

- [ ] **Step 5: Commit**

```bash
git add main/display/display_backend.h main/display/status_display.h main/display/status_display.c main/display/round/round_backend.c
git commit -m 'feat(display): notify_display_key vtable hook + coordinator'
```

---

## Task 6 : Keycode `K_DISP_NEXT`

**Files:**
- Modify: `main/input/key_definitions.h` (add `K_DISP_NEXT 0x3F00`)
- Modify: `main/input/keyboard_actions.h`, `main/input/keyboard_actions.c` (add `km_post_display_next`)
- Modify: `main/input/key_processor.c` (`detect_internal_function` switch)
- Modify: the display/keyboard-manager consumer that runs `km_post_display_update` actions → also handle the new action by calling `status_display_notify_display_key()`.

**Interfaces:**
- Consumes: `status_display_notify_display_key()` (Task 5).
- Produces: `void km_post_display_next(void);`

- [ ] **Step 1: Add the keycode**

In `main/input/key_definitions.h`, after the `K_SEC_*` block, add:
```c
/* Display: cycle l'écran OLED de repos (HOME→STATS→TAMA) — 0x3F00 (libre) */
#define K_DISP_NEXT                  0x3F00
```

- [ ] **Step 2: Add km_post_display_next (mirror km_post_display_update)**

Read `main/input/keyboard_actions.c:63` (`km_post_display_update`) to see the action-queue mechanism. Add a parallel action enum value + a `km_post_display_next(void)` that posts it, and in the consumer (same file/task that reacts to `km_post_display_update`) handle it by calling `status_display_notify_display_key()`. Declare `void km_post_display_next(void);` in `keyboard_actions.h`.

- [ ] **Step 3: Detect the keycode**

In `main/input/key_processor.c`, `detect_internal_function` switch (around line 104), add a case:
```c
    case K_DISP_NEXT:
        km_post_display_next();
        break;
```
Ensure `K_DISP_NEXT` is recognized as an internal function (added to the same detection path as other `0x3?00` internal keycodes; follow how `K_TAMA_FEED`/`K_LAYER_LOCK` are matched).

- [ ] **Step 4: Board build + optional pipeline test**

Run: `./scripts/check.sh --board kase_v2 2>&1 | tail -3` → vert.
(Optional host test: in `test/test_keycode_report.c`, press a key mapped to `K_DISP_NEXT` and assert it is absorbed as an internal function — only if the internal-function detection is host-reachable; otherwise rely on board build.)

- [ ] **Step 5: Commit**

```bash
git add main/input/key_definitions.h main/input/keyboard_actions.h main/input/keyboard_actions.c main/input/key_processor.c
git commit -m 'feat(input): K_DISP_NEXT keycode → cycle OLED screen'
```

---

## Task 7 : Interface d'écran + manager `oled_screens` (+ module `oled_kpm`)

**Files:**
- Create: `main/display/oled/screens/oled_screen.h` (interface + helpers partagés)
- Create: `main/display/oled/oled_screens.h`, `main/display/oled/oled_screens.c`
- Create: `main/display/oled/oled_kpm.h`, `main/display/oled/oled_kpm.c` (fenêtre KPM partagée par HOME + STATS ; interface définie ci-dessous, impl reprise de l'ancien `oled_backend`)

**Note ordering :** `oled_kpm` est créé ICI (avant les écrans) car HOME (Task 9) et STATS (Task 11) le consomment tous les deux, et le manager le `tick`.

**Interfaces:**
- Produces:
  - `oled_screen.h`:
    ```c
    typedef struct {
        void (*build)(lv_obj_t *parent);
        void (*update)(void);
        void (*destroy)(void);
    } oled_screen_t;
    lv_obj_t *oled_make_card(lv_obj_t *parent, int x, int y, int w, int h, int radius);
    ```
  - `oled_screens.h`:
    ```c
    void oled_screens_reset(uint32_t now_ms);    /* init nav + détruit l'écran courant */
    void oled_screens_tick(uint32_t now_ms);     /* build/destroy sur changement, sinon update (lock LVGL tenu par l'appelant) */
    void oled_screens_layer_changed(uint32_t now_ms);
    void oled_screens_disp_key(uint32_t now_ms);
    void oled_screens_activity(uint32_t now_ms);
    ```

- [ ] **Step 1: Screen interface + shared card helper**

Create `main/display/oled/screens/oled_screen.h` with the `oled_screen_t` struct (above) and declare `oled_make_card` (move the `make_card` body from the old `oled_backend.c` into `oled_screens.c` and export it here).

- [ ] **Step 2: Manager**

Create `main/display/oled/oled_screens.c`:
- Include `oled_nav.h`, `oled_screen.h`, `lvgl.h`, `esp_lvgl_port.h`, and each screen's extern `const oled_screen_t screen_*;`.
- Static registry: `static const oled_screen_t *s_screens[OLED_SCR_COUNT];` filled with `&screen_splash`, `&screen_home`, `&screen_layer`, `&screen_stats`, `&screen_tama` (externs from Tasks 8-12).
- `static oled_screen_id_t s_current = OLED_SCR_COUNT;` (none built).
- `s_container`: a full-screen `lv_obj` created on reset (parent for screens), or use `lv_scr_act()` directly.
- `oled_screens_reset(now)`: destroy current (if any), `oled_nav_init(now)`, `oled_nav_set_tama_enabled(tama_engine_is_enabled())`, `s_current=OLED_SCR_COUNT`.
- `oled_screens_tick(now)`: `id = oled_nav_active(now)`; if `id != s_current` → `if valid: s_screens[s_current]->destroy()`; `s_screens[id]->build(lv_scr_act())`; `s_current=id`; then `s_screens[s_current]->update()`.
- `oled_screens_layer_changed/disp_key/activity(now)`: forward to `oled_nav_event(...)`.

Provide `oled_make_card` (moved from old backend):
```c
lv_obj_t *oled_make_card(lv_obj_t *parent, int x, int y, int w, int h, int radius) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h); lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_opa(card, LV_OPA_0, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, radius, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}
```

- [ ] **Step 3: Add sources to the firmware build**

In `main/CMakeLists.txt`, add the new files to `SRCS` (or the glob covers `display/oled/*.c` and `display/oled/screens/*.c` — verify; if not glob, add each: `oled_nav.c oled_stats.c oled_screens.c screens/screen_splash.c … `). This task can't board-build alone (screens not written yet) — build happens in Task 13. Commit compiles as part of the final wiring.

- [ ] **Step 4: Commit**

```bash
git add main/display/oled/screens/oled_screen.h main/display/oled/oled_screens.* main/CMakeLists.txt
git commit -m 'feat(oled): screen interface + manager (nav-driven build/update/destroy)'
```

---

## Tasks 8–12 : Modules d'écran

Chaque écran expose `const oled_screen_t screen_<name>;` avec `build/update/destroy`. Ils dessinent en mono (`lv_color_black()` = allumé), sous le lock LVGL déjà tenu par `oled_screens_tick`. `destroy` supprime les objets (`lv_obj_del`) et remet les pointeurs statiques à NULL.

### Task 8 : `screen_splash.c`
**Files:** Create `main/display/oled/screens/screen_splash.c`
- `build`: `#include "esp_app_desc.h"`. Deux labels centrés : "KaSe" (font_28) haut, `esp_app_get_description()->version` (UI_FONT) sous.
- `update`: no-op (statique).
- `destroy`: `lv_obj_del` des 2 labels, pointeurs → NULL.
- Board build report en Task 13. Commit : `feat(oled): screen_splash (nom + version firmware)`.

### Task 9 : `screen_home.c`
**Files:** Create `main/display/oled/screens/screen_home.c`
- Reprend les éléments de statut de l'ancien backend (`oled_update_connection_icons` logique) : icône chemin (`flash`/`wifi`/`bluetooth_16px`), slot BT, indicateur caps (`hid_led_state & HID_LED_CAPS_LOCK`), souris. `oled_make_card` autour du nom de couche (`default_layout_names[current_layout]`). Mini sparkline KPM (via `oled_sparkline_bars`, dessinée en petit — barres `lv_obj` ou un canvas ; MVP : un `lv_bar` compact + valeur KPM).
- `update`: rafraîchit connexion/caps/souris/KPM (déplacer la logique de `oled_update_connection_icons` + les indicateurs de `oled_update`).
- `destroy`: supprime tous ses objets.
- Commit : `feat(oled): screen_home (statut d'un coup d'œil)`.

### Task 10 : `screen_layer.c`
**Files:** Create `main/display/oled/screens/screen_layer.c`
- `build`: grand label centré = `default_layout_names[current_layout]` (font_28), sous-titre `lv_label_set_text_fmt(sub, "> layer %d", current_layout)` (UI_FONT).
- `update`: relit `current_layout` (au cas où change pendant le flash) et met à jour les deux labels.
- `destroy`: supprime.
- Commit : `feat(oled): screen_layer (nom de couche en grand)`.

### Task 11 : `screen_stats.c`
**Files:** Create `main/display/oled/screens/screen_stats.c`

Le module `oled_kpm` (interface ci-dessous) a été créé en Task 7 ; le manager le
`tick` et `oled_backend.notify_keypress` appelle `oled_kpm_keypress()`.
```c
/* oled_kpm.h (créé Task 7) */
#define OLED_KPM_WINDOW 60
void            oled_kpm_reset(uint32_t now_ms);
void            oled_kpm_keypress(void);                 /* +1 sur le bin courant */
void            oled_kpm_tick(uint32_t now_ms);          /* échantillonne toutes les 1000ms */
uint32_t        oled_kpm_value(void);                    /* somme fenêtre = KPM */
const uint32_t *oled_kpm_history(int *out_n);            /* pour la sparkline */
```

`screen_stats.c` (consomme `oled_stats.h` + `oled_kpm.h`) : KPM (`oled_kpm_value`),
WPM (`oled_wpm_from_kpm(oled_kpm_value())`), sparkline (`oled_sparkline_bars` sur
`oled_kpm_history` → barres dessinées en `lv_obj` fins ou un mini-canvas ; MVP : un
`lv_bar` par bin, ou un seul `lv_bar` KPM si la sparkline pixel est trop lourde),
total de frappes (`key_stats` total).
- `build`/`update`/`destroy` en conséquence.
- Commit : `feat(oled): oled_kpm module + screen_stats (KPM/WPM/sparkline/total)`.

### Task 12 : `screen_tama.c`
**Files:** Create `main/display/oled/screens/screen_tama.c`
- `build`: crée le conteneur tama plein/grand et appelle `tama_render_create(parent, W, H)`. Barres faim/bonheur (`tama_engine_get_stats`), niveau, tama-time.
- `update`: `tama_render_update(tama_engine_get_state(), tama_engine_get_stats(), tama_engine_get_critter())` + barres.
- `destroy`: `tama_render_destroy()` + supprime les barres.
- Commit : `feat(oled): screen_tama (pet plein écran)`.

---

## Task 13 : Réécrire `oled_backend.c` (mince) + assembler

**Files:**
- Modify (rewrite): `main/display/oled/oled_backend.c`

**Interfaces:**
- Consumes: `oled_screens_*` (Task 7), `oled_nav.h`.

- [ ] **Step 1: Réécrire le backend mince**

Remplacer tout l'intérieur de `oled_backend.c` (garder `oled_init` panel/hw config, `oled_sleep`/`oled_wake`, `oled_show_dfu`). Nouveau flux :
- `oled_refresh_all()` / `oled_init` post-hw : sous lock LVGL → `display_clear_screen()` puis `oled_screens_reset(now_ms())` (où `now_ms()` = `xTaskGetTickCount()` en ms).
- `oled_update()` : `oled_screens_activity`? non — `update` est le tick périodique : sous lock LVGL → `oled_screens_tick(now_ms())`.
- `oled_update_layer()` : `oled_screens_layer_changed(now_ms())` (puis un tick pour rebâtir tout de suite).
- `oled_notify_keypress()` : compteur KPM/tama + `oled_screens_activity(now_ms())`.
- `oled_notify_mouse()` : `oled_screens_activity(now_ms())` + marque l'activité souris (pour l'indicateur HOME).
- `oled_notify_display_key()` (NOUVEAU, câblé au vtable) : `oled_screens_disp_key(now_ms())`.
- Ajouter `.notify_display_key = oled_notify_display_key` dans le `display_backend_t oled_display_backend`.
- `oled_sleep`/`oled_wake` : appeler `oled_screens_reset` au wake ; au sleep, détruire l'écran courant + power off panel (garder la logique existante).

- [ ] **Step 2: Board build (V2 + V2D)**

Run: `./scripts/check.sh --board kase_v2 2>&1 | tail -3` puis `./scripts/check.sh --board kase_v2_debug 2>&1 | tail -3`
Expected: les deux verts. Corriger les erreurs de link (screens externs, includes) jusqu'au vert.

- [ ] **Step 3: Full host + tout board**

Run: `./scripts/check.sh 2>&1 | tail -5`
Expected: 6 boards + host verts.

- [ ] **Step 4: Commit**

```bash
git add main/display/oled/oled_backend.c
git commit -m 'feat(oled): backend mince — pilote le manager multi-écrans'
```

---

## Task 14 : Smoke-test hardware + doc

**Files:**
- Modify: `docs/HARDWARE_SMOKE_TEST.md`

- [ ] **Step 1: Ajouter les points OLED**

Sous la section V2/V2D, ajouter :
```markdown
## OLED refonte (V2 / V2D)
- [ ] Boot : splash "KaSe" + version ~2s, puis HOME
- [ ] HOME : connexion (USB/BLE/RF + slot), couche, caps, souris, mini-KPM
- [ ] Changement de couche → écran LAYER (nom en grand) ~2.5s puis retour
- [ ] Keycode K_DISP_NEXT : cycle HOME → STATS → TAMA → HOME
- [ ] STATS : KPM/WPM bougent en tapant, sparkline se remplit, total croît
- [ ] Idle ~30s (tama activé) → écran TAMA ; 1ʳᵉ frappe → retour écran de repos
- [ ] Idle avec tama désactivé → reste sur l'écran de repos
- [ ] Pas de scintillement / rebuild en boucle entre deux écrans
- [ ] Sleep/wake : écran se reconstruit correctement au réveil
```

- [ ] **Step 2: Commit**

```bash
git add docs/HARDWARE_SMOKE_TEST.md
git commit -m 'docs(smoke): points de test OLED refonte'
```

---

## Ordre & dépendances

1→2→3 (nav pur, TDD) · 4 (stats pur, TDD) — indépendants, font en premier.
5 (vtable hook) → 6 (keycode). 7 (manager) → 8-12 (écrans) → 13 (backend assemble) → 14 (doc).
Le firmware ne compile en entier qu'à partir de la Task 13 (les écrans se référencent). Tasks 1-4 sont vertes en host isolément ; 5-6 board-buildent isolément ; 7-12 compilent au board seulement une fois 13 faite.

## Self-review (couverture spec)

- Spec §3.1 interface écran → Task 7. §3.2 nav pure → Tasks 1-3 (+ tests). §3.3 flux rendu → Tasks 7,13.
- Spec §4 les 5 écrans → Tasks 8-12 (splash/home/layer/stats/tama). Version au splash → Task 8.
- Spec §5 keycode → Tasks 5,6. §6 tests → Tasks 1-4 (nav+stats host) + §6 rendu → Tasks 13 (board) + 14 (smoke).
- Spec §7 YAGNI (pas de lock, pas de round, pas de /jour) : respecté (aucune tâche ne les fait).
- Spec §8 risques (RAM, churn, lv_obj_is_valid) : Task 13 board build (RAM), Task 7 manager ne rebuild qu'au changement (churn), Global Constraints (lv_obj_is_valid).
