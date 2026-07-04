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
