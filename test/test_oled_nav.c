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

void test_oled_nav(void) {
    TEST_SUITE("OLED nav — machine à états pure");
    TEST_RUN(test_nav_boot_splash_then_home);
    TEST_RUN(test_nav_layer_flash);
    TEST_RUN(test_nav_activity_no_switch);
    TEST_RUN(test_nav_dispkey_cycle);
    TEST_RUN(test_nav_dispkey_cuts_flash);
    TEST_RUN(test_nav_idle_tama);
    TEST_RUN(test_nav_idle_requires_tama);
    TEST_RUN(test_nav_flash_beats_idle);
}
