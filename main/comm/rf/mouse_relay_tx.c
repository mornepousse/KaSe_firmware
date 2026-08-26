/* Voir mouse_relay_tx.h. Modelé sur comm/rf/kbd_relay_tx.c pour la séquence
 * d'appairage, mais sans rien de ce qui appartient au clavier. */

#include "mouse_relay_tx.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "board.h"
#include "rf_driver.h"
#include "rf_pairing.h"
#include "rf_packet.h"

static const char *TAG = "mouse_rf";

static rf_radio_t s_radio;
static bool s_paired;
static uint32_t s_tx, s_tx_ack;

/* Configuration radio de la Conchodytes.
 *
 * ⚠ `shares_bus_first = false` — et ce n'est pas un détail. Le bus SPI est
 * PARTAGÉ avec le PMW3389, et c'est `pmw3389_init()` qui appelle
 * `spi_bus_initialize`. Mettre `true` ici ferait un second appel sur un bus
 * déjà initialisé. */
static rf_radio_cfg_t mouse_nrf_cfg(void)
{
    rf_radio_cfg_t c = {
        .spi_host         = BOARD_NRF_SPI_HOST,
        .pin_mosi         = BOARD_NRF_MOSI,
        .pin_miso         = BOARD_NRF_MISO,
        .pin_sck          = BOARD_NRF_SCK,
        .clock_hz         = BOARD_NRF_CLOCK_HZ,
        .pin_csn          = BOARD_NRF_CSN,
        .pin_ce           = BOARD_NRF_CE,
        .pin_irq          = BOARD_NRF_IRQ,
        .channel          = BOARD_NRF_CHANNEL,
        .rx_addr          = { 'K', 'a', 'S', 'e' },   /* base 4 octets */
        .addr_suffix      = BOARD_NRF_ADDR_SUFFIX,    /* 0x02 = slot souris */
        .shares_bus_first = false,                    /* le capteur l'a déjà fait */
    };
    return c;
}

esp_err_t mouse_relay_init(void)
{
    rf_radio_cfg_t cfg = mouse_nrf_cfg();

    /* Restaure l'appairage AVANT d'initialiser la radio : le set_id détermine
     * l'adresse et le canal de travail, qui ne sont pas ceux du rendez-vous. */
    uint8_t slot = BOARD_NRF_ADDR_SUFFIX;
    uint16_t set_id = rf_pairing_load_set_id_half(BOARD_NRF_ADDR_SUFFIX, &slot);
    if (set_id) {
        rf_apply_set_id(&cfg, set_id, slot);
        ESP_LOGI(TAG, "appairage restaure : set_id=0x%04X slot=0x%02X", set_id, slot);
    } else {
        ESP_LOGW(TAG, "non appairee — les rapports seront jetes. Ouvrir la "
                      "fenetre du dongle (KS_CMD_RF_PAIR_START) puis appairer.");
    }

    esp_err_t err = rf_driver_init_tx(&s_radio, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rf_driver_init_tx : %s", esp_err_to_name(err));
        return err;
    }
    if (!s_radio.present) {
        /* Le probe lit CONFIG/RF_SETUP et refuse le tout-à-zéro comme le
         * tout-à-un. Sur cette carte, les 0xFF ont voulu dire « module non
         * alimente » pendant des heures — une soudure froide sur son 3,3 V. */
        ESP_LOGE(TAG, "la radio ne repond pas : registres incoherents. "
                      "Verifier l'alimentation du module U8.");
        return ESP_ERR_NOT_FOUND;
    }

    /* ⚠ ZÉRO RETRANSMISSION — surtout PAS le 0x1F du pilote (ARD=500 µs, ARC=15,
     * ~13 ms de pire cas), taillé pour le CLAVIER dont l'état est ABSOLU.
     *
     * Ici les rapports partent à 1 kHz et portent un déplacement RELATIF, donc
     * NON IDEMPOTENT. Les deux termes du compromis sont dissymétriques :
     *   - une trame PERDUE coûte 1 ms de geste, que la suivante rend invisible ;
     *   - une trame DUPLIQUÉE applique deux fois le même déplacement, et ça se
     *     voit — le curseur saute.
     * Or c'est précisément ce que produit une reprise dont seul l'ACK s'était
     * perdu : le dongle reçoit deux fois la même trame. Mesuré au banc le
     * 2026-08-26 avec ARC=1 : 903 trames émises pour 1018 acceptées côté dongle.
     *
     * En prime, `rf_driver_send()` ne bloque plus que le temps d'une tentative,
     * ce qui tient dans la période de rapport de 1 ms. */
    rf_driver_set_retr(&s_radio, 0x00);

    s_paired = (set_id != 0);
    ESP_LOGI(TAG, "radio prete, canal 0x%02X, %s",
             cfg.channel, s_paired ? "appairee" : "NON appairee");

    return ESP_OK;
}

bool mouse_relay_active(void)
{
    return s_paired && s_radio.present;
}

