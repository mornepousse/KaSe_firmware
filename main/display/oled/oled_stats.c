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
