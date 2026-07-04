#pragma once
#include <stdint.h>

/* Downsample hist[n] en out[out_n] hauteurs de barres 0..7 (moyenne par bin,
   échelle linéaire sur max ; max=0 → toutes à 0).
   Contrat d'appel : out_n <= n (downsampling). Avec out_n > n (upsampling) les
   bins vides sortent à 0 — pas le cas d'usage prévu. */
void oled_sparkline_bars(const uint32_t *hist, int n, uint32_t max,
                         uint8_t *out, int out_n);

/* Mots/min ≈ frappes/min ÷ 5. */
uint32_t oled_wpm_from_kpm(uint32_t kpm);
