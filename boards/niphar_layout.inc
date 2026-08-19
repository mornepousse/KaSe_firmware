/*
 * Niphargus moitié gauche — physical key layout.
 * 4 rows × 7 cols electrical grid; 3 electrical positions unused (row2/col6,
 * row3/col5, row3/col6) — see boards/niphar_left/board_keymap.c.
 *
 * Format v2: direct x/y positioning with optional rotation (r) and width (w).
 * Top-level "keys" = individually positioned keys (thumb cluster + the two
 * bracket keys of the pinky-extension column, which don't span the full
 * column height). "groups" = vertical finger columns (rows 0..2) sharing a
 * common origin and rotation.
 *
 * Requires: PRODUCT_NAME, MATRIX_ROWS, MATRIX_COLS, STR() defined.
 */
const char board_layout_json[] =
"{"
  "\"name\":\"" PRODUCT_NAME "\","
  "\"rows\":" STR(MATRIX_ROWS) ","
  "\"cols\":" STR(MATRIX_COLS) ","
  "\"keys\":["
    "{\"row\":3,\"col\":0,\"x\":0.3,\"y\":3.6,\"r\":20.0},"
    "{\"row\":3,\"col\":1,\"x\":1.3,\"y\":3.8,\"r\":10.0},"
    "{\"row\":3,\"col\":2,\"x\":2.3,\"y\":3.9,\"w\":1.2},"
    "{\"row\":3,\"col\":3,\"x\":3.6,\"y\":3.9,\"r\":-10.0},"
    "{\"row\":3,\"col\":4,\"x\":4.6,\"y\":3.7,\"r\":-20.0,\"w\":1.4},"
    "{\"row\":0,\"col\":6,\"x\":6.0,\"y\":0.0},"
    "{\"row\":1,\"col\":6,\"x\":6.0,\"y\":1.0}"
  "],"
  "\"groups\":["
    "{\"x\":0.0,\"y\":0.3,\"keys\":["
      "{\"row\":0,\"col\":0,\"x\":0,\"y\":0},"
      "{\"row\":1,\"col\":0,\"x\":0,\"y\":1},"
      "{\"row\":2,\"col\":0,\"x\":0,\"y\":2}"
    "]},"
    "{\"x\":1.0,\"y\":0.15,\"keys\":["
      "{\"row\":0,\"col\":1,\"x\":0,\"y\":0},"
      "{\"row\":1,\"col\":1,\"x\":0,\"y\":1},"
      "{\"row\":2,\"col\":1,\"x\":0,\"y\":2}"
    "]},"
    "{\"x\":2.0,\"y\":0.0,\"keys\":["
      "{\"row\":0,\"col\":2,\"x\":0,\"y\":0},"
      "{\"row\":1,\"col\":2,\"x\":0,\"y\":1},"
      "{\"row\":2,\"col\":2,\"x\":0,\"y\":2}"
    "]},"
    "{\"x\":3.0,\"y\":0.05,\"keys\":["
      "{\"row\":0,\"col\":3,\"x\":0,\"y\":0},"
      "{\"row\":1,\"col\":3,\"x\":0,\"y\":1},"
      "{\"row\":2,\"col\":3,\"x\":0,\"y\":2}"
    "]},"
    "{\"x\":4.0,\"y\":0.2,\"keys\":["
      "{\"row\":0,\"col\":4,\"x\":0,\"y\":0},"
      "{\"row\":1,\"col\":4,\"x\":0,\"y\":1},"
      "{\"row\":2,\"col\":4,\"x\":0,\"y\":2}"
    "]},"
    "{\"x\":5.0,\"y\":0.4,\"keys\":["
      "{\"row\":0,\"col\":5,\"x\":0,\"y\":0},"
      "{\"row\":1,\"col\":5,\"x\":0,\"y\":1},"
      "{\"row\":2,\"col\":5,\"x\":0,\"y\":2}"
    "]}"
  "]"
"}"
;
