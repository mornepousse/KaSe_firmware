#include "test_framework.h"
#include "oled_nav.h"

/* oled_nav_init N'ARME PAS le splash (cas réveil/refresh) → HOME direct.
   Le splash n'apparaît QUE sur OLED_EV_BOOT (vrai démarrage). */
static void test_nav_init_no_splash(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    TEST_ASSERT_EQ(oled_nav_active(0),    OLED_SCR_HOME, "init seul → HOME (pas de splash au réveil)");
    TEST_ASSERT_EQ(oled_nav_active(1000), OLED_SCR_HOME, "toujours HOME sans BOOT");
}

/* Boot (OLED_EV_BOOT) → SPLASH pendant 2s → HOME. */
static void test_nav_boot_splash_then_home(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    oled_nav_event(OLED_EV_BOOT, 0);
    TEST_ASSERT_EQ(oled_nav_active(0),    OLED_SCR_SPLASH, "BOOT → SPLASH");
    TEST_ASSERT_EQ(oled_nav_active(1999), OLED_SCR_SPLASH, "t=1999 → SPLASH");
    TEST_ASSERT_EQ(oled_nav_active(2000), OLED_SCR_HOME,   "t=2000 → HOME (splash fini)");
    TEST_ASSERT_EQ(oled_nav_active(9000), OLED_SCR_HOME,   "repos = HOME");
}

/* Changement de couche = activité : ne bascule PAS d'écran (HOME affiche déjà
   la couche) et remet le minuteur d'inactivité. */
static void test_nav_layer_change_is_activity(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(true);
    oled_nav_event(OLED_EV_ACTIVITY, 0);
    /* Un changement de couche à t=20000 remet le minuteur d'inactivité :
       juste avant le seuil suivant → toujours HOME ; au seuil → TAMA. */
    oled_nav_event(OLED_EV_LAYER_CHANGED, 20000);
    TEST_ASSERT_EQ(oled_nav_active(20000), OLED_SCR_HOME, "changement de couche → reste HOME");
    TEST_ASSERT_EQ(oled_nav_active(20000 + OLED_NAV_IDLE_MS - 1), OLED_SCR_HOME, "layer-change a remis l'idle (pas TAMA)");
    TEST_ASSERT_EQ(oled_nav_active(20000 + OLED_NAV_IDLE_MS),     OLED_SCR_TAMA, "seuil après le layer-change → TAMA");
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
    TEST_ASSERT_EQ(oled_nav_active(3000), OLED_SCR_HOME, "repos HOME");
    oled_nav_event(OLED_EV_DISP_KEY, 3000);
    TEST_ASSERT_EQ(oled_nav_active(3000), OLED_SCR_STATS, "cycle → STATS");
    oled_nav_event(OLED_EV_DISP_KEY, 3001);
    TEST_ASSERT_EQ(oled_nav_active(3001), OLED_SCR_TAMA,  "cycle → TAMA");
    oled_nav_event(OLED_EV_DISP_KEY, 3002);
    TEST_ASSERT_EQ(oled_nav_active(3002), OLED_SCR_HOME,  "cycle → HOME");
}

/* Le keycode coupe un splash de boot en cours. */
static void test_nav_dispkey_cuts_splash(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    oled_nav_event(OLED_EV_BOOT, 0);
    TEST_ASSERT_EQ(oled_nav_active(100), OLED_SCR_SPLASH, "splash actif");
    oled_nav_event(OLED_EV_DISP_KEY, 100);
    TEST_ASSERT_EQ(oled_nav_active(100), OLED_SCR_STATS, "keycode coupe le splash → STATS");
}

/* Idle 30s → TAMA (si activé), retour resting à l'activité. */
static void test_nav_idle_tama(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(true);
    oled_nav_event(OLED_EV_ACTIVITY, 3000);
    TEST_ASSERT_EQ(oled_nav_active(3000 + OLED_NAV_IDLE_MS - 1), OLED_SCR_HOME, "avant seuil idle → HOME");
    TEST_ASSERT_EQ(oled_nav_active(3000 + OLED_NAV_IDLE_MS),     OLED_SCR_TAMA, "seuil idle → TAMA");
    oled_nav_event(OLED_EV_ACTIVITY, 3000 + OLED_NAV_IDLE_MS);
    TEST_ASSERT_EQ(oled_nav_active(3001 + OLED_NAV_IDLE_MS), OLED_SCR_HOME, "activité → retour HOME");
}

/* Pas d'idle→TAMA si tama désactivé. */
static void test_nav_idle_requires_tama(void) {
    oled_nav_init(0);
    oled_nav_set_tama_enabled(false);
    oled_nav_event(OLED_EV_ACTIVITY, 0);
    TEST_ASSERT_EQ(oled_nav_active(40000), OLED_SCR_HOME, "tama off → pas de TAMA en idle");
}

void test_oled_nav(void) {
    TEST_SUITE("OLED nav — machine à états pure");
    TEST_RUN(test_nav_init_no_splash);
    TEST_RUN(test_nav_boot_splash_then_home);
    TEST_RUN(test_nav_layer_change_is_activity);
    TEST_RUN(test_nav_activity_no_switch);
    TEST_RUN(test_nav_dispkey_cycle);
    TEST_RUN(test_nav_dispkey_cuts_splash);
    TEST_RUN(test_nav_idle_tama);
    TEST_RUN(test_nav_idle_requires_tama);
}
