/* Voir pmw3389.h. Porté du bring-up validé sur carte le 2026-08-25
 * (~/Documents/GitHub/Conchodytes/bringup/), lui-même dérivé du POC
 * mornepousse/Mase et de mrjohnk/PMW3389DM — avec trois corrections que ni
 * l'un ni l'autre ne portent : temporisations conformes au 3389, ordre de
 * démarrage, et le bon blob SROM. */

#include "pmw3389.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#include "board.h"

static const char *TAG = "pmw3389";

/* ── Registres, Table du §5.1, p. 20 ────────────────────────────────────── */
#define REG_PRODUCT_ID          0x00
#define REG_REVISION_ID         0x01
#define REG_MOTION              0x02
#define REG_DELTA_X_L           0x03
#define REG_DELTA_X_H           0x04
#define REG_DELTA_Y_L           0x05
#define REG_DELTA_Y_H           0x06
#define REG_SQUAL               0x07
#define REG_SHUTTER_LOWER       0x0B
#define REG_SHUTTER_UPPER       0x0C
#define REG_RESOLUTION_L        0x0E   /* 16 bits sur le 3389, pas un Config1 */
#define REG_RESOLUTION_H        0x0F
#define REG_CONFIG2             0x10
#define REG_SROM_ENABLE         0x13
#define REG_SROM_ID             0x2A
#define REG_POWER_UP_RESET      0x3A
#define REG_INVERSE_PRODUCT_ID  0x3F
#define REG_MOTION_BURST        0x50
#define REG_SROM_LOAD_BURST     0x62

/* Disposition du Motion_Burst — ÉTABLIE PAR LA MESURE le 2026-08-25, faute de
 * l'avoir dans une datasheet. Ni les 20 pages du 3389 ni celles du 3360 ne la
 * décrivent ; elles ne donnent que ses temps (tSRAD_MOTBR 35 µs, tBEXIT 500 ns,
 * Table 5, p. 16).
 *
 * Méthode : afficher les octets du burst à côté des mêmes registres lus un par
 * un, puis balayer sur un seul axe à la fois, souris soulevée au retour pour
 * que le signe ne s'annule pas.
 *
 * Preuves retenues :
 *   [0]  vaut toujours la référence + 0x80 — le bit MOT, armé par le burst et
 *        effacé par la lecture suivante. Vu trois fois.
 *   [1]  constant à 0x7F.
 *   [2,3] biais de 100,0 % sur 76 échantillons d'un balayage horizontal.
 *   [4,5] biais de 99,8 % sur 71 échantillons d'un balayage vertical.
 *   [6]  même plage que SQUAL lu séparément.
 *   [8]  culmine à 0x7F — un pixel vaut au plus 127.
 *   [10] nul, comme Shutter_Upper ; [11] même plage que Shutter_Lower.
 *   [12..15] toujours nuls : le burst fait 12 octets, pas plus. */
#define BURST_LEN            12
#define BURST_MOTION          0
#define BURST_OBSERVATION     1
#define BURST_DELTA_X_L       2
#define BURST_DELTA_X_H       3
#define BURST_DELTA_Y_L       4
#define BURST_DELTA_Y_H       5
#define BURST_SQUAL           6
#define BURST_RAWDATA_SUM     7
#define BURST_MAX_RAWDATA     8
#define BURST_MIN_RAWDATA     9
#define BURST_SHUTTER_UPPER  10
#define BURST_SHUTTER_LOWER  11

/* ── Temporisations, Table 5, p. 16 ─────────────────────────────────────────
 * Les valeurs du 3389, PAS celles du 3360. Marge volontaire au-dessus du
 * minimum : ces attentes ne sont pas dans le chemin critique, et l'expérience
 * du banc est qu'un tSRAD trop court passe puis lâche par intermittence. */
#define T_SRAD_US   180   /* min 160 */
#define T_SRW_US     20   /* min 20  */
#define T_SWW_US    200   /* min 180 */
#define T_SROM_US    15   /* entre octets du burst SROM */

extern const unsigned short pmw3389_firmware_length;
extern const unsigned char pmw3389_firmware_data[];

static spi_device_handle_t s_dev;
static bool s_ready;

/* ── Transport ──────────────────────────────────────────────────────────────
 * CS piloté à la main, et ce n'est pas un choix de style : `tSRAD` tombe ENTRE
 * l'octet d'adresse et l'octet de donnée. Le CS matériel d'ESP-IDF le
 * relèverait au milieu de l'attente, ce qui avorte la lecture. D'où
 * `spics_io_num = -1` — même geste que comm/rf/rf_driver.c pour le nRF24. */

static inline void cs_low(void)
{
    gpio_set_level(BOARD_SNS_NCS_GPIO, 0);
    esp_rom_delay_us(1);
}

