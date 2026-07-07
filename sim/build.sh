#!/usr/bin/env bash
# Rend les 5 écrans OLED en PNG (sim/img_*.png). Usage: bash sim/build.sh
set -e
cd "$(dirname "$0")/.."
[ -f sim/liblvgl.a ] || { echo "liblvgl.a manquant — build LVGL d'abord"; exit 1; }
gcc -O1 -w -Isim/stubs -Isim -Imain/display/oled -Imain/display/oled/screens -Imain/display/assets -Imanaged_components/lvgl__lvgl \
  sim/oled_sim.c sim/render_screens.c sim/sim_stubs.c \
  main/display/oled/oled_kpm.c main/display/oled/oled_stats.c \
  main/display/oled/screens/screen_splash.c main/display/oled/screens/screen_home.c \
  main/display/oled/screens/screen_stats.c \
  main/display/assets/img_usb.c main/display/assets/img_signal.c main/display/assets/img_bluetooth.c \
  sim/liblvgl.a -lm -o sim/render_screens
./sim/render_screens
