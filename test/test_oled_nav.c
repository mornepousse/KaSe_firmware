#include "test_framework.h"
#include "oled_nav.h"

/* oled_nav_init N'ARME PAS le splash (cas réveil/refresh) → HOME direct. */
static void test_nav_init_no_splash(void) {
    oled_nav_init(0);
    TEST_ASSERT_EQ(oled_nav_active(0),    OLED_SCR_HOME, "init seul → HOME (pas de splash au réveil)");
    TEST_ASSERT_EQ(oled_nav_active(1000), OLED_SCR_HOME, "toujours HOME sans BOOT");
}

/* Boot (OLED_EV_BOOT) → SPLASH pendant 2s → HOME. */
static void test_nav_boot_splash_then_home(void) {
    oled_nav_init(0);
    oled_nav_event(OLED_EV_BOOT, 0);
    TEST_ASSERT_EQ(oled_nav_active(0),    OLED_SCR_SPLASH, "BOOT → SPLASH");
    TEST_ASSERT_EQ(oled_nav_active(1999), OLED_SCR_SPLASH, "t=1999 → SPLASH");
    TEST_ASSERT_EQ(oled_nav_active(2000), OLED_SCR_HOME,   "t=2000 → HOME (splash fini)");
    TEST_ASSERT_EQ(oled_nav_active(9000), OLED_SCR_HOME,   "repos = HOME");
}

/* L'activité / changement de couche ne changent jamais l'écran (plus d'idle-tama). */
static void test_nav_activity_no_switch(void) {
    oled_nav_init(0);
    oled_nav_event(OLED_EV_ACTIVITY, 3000);
    TEST_ASSERT_EQ(oled_nav_active(3000), OLED_SCR_HOME, "activité → reste HOME");
    oled_nav_event(OLED_EV_LAYER_CHANGED, 5000);
    TEST_ASSERT_EQ(oled_nav_active(50000), OLED_SCR_HOME, "long idle → toujours HOME (plus de TAMA)");
}

/* Keycode cycle : HOME → STATS → HOME. */
static void test_nav_dispkey_cycle(void) {
    oled_nav_init(0);
    TEST_ASSERT_EQ(oled_nav_active(3000), OLED_SCR_HOME, "repos HOME");
    oled_nav_event(OLED_EV_DISP_KEY, 3000);
    TEST_ASSERT_EQ(oled_nav_active(3000), OLED_SCR_STATS, "cycle → STATS");
    oled_nav_event(OLED_EV_DISP_KEY, 3001);
    TEST_ASSERT_EQ(oled_nav_active(3001), OLED_SCR_HOME,  "cycle → HOME");
}

/* Le keycode coupe un splash de boot en cours. */
static void test_nav_dispkey_cuts_splash(void) {
    oled_nav_init(0);
    oled_nav_event(OLED_EV_BOOT, 0);
    TEST_ASSERT_EQ(oled_nav_active(100), OLED_SCR_SPLASH, "splash actif");
    oled_nav_event(OLED_EV_DISP_KEY, 100);
    TEST_ASSERT_EQ(oled_nav_active(100), OLED_SCR_STATS, "keycode coupe le splash → STATS");
}

void test_oled_nav(void) {
    TEST_SUITE("OLED nav — machine à états pure");
    TEST_RUN(test_nav_init_no_splash);
    TEST_RUN(test_nav_boot_splash_then_home);
    TEST_RUN(test_nav_activity_no_switch);
    TEST_RUN(test_nav_dispkey_cycle);
    TEST_RUN(test_nav_dispkey_cuts_splash);
}