static inline void cs_high(void)
{
    esp_rom_delay_us(1);
    gpio_set_level(BOARD_SNS_NCS_GPIO, 1);
}

static esp_err_t xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    return spi_device_polling_transmit(s_dev, &t);
}

static uint8_t reg_read(uint8_t addr)
{
    uint8_t a = addr & 0x7F;      /* MSB à 0 = lecture */
    uint8_t d = 0;

    cs_low();
    xfer(&a, NULL, 1);
    esp_rom_delay_us(T_SRAD_US);
    xfer(NULL, &d, 1);
    cs_high();
    esp_rom_delay_us(T_SRW_US);
    return d;
}

static void reg_write(uint8_t addr, uint8_t val)
{
    uint8_t buf[2] = { (uint8_t)(addr | 0x80), val };   /* MSB à 1 = écriture */

    cs_low();
    xfer(buf, NULL, 2);
    esp_rom_delay_us(T_SRW_US);
    cs_high();
    esp_rom_delay_us(T_SWW_US);
}

/* ── Téléversement du SROM ──────────────────────────────────────────────────
 * 4094 octets à 15 µs l'octet : ~61 ms pendant lesquelles RIEN d'autre ne doit
 * toucher le bus. `spi_device_acquire_bus` le garantit face au nRF24 — une
 * trame radio au milieu corromprait le SROM, et le capteur démarrerait sur un
 * firmware invalide sans forcément le dire. */
