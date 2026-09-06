#include "rf_probe.h"
#include "board.h"
#include "rf_driver.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rf_probe";

/* Canal et suffixe repris de boards/kase_dongle/board_rf.h (radio 1) et de
 * comm/rf/rf_slot.h : le slot 0x01 est le clavier. Ils n'ont aucune importance
 * pour le probe lui-même — qui ne lit que deux registres — mais évitent de
 * laisser une config à moitié fausse dans l'arbre. */
#define RF_PROBE_CHANNEL      0x4C   /* 2476 MHz, slot clavier du dongle */
#define RF_PROBE_ADDR_SUFFIX  0x01


/* Test de lignes — cherche les ponts entre les six signaux de la radio.
 * Chaque paire est testee dans LES DEUX SENS : on force A haut avec B en entree
 * tiree bas, puis A bas avec B tire haut. Un pont n'est declare que si B suit A
 * dans les deux cas, ce qui rend le test immunise au pull-up propre d'une ligne
 * (l'IRQ du nRF24 est haut au repos). Repris de rf_line_test.c, ramene a une
 * seule radio : les moities Niphargus n'en portent qu'une, la ou le dongle en a
 * deux et fait passer le test par board_rf_radio1/2_cfg().
 *
 * Remet toutes les broches a zero en sortant, pour que l'init SPI/radio qui
 * suit puisse les reclamer. */
static void rf_probe_lines(void)
{
    const struct { int gpio; const char *name; } ln[] = {
        { BOARD_NRF_MOSI, "MOSI" }, { BOARD_NRF_MISO, "MISO" },
        { BOARD_NRF_SCK,  "SCK"  }, { BOARD_NRF_CSN,  "CSN"  },
        { BOARD_NRF_CE,   "CE"   }, { BOARD_NRF_IRQ,  "IRQ"  },
    };
    const int N = (int)(sizeof(ln) / sizeof(ln[0]));

    ESP_LOGW(TAG, "--- test de lignes (%d lignes, bidirectionnel) ---", N);
    int shorts = 0;
    for (int a = 0; a < N; a++) {
        for (int b = 0; b < N; b++) {
            if (a == b) continue;
            gpio_reset_pin(ln[a].gpio); gpio_reset_pin(ln[b].gpio);
            gpio_set_direction(ln[a].gpio, GPIO_MODE_OUTPUT);
            gpio_set_direction(ln[b].gpio, GPIO_MODE_INPUT);

            gpio_set_pull_mode(ln[b].gpio, GPIO_PULLDOWN_ONLY);
            gpio_set_level(ln[a].gpio, 1);
            vTaskDelay(pdMS_TO_TICKS(2));
            int hi = gpio_get_level(ln[b].gpio);

            gpio_set_pull_mode(ln[b].gpio, GPIO_PULLUP_ONLY);
            gpio_set_level(ln[a].gpio, 0);
            vTaskDelay(pdMS_TO_TICKS(2));
            int lo = gpio_get_level(ln[b].gpio);

            if (hi == 1 && lo == 0 && a < b) {
                ESP_LOGW(TAG, "PONT : %s(%d) <-> %s(%d)",
                         ln[a].name, ln[a].gpio, ln[b].name, ln[b].gpio);
                shorts++;
            }
        }
    }

    /* Etat au repos de chaque ligne, entree flottante puis tiree : distingue une
     * ligne libre d'une ligne clouee a une alimentation ou a la masse. */
    ESP_LOGW(TAG, "--- etat au repos (pu = avec pull-up, pd = avec pull-down) ---");
    for (int i = 0; i < N; i++) {
        gpio_reset_pin(ln[i].gpio);
        gpio_set_direction(ln[i].gpio, GPIO_MODE_INPUT);
        gpio_set_pull_mode(ln[i].gpio, GPIO_PULLUP_ONLY);
        vTaskDelay(pdMS_TO_TICKS(2));
        int pu = gpio_get_level(ln[i].gpio);
        gpio_set_pull_mode(ln[i].gpio, GPIO_PULLDOWN_ONLY);
        vTaskDelay(pdMS_TO_TICKS(2));
        int pd = gpio_get_level(ln[i].gpio);
        /* L'IRQ du nRF24 est active BASSE et repose HAUTE : une puce presente
         * et alimentee la tient elle-meme, et un pull-down interne (~45 kOhm)
         * ne la fait pas descendre. Lire pu=1 pd=1 sur cette ligne est donc le
         * signe d'une radio VIVANTE, pas d'un defaut — c'est meme le seul
         * temoin de presence que donne le test de lignes. Le libeller comme un
         * clou ferait paniquer pour rien. Les cinq autres lignes, elles, sont
         * pilotees par le MCU et doivent bien etre libres au repos. */
        const bool est_irq = (ln[i].gpio == BOARD_NRF_IRQ);
        const char *verdict = (pu == 1 && pd == 0) ? "libre"
                            : (pu == 0 && pd == 0) ? "CLOUEE A LA MASSE"
                            : (pu == 1 && pd == 1) ? (est_irq ? "tenue haute (normal : IRQ au repos)"
                                                             : "CLOUEE AU 3V3")
                            : "incoherente";
        ESP_LOGW(TAG, "%-4s (GPIO%2d) : pu=%d pd=%d -> %s",
                 ln[i].name, ln[i].gpio, pu, pd, verdict);
    }

    for (int i = 0; i < N; i++) gpio_reset_pin(ln[i].gpio);
    ESP_LOGW(TAG, "--- fin test de lignes (%d pont(s)) ---", shorts);
}

void rf_probe_run(void)
{
    rf_radio_cfg_t cfg = {
        .spi_host         = BOARD_NRF_SPI_HOST,
        .pin_mosi         = BOARD_NRF_MOSI,
        .pin_miso         = BOARD_NRF_MISO,
        .pin_sck          = BOARD_NRF_SCK,
        .clock_hz         = 8 * 1000 * 1000,
        .pin_csn          = BOARD_NRF_CSN,
        .pin_ce           = BOARD_NRF_CE,
        .pin_irq          = BOARD_NRF_IRQ,
        .channel          = RF_PROBE_CHANNEL,
        .rx_addr          = { 'K', 'a', 'S', 'e' },
        .addr_suffix      = RF_PROBE_ADDR_SUFFIX,
        .shares_bus_first = true,
    };

    ESP_LOGW(TAG, "=== probe nRF24 : sck=%d miso=%d mosi=%d csn=%d ce=%d irq=%d ===",
             cfg.pin_sck, cfg.pin_miso, cfg.pin_mosi,
             cfg.pin_csn, cfg.pin_ce, cfg.pin_irq);

    rf_probe_lines();

    static rf_radio_t radio;
    esp_err_t e = rf_driver_init(&radio, &cfg);

    if (e == ESP_OK && radio.present)
        ESP_LOGW(TAG, "=== LA RADIO REPOND (init PRX OK) ===");
    else
        ESP_LOGW(TAG, "=== PAS DE REPONSE (err=%s) — voir la ligne 'probe csn=' "
                      "ci-dessus pour les valeurs lues ===", esp_err_to_name(e));
}
