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