static esp_err_t srom_upload(void)
{
    reg_write(REG_CONFIG2, 0x20);        /* valeur par défaut du registre, p. 20 */
    reg_write(REG_SROM_ENABLE, 0x1D);
    vTaskDelay(pdMS_TO_TICKS(10));       /* plus d'une période de trame */
    reg_write(REG_SROM_ENABLE, 0x18);

    uint8_t burst = REG_SROM_LOAD_BURST | 0x80;
    cs_low();
    xfer(&burst, NULL, 1);
    esp_rom_delay_us(T_SROM_US);
    for (unsigned i = 0; i < pmw3389_firmware_length; i++) {
        xfer(&pmw3389_firmware_data[i], NULL, 1);
        esp_rom_delay_us(T_SROM_US);
    }
    cs_high();
    esp_rom_delay_us(200);

    uint8_t srom_id = reg_read(REG_SROM_ID);
    ESP_LOGI(TAG, "SROM_ID = 0x%02X", srom_id);
    if (srom_id == 0x00) {
        ESP_LOGE(TAG, "televersement SROM rate : le capteur ne suivra rien");
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* 0x00 = Rest désactivé. La gestion d'énergie viendra avec la radio ; en
     * attendant, un capteur qui s'endort seul fausse toute mesure de banc. */
    reg_write(REG_CONFIG2, 0x00);
    return ESP_OK;
}

/* Remise à zéro du port série PUIS reset logiciel — dans cet ordre, et avant
 * la première lecture de registre. Voir le commentaire de pmw3389_init dans
 * l'en-tête : sans ça la première lecture sort décalée de deux bits. */
static void sensor_reset(void)
{
    cs_high(); cs_low(); cs_high();
    reg_write(REG_POWER_UP_RESET, 0x5A);
    vTaskDelay(pdMS_TO_TICKS(50));       /* tMOT-RST = 50 ms, Table 5, p. 16 */

    (void)reg_read(REG_MOTION);
    (void)reg_read(REG_DELTA_X_L);
    (void)reg_read(REG_DELTA_X_H);
    (void)reg_read(REG_DELTA_Y_L);
    (void)reg_read(REG_DELTA_Y_H);
}

esp_err_t pmw3389_probe(uint8_t *id, uint8_t *inverse, uint8_t *revision)
{
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    uint8_t i = reg_read(REG_PRODUCT_ID);
    uint8_t v = reg_read(REG_INVERSE_PRODUCT_ID);
    uint8_t r = reg_read(REG_REVISION_ID);
    if (id) *id = i;
    if (inverse) *inverse = v;
    if (revision) *revision = r;
    return ESP_OK;
}

/* Règle la résolution en cpi.
 *
 * ⚠ LA FORMULE N'EST PAS DANS LA DATASHEET dont on dispose. La version en
 * bibliothèque (PMW3389DM-T3QU v1.0, 07 sep 2017, 20 pages) donne le tableau
 * récapitulatif p. 20 — `0x0E Resolution_L` RW défaut 0x00, `0x0F Resolution_H`
 * RW défaut 0x42 — et « jusqu'à 16000 cpi » p. 1, mais AUCUNE description bit à
 * bit des deux registres. L'encodage retenu ici, pas de 50 cpi sur 16 bits en
 * petit-boutien, est celui des pilotes PMW3389 du domaine public. Il est donc à
 * VALIDER À L'USAGE, et `pmw3389_init()` journalise la valeur relue de la puce
 * pour qu'on puisse le confronter au réel plutôt qu'y croire.
 *
 * 16000 cpi max ⇒ 320 pas, qui tiennent sur 9 bits : d'où le couple L/H. */
void pmw3389_set_cpi(uint16_t cpi)
{
    if (cpi < 50)    cpi = 50;
    if (cpi > 16000) cpi = 16000;
    uint16_t pas = (uint16_t)(cpi / 50u);
    reg_write(REG_RESOLUTION_L, (uint8_t)(pas & 0xFF));
    reg_write(REG_RESOLUTION_H, (uint8_t)(pas >> 8));
}

esp_err_t pmw3389_init(void)
{
    /* Lignes de sélection AVANT tout le reste. Le nRF24 partage le bus : son
     * CSN doit être haut et son CE bas avant qu'un seul octet circule, sinon
     * deux esclaves tirent MISO en même temps. */
    gpio_config_t sel = {
        .pin_bit_mask = (1ULL << BOARD_SNS_NCS_GPIO) |
                        (1ULL << BOARD_NRF_CSN) | (1ULL << BOARD_NRF_CE),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&sel));
    gpio_set_level(BOARD_SNS_NCS_GPIO, 1);
    gpio_set_level(BOARD_NRF_CSN, 1);
    gpio_set_level(BOARD_NRF_CE, 0);

    gpio_config_t motion = {
        .pin_bit_mask = 1ULL << BOARD_SNS_MOTION_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&motion));

    /* Le bus est partagé. Quand le relais radio sera compilé pour ce rôle, il
     * pourra l'avoir initialisé avant nous : ESP_ERR_INVALID_STATE veut alors
     * dire « déjà fait », et c'est bien. Toute autre erreur est réelle. */
    spi_bus_config_t bus = {
        .sclk_io_num     = BOARD_NRF_SCK,
        .mosi_io_num     = BOARD_NRF_MOSI,
        .miso_io_num     = BOARD_NRF_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 64,
    };
    esp_err_t err = spi_bus_initialize(BOARD_NRF_SPI_HOST, &bus, SPI_DMA_DISABLED);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    spi_device_interface_config_t dev = {
        .clock_speed_hz = BOARD_SNS_CLOCK_HZ,
        .mode           = BOARD_SNS_SPI_MODE,   /* 3 — le nRF24 est en 0 */
        .spics_io_num   = -1,                   /* CS manuel, cf. plus haut */
        .queue_size     = 1,
    };
    err = spi_bus_add_device(BOARD_NRF_SPI_HOST, &dev, &s_dev);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(50));
    sensor_reset();

    uint8_t id = 0, inv = 0, rev = 0;
    pmw3389_probe(&id, &inv, &rev);
    ESP_LOGI(TAG, "Product_ID=0x%02X Inverse=0x%02X Revision=0x%02X", id, inv, rev);

    if ((uint8_t)(id ^ inv) != 0xFF) {
        /* Le complément ne tient pas : ce n'est pas une puce inattendue, c'est
         * le bus qui ment. Ligne coupée, ligne collée, mauvais mode SPI, ou le
         * nRF24 qui répond à la place du capteur. */
        ESP_LOGE(TAG, "0x%02X ^ 0x%02X != 0xFF : le bus ment, pas la puce", id, inv);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (id != PMW3389_PRODUCT_ID) {
        ESP_LOGE(TAG, "0x%02X n'est pas un PMW3389 (attendu 0x%02X)",
                 id, PMW3389_PRODUCT_ID);
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_ERROR_CHECK(spi_device_acquire_bus(s_dev, portMAX_DELAY));
    err = srom_upload();
    spi_device_release_bus(s_dev);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(10));
    s_ready = true;

    /* Le firmware ne réglait PAS la résolution : la puce restait sur ce que le
     * reset et le SROM lui laissent, d'où un curseur beaucoup trop rapide.
     * On relève la valeur en place AVANT de la fixer — le tableau des registres
     * de la datasheet donne les défauts de reset, pas ce que le SROM laisse. */
    uint8_t rl = reg_read(REG_RESOLUTION_L), rh = reg_read(REG_RESOLUTION_H);
    ESP_LOGI(TAG, "resolution trouvee : L=0x%02X H=0x%02X", rl, rh);

    pmw3389_set_cpi(BOARD_SNS_CPI);
    rl = reg_read(REG_RESOLUTION_L); rh = reg_read(REG_RESOLUTION_H);
    ESP_LOGI(TAG, "resolution reglee a %d cpi : L=0x%02X H=0x%02X",
             BOARD_SNS_CPI, rl, rh);
    ESP_LOGI(TAG, "montage : BOARD_SNS_ROT_180=%d%s", BOARD_SNS_ROT_180,
             BOARD_SNS_ROT_180 ? " (dx et dy nies)" : " (axes bruts)");
    return ESP_OK;
}

