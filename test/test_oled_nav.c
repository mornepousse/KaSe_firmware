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

void test_oled_nav(void) {
    TEST_SUITE("OLED nav — machine à états pure");
    TEST_RUN(test_nav_boot_splash_then_home);
    TEST_RUN(test_nav_layer_flash);
    TEST_RUN(test_nav_activity_no_switch);
}