bool mouse_relay_send(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel)
{
    if (!mouse_relay_active()) return false;

    uint8_t buf[6];
    uint16_t n = rf_encode_hidreport_mouse(buf, buttons, dx, dy, wheel);
    /* Jamais de réémission : le déplacement est RELATIF, donc non idempotent.
     * Rejouer un rapport ferait avancer le curseur une seconde fois. C'est la
     * différence de fond avec le relais clavier, dont l'état est absolu et se
     * rafraîchit sans risque. Une trame perdue coûte quelques comptes de
     * mouvement ; une trame rejouée fait sauter le curseur. */
    /* Sérialise l'échange complet vis-à-vis du capteur, qui partage ce bus :
     * `rf_driver_send()` écrit la charge utile, pulse CE, puis scrute STATUS —
     * plusieurs transactions séparées par des attentes.
     *
     * ⚠ Ceci ne suffit PAS à faire cohabiter deux appareils de MODES SPI
     * différents, et ne réglait rien du lien mort mesuré le 2026-08-26 : la
     * cause était la bascule de mode sous un CSN déjà abaissé, corrigée dans
     * `rf_driver.c` (voir `spi_parquer_mode`). C'est gardé parce que sérialiser
     * reste juste, pas parce que ça a guéri quoi que ce soit. */
    spi_device_acquire_bus(s_radio.spi, portMAX_DELAY);
    s_tx++;
    bool ok = rf_driver_send(&s_radio, buf, (uint8_t)n);
    if (ok) s_tx_ack++;
    spi_device_release_bus(s_radio.spi);
    return ok;
}

void mouse_relay_stats(uint32_t *envoyes, uint32_t *acquittes)
{
    if (envoyes)   *envoyes   = s_tx;
    if (acquittes) *acquittes = s_tx_ack;
}

esp_err_t mouse_relay_pair(void)
{
    if (!s_radio.present) return ESP_ERR_INVALID_STATE;

    uint8_t my_mac[6];
    esp_read_mac(my_mac, ESP_MAC_WIFI_STA);

    /* Requête v2 : elle porte le type d'appareil en plus du slot. Le dongle
     * honore le slot déclaré (rf_pairing_resolve_slot), donc la souris obtient
     * le slot 2 quel que soit l'ordre d'appairage. */
    uint8_t req[9];
    uint16_t reqlen = rf_encode_pair_req2(req, my_mac, BOARD_NRF_ADDR_SUFFIX,
                                          RF_DEV_MOUSE);

    static const uint8_t pair_addr[5] = RF_PAIR_ADDR;
    rf_radio_cfg_t cfg = mouse_nrf_cfg();

    ESP_LOGI(TAG, "appairage : emission sur le rendez-vous canal 0x%02X",
             RF_PAIR_CHANNEL);

    for (int essai = 1; essai <= 20; essai++) {
        rf_driver_set_channel(&s_radio, RF_PAIR_CHANNEL);
        rf_driver_set_tx_address(&s_radio, pair_addr);
        bool ack = rf_driver_send(&s_radio, req, (uint8_t)reqlen);

        uint8_t rep[16];
        uint16_t n = rf_driver_pair_listen(&s_radio, RF_PAIR_CHANNEL, pair_addr,
                                           rep, sizeof(rep), 300);
        if (n) {
            rf_pair_ack_t a;
            if (rf_decode_pair_ack(rep, n, &a)) {
                ESP_LOGI(TAG, "ACK recu : set_id=0x%04X slot=0x%02X dongle=%02X:%02X:%02X:%02X:%02X:%02X",
                         a.set_id, a.slot, a.dongle_wifi_mac[0], a.dongle_wifi_mac[1],
                         a.dongle_wifi_mac[2], a.dongle_wifi_mac[3],
                         a.dongle_wifi_mac[4], a.dongle_wifi_mac[5]);
                esp_err_t e = rf_pairing_save_half(a.set_id, a.slot, a.dongle_wifi_mac);
                if (e != ESP_OK) {
                    ESP_LOGE(TAG, "sauvegarde NVS : %s", esp_err_to_name(e));
                    return e;
                }
                ESP_LOGW(TAG, "appairee. Redemarrer pour que le lien s'active.");
                return ESP_OK;
            }
            ESP_LOGW(TAG, "reponse de %u octets, mais ce n'est pas un PAIR_ACK", n);
        }

        /* `ack` vaut le TX_DS du nRF : il dit que QUELQU'UN a acquitté la
         * trame au niveau ESB, pas que le dongle l'a comprise. Le distinguer
         * aide au diagnostic — sans ACK du tout, c'est la portée, le canal ou
         * l'adresse ; avec ACK mais sans réponse, c'est la fenêtre du dongle
         * qui est fermée. */
        ESP_LOGI(TAG, "essai %d/20 : %s, pas de PAIR_ACK",
                 essai, ack ? "trame acquittee au niveau radio" : "aucun acquittement");
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    /* Remet la radio sur son adresse de travail avant de rendre la main. */
    rf_driver_set_channel(&s_radio, cfg.channel);
    ESP_LOGE(TAG, "appairage echoue apres 20 essais. La fenetre du dongle "
                  "est-elle ouverte (KS_CMD_RF_PAIR_START) ?");
    return ESP_ERR_TIMEOUT;
}