esp_err_t pmw3389_read_motion(pmw3389_motion_t *out)
{
    if (!s_ready || !out) return ESP_ERR_INVALID_STATE;

    /* UNE transaction au lieu de neuf.
     *
     * L'ancienne version faisait une écriture puis huit lectures de registres :
     * 230 µs + 8 × 210 µs ≈ 1,9 ms, dont l'essentiel en tSRAD (160 µs par
     * lecture). Le burst n'en paie qu'un seul, et à 35 µs : ~90 µs au total,
     * soit un facteur 21. Sur une souris ça n'est pas un confort — à 1,9 ms le
     * relevé bloquait le scrutin des clics un quart du temps, et plafonnait la
     * cadence de rapport à 500 Hz.
     *
     * Il rend en prime un instantané COHÉRENT. Les lectures séparées
     * échantillonnaient des trames différentes : SQUAL relevé ainsi tombait à
     * 15-40 quand le même registre lu seul valait 80, ce qui avait un moment
     * fait soupçonner l'optique à tort. */
    uint8_t b[BURST_LEN];

    /* ⚠ LE BUS EST PARTAGÉ AVEC LE nRF24, ET LES DEUX PILOTENT LEUR CS À LA
     * MAIN (`spics_io_num = -1`). ESP-IDF ne sait donc pas où commence ni finit
     * une transaction logique : entre l'octet d'adresse et les données, ce
     * driver garde CS bas pendant 35 µs sans rien émettre, et le pilote se croit
     * libre. On sérialise donc explicitement.
     *
     * ⚠ Ceci ne règle PAS la cohabitation de deux MODES SPI différents (3 ici,
     * 0 pour la radio) : la bascule de mode a lieu au démarrage de la
     * transaction, donc après que l'appelant a baissé son CS. C'est ce qui a
     * tué le lien radio le 2026-08-26, et c'est corrigé côté radio par
     * `spi_parquer_mode()` dans comm/rf/rf_driver.c — pas ici. */
    spi_device_acquire_bus(s_dev, portMAX_DELAY);

    reg_write(REG_MOTION_BURST, 0x00);      /* arme le mode burst */

    uint8_t addr = REG_MOTION_BURST;        /* MSB à 0 : lecture */
    cs_low();
    xfer(&addr, NULL, 1);
    esp_rom_delay_us(35);                   /* tSRAD_MOTBR, Table 5, p. 16 */
    xfer(NULL, b, BURST_LEN);               /* les 12 octets d'affilée */
    cs_high();
    esp_rom_delay_us(5);                    /* tBEXIT = 500 ns, large */

    /* ⚠ LE BIT MOT COMMANDE : sans mouvement depuis la dernière lecture, les
     * registres Delta ne portent PAS un déplacement nul, ils portent n'importe
     * quoi. Les lire sans regarder ce bit revient à injecter du bruit dans le
     * curseur — c'est ce qui faisait « bouger la souris toute seule » au banc le
     * 2026-08-26. L'octet était déjà rapatrié par le burst (offset 0), il n'était
     * simplement pas consulté. */
    out->motion  = (b[BURST_MOTION] & 0x80) != 0;
    if (out->motion) {
        out->dx = (int16_t)(((uint16_t)b[BURST_DELTA_X_H] << 8) | b[BURST_DELTA_X_L]);
        out->dy = (int16_t)(((uint16_t)b[BURST_DELTA_Y_H] << 8) | b[BURST_DELTA_Y_L]);
#if BOARD_SNS_ROT_180
        /* Capteur monté à 180° : les deux axes sont retournés, on les remet
         * d'aplomb ici. Voir BOARD_SNS_ROT_180 dans board.h — la constante passe
         * à 0 le jour où le layout tourne l'empreinte. */
        out->dx = (int16_t)(-out->dx);
        out->dy = (int16_t)(-out->dy);
#endif
    } else {
        out->dx = 0;
        out->dy = 0;
    }
    out->squal   = b[BURST_SQUAL];
    spi_device_release_bus(s_dev);

    out->shutter = (uint16_t)(((uint16_t)b[BURST_SHUTTER_UPPER] << 8) |
                               b[BURST_SHUTTER_LOWER]);

    return ESP_OK;
}

bool pmw3389_motion_pending(void)
{
    return gpio_get_level(BOARD_SNS_MOTION_GPIO) == 0;   /* actif bas */
}
