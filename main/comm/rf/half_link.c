#include "half_link.h"
#include "board.h"
#include "rf_driver.h"
#include "rf_packet.h"
#include "rf_slot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "half_link";

static rf_radio_t s_radio;
static half_state_t s_distant;   /* etat de la moitie d en face */
static volatile bool s_distant_change;   /* consomme par half_link_remote_changed */
static uint8_t    s_seq;

/* Config commune aux deux bouts : même canal, même adresse, sinon rien ne
 * passe (nRF24L01+ PS §6.3 : « You must program a transmitter and a receiver
 * with the same RF channel frequency to communicate with each other »). */
static rf_radio_cfg_t half_link_cfg(void)
{
    rf_radio_cfg_t c = {
        .spi_host         = BOARD_NRF_SPI_HOST,
        .pin_mosi         = BOARD_NRF_MOSI,
        .pin_miso         = BOARD_NRF_MISO,
        .pin_sck          = BOARD_NRF_SCK,
        .clock_hz         = 8 * 1000 * 1000,
        .pin_csn          = BOARD_NRF_CSN,
        .pin_ce           = BOARD_NRF_CE,
        .pin_irq          = BOARD_NRF_IRQ,
        .channel          = RF_CH_HALF_LINK,
        .rx_addr          = { 'K', 'a', 'S', 'e' },
        .addr_suffix      = RF_ADDR_HALF_LINK,
        .shares_bus_first = true,
    };
    return c;
}

#if CONFIG_KASE_HAS_RF_TX
bool half_link_tx_init(void)
{
    rf_radio_cfg_t cfg = half_link_cfg();
    esp_err_t e = rf_driver_init_tx(&s_radio, &cfg);
    if (e != ESP_OK || !s_radio.present) {
        ESP_LOGE(TAG, "TX init echouee (%d) — la moitie droite restera muette", (int)e);
        return false;
    }
    ESP_LOGI(TAG, "TX pret : ch=0x%02X addr=KaSe.%02X", cfg.channel, cfg.addr_suffix);

    /* Rafale d'epreuve au demarrage. Sans elle, savoir si le lien porte
     * dependrait de quelqu'un appuyant sur une touche PENDANT qu'on ecoute la
     * console — synchronisation peu commode entre deux operateurs. Ici un
     * simple reset suffit a obtenir le verdict.
     *
     * L'acquittement est MATERIEL : le nRF24 d'en face repond de lui-meme si
     * canal et adresse concordent, sans que son logiciel intervienne. Un taux
     * eleve prouve donc que la radio de la gauche ecoute sur le bon canal,
     * meme si sa couche applicative avait un probleme par ailleurs. */
    {
        const int N = 10;
        uint8_t bm[RF_HALF_BITMAP_BYTES];
        memset(bm, 0, sizeof(bm));
        int ok = 0;
        for (int i = 0; i < N; i++) {
            if (half_link_tx_matrix(bm)) ok++;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (ok == 0) {
            ESP_LOGE(TAG, "epreuve : 0/%d acquittes — PERSONNE N'ECOUTE sur ch=0x%02X",
                     N, cfg.channel);
        } else {
            ESP_LOGW(TAG, "epreuve : %d/%d acquittes — LA GAUCHE ECOUTE", ok, N);
            /* LA mesure de R1. L'acquittement seul ne dit rien de la surdite :
             * l'ESB retransmet jusqu'a 15 fois, donc un paquet passe meme si la
             * gauche etait sourde au premier essai. Ce qui trahit la surdite,
             * c'est le NOMBRE DE RETRANSMISSIONS — chaque excursion PRX->PTX de
             * la gauche coute un essai perdu a la droite.
             *
             * Un ratio proche de zero veut dire que la bascule ne se voit pas ;
             * un ratio eleve mesure exactement ce que le pari coute. */
            if (rf_tx_count)
                ESP_LOGW(TAG, "R1 : %u retransmissions pour %u paquets = %u.%02u par paquet",
                         (unsigned)rf_tx_retr_sum, (unsigned)rf_tx_count,
                         (unsigned)(rf_tx_retr_sum / rf_tx_count),
                         (unsigned)((rf_tx_retr_sum * 100 / rf_tx_count) % 100));
        }
    }
    return true;
}

bool half_link_tx_matrix(const uint8_t *bitmap)
{
    if (!s_radio.present) return false;
    rf_heartbeat_t h;
    memset(&h, 0, sizeof(h));
    memcpy(h.bitmap, bitmap, RF_HALF_BITMAP_BYTES);
    h.seq = s_seq++;
    /* batt_dV et link_q restent a zero : la jauge est la brick B7, et la
     * qualite de lien se calculera quand le compteur de retransmissions aura
     * un sens (il faut un recepteur en face). */
    uint8_t buf[16];
    uint16_t n = rf_encode_heartbeat(buf, &h);
    bool ack = rf_driver_send(&s_radio, buf, (uint8_t)n);

    /* Instrument de banc : sans lui, on ne distingue pas « les paquets partent
     * et sont acquittes » de « ils partent dans le vide ». Resume tous les dix
     * envois plutot qu'une ligne par paquet — a la frappe, une ligne par paquet
     * noierait la console et fausserait le timing. */
    static uint32_t envois, acquittes;
    envois++;
    if (ack) acquittes++;
    if ((envois % 10) == 0)
        ESP_LOGW(TAG, "TX %u envois, %u acquittes (%u%%)",
                 (unsigned)envois, (unsigned)acquittes,
                 (unsigned)(acquittes * 100 / envois));
    return ack;
}
#endif /* CONFIG_KASE_HAS_RF_TX */

#if CONFIG_KASE_HALF_LINK_RX
static void half_link_rx_task(void *arg)
{
    (void)arg;
    uint8_t buf[32];
    uint32_t recus = 0, rejetes = 0, perdus = 0, excursions = 0;
    bool seq_amorce = false;
    uint8_t seq_attendu = 0;
#if CONFIG_KASE_HALF_LINK_R1
    /* Adresse du dongle, slot clavier — cible des excursions. */
    const uint8_t addr_dongle[5] = { 'K', 'a', 'S', 'e', 0x01 };
    const uint8_t addr_lien[5]   = { 'K', 'a', 'S', 'e', RF_ADDR_HALF_LINK };
    uint32_t derniere_excursion = 0;
#endif
    for (;;) {
#if CONFIG_KASE_HALF_LINK_R1
        /* ÉPREUVE R1. La gauche est sourde pendant qu'elle émet : on provoque
         * l'excursion à cadence fixe et on mesure ce qu'elle coûte en trous de
         * séquence. rf_driver_oob_tx fait le PRX→PTX→PRX complet, y compris la
         * restauration du canal d'écoute et le CE haut.
         *
         * 20 ms, soit 50 excursions/s : au-delà de ce qu'un clavier produit en
         * frappe rapide, donc un majorant honnête du coût. */
        uint32_t maintenant = (uint32_t)(esp_timer_get_time() / 1000);
        if (maintenant - derniere_excursion >= 20) {
            derniere_excursion = maintenant;
            uint8_t bidon[9] = { 0x50, 0, 0, 0, 0, 0, 0, 0, 0 };
            rf_driver_oob_tx(&s_radio, RF_CH_KBD_DONGLE, addr_dongle,
                             bidon, sizeof(bidon),
                             RF_CH_HALF_LINK, addr_lien);
            excursions++;
        }
#endif
        if (rf_driver_rx_available(&s_radio)) {
            uint16_t n = rf_driver_read_rx(&s_radio, buf, sizeof(buf));
            rf_heartbeat_t h;
            if (n && rf_decode_heartbeat(buf, n, &h)) {
                recus++;
                /* FUSION : l'etat recu devient celui de la moitie distante. Le
                 * moteur le lira via half_link_remote_pressed(). */
                {
                    uint8_t avant[RF_HALF_BITMAP_BYTES];
                    memcpy(avant, s_distant.bitmap, sizeof(avant));
                    half_state_recu(&s_distant, h.bitmap,
                                    (uint32_t)(esp_timer_get_time() / 1000));
                    /* Ne lever le drapeau que si l'ETAT a change : la droite
                     * emet sur changement, mais une retransmission ESB peut
                     * livrer deux fois la meme trame. */
                    if (memcmp(avant, s_distant.bitmap, sizeof(avant)))
                        s_distant_change = true;
                }
                /* Trous de séquence : le seul témoin de ce que l'excursion
                 * coûte. seq est un octet, l'écart se calcule donc modulo 256. */
                if (seq_amorce) {
                    uint8_t ecart = (uint8_t)(h.seq - seq_attendu);
                    if (ecart) perdus += ecart;
                }
                seq_amorce = true;
                seq_attendu = (uint8_t)(h.seq + 1);
                if ((recus % 20) == 0)
                    ESP_LOGW(TAG, "R1 : %u recus, %u perdus (%u%%), %u excursions",
                             (unsigned)recus, (unsigned)perdus,
                             (unsigned)(perdus * 100 / (recus + perdus)),
                             (unsigned)excursions);
                /* Journal de banc : on affiche les coordonnees pressees plutot
                 * que le bitmap brut, pour pouvoir comparer a ce qu'on presse
                 * physiquement sur la droite. */
                char pos[64]; int off = 0;
                for (int r = 0; r < RF_HALF_ROWS && off < (int)sizeof(pos) - 8; r++)
                    for (int c = 0; c < RF_HALF_COLS && off < (int)sizeof(pos) - 8; c++)
                        if (rf_bitmap_get(h.bitmap, (uint8_t)r, (uint8_t)c))
                            off += snprintf(pos + off, sizeof(pos) - off, "(%d,%d)", r, c);
                ESP_LOGW(TAG, "RX #%u seq=%u : %s", (unsigned)recus, h.seq,
                         off ? pos : "(rien enfonce)");
            } else {
                rejetes++;
                ESP_LOGW(TAG, "RX trame rejetee (len=%u, total %u)", n, (unsigned)rejetes);
            }
        }
        /* Repli sur silence. 250 ms : au-dela, une moitie qui s est tue laisse
         * l hote sur son dernier etat — et si c etait « Maj enfoncee », il le
         * reste. On ne relache QUE ce que cette moitie tenait. */
        if (half_state_timeout(&s_distant,
                               (uint32_t)(esp_timer_get_time() / 1000), 250))
        {
            ESP_LOGW(TAG, "lien silencieux > 250 ms — touches de la droite relachees");
            s_distant_change = true;
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

bool half_link_remote_pressed(uint8_t row, uint8_t col)
{
    return half_state_pressed(&s_distant, row, col);
}

bool half_link_remote_changed(void)
{
    if (!s_distant_change) return false;
    s_distant_change = false;
    return true;
}

bool half_link_rx_start(void)
{
    rf_radio_cfg_t cfg = half_link_cfg();
    esp_err_t e = rf_driver_init(&s_radio, &cfg);
    if (e != ESP_OK || !s_radio.present) {
        ESP_LOGE(TAG, "RX init echouee (%d) — la gauche n'entendra pas la droite", (int)e);
        return false;
    }
    ESP_LOGI(TAG, "RX a l'ecoute : ch=0x%02X addr=KaSe.%02X", cfg.channel, cfg.addr_suffix);
    xTaskCreatePinnedToCore(half_link_rx_task, "half_rx", 4096, NULL, 4, NULL, 1);
    return true;
}
#endif /* CONFIG_KASE_HALF_LINK_RX */
