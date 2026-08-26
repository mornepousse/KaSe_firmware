/* Tâche principale de la souris Conchodytes.
 *
 * Assemble les trois entrées de la carte : le capteur PMW3389, les trois clics
 * SPDT et l'encodeur de molette. La logique de décodage vit ailleurs et est
 * testée sur l'hôte (input/mouse_buttons.c, input/mouse_wheel.c) ; ce fichier
 * ne fait que lire des GPIO et cadencer.
 *
 * ⚠ ÉTAT : cette tâche ne produit encore AUCUN rapport HID. Le relais vers le
 * slot 2 du dongle est le jalon suivant et aura sa propre spec — voir
 * docs/superpowers/specs/2026-08-25-conchodytes-firmware-design.md §5 et §7,
 * ce dernier portant la question non tranchée de la saturation à ±127.
 * En attendant, elle journalise, ce qui suffit à valider le matériel.
 */

#include "mouse_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "board.h"
#include "periph/pmw3389.h"
#include "input/mouse_buttons.h"
#include "input/mouse_wheel.h"
#include "comm/rf/mouse_relay_tx.h"

static const char *TAG = "mouse";

/* ── Scrutin ────────────────────────────────────────────────────────────────
 * 1 kHz — ce qui EXIGE CONFIG_FREERTOS_HZ=1000, posé dans
 * sdkconfig.defaults.conchodytes. Avec le défaut ESP-IDF de 100 Hz,
 * vTaskDelay(1) vaudrait DIX millisecondes, de quoi manquer la moitié des
 * appuis brefs et toute la fenêtre de rebond. C'est exactement ce qui est
 * arrivé à la campagne de mesure du 2026-08-25.
 *
 * ⚠ Cette cadence convient aux CLICS. Elle ne convient PAS à la molette :
 * 60 fentes en quadrature font 240 transitions par tour, soit 2400/s sur un
 * coup sec, et il en faudrait ~5 kHz pour ne rien perdre. Voir wheel_note
 * plus bas. */
#define SCAN_PERIOD_TICKS  1

/* Le capteur est nettement plus lent à interroger que les GPIO : chaque
 * lecture de registre coûte 160 µs de tSRAD, et il y en a huit par relevé.
 * On l'interroge donc moins souvent que les boutons. */
/* ⚠ ÉTAIT À 8 — soit une lecture capteur toutes les 8 ms, 125 rapports/s.
 *
 * Deux défauts, tous deux ressentis comme de l'imprécision :
 *
 * 1. MOUVEMENT EN PAQUETS. Sept rapports vides puis un qui porte 8 ms de geste.
 *    Le curseur avance par à-coups au lieu de suivre.
 * 2. ÉCRÊTAGE. `dx` voyage sur un int8, ±127 par rapport. À 125 Hz et
 *    BOARD_SNS_CPI = 1000 cpi, le plafond est 127 × 125 = 15 875 comptes/s,
 *    soit ~40 cm/s. Au-delà l'excédent n'est pas perdu (il est reporté sur les
 *    trames suivantes) mais il arrive EN RETARD : le curseur traîne puis
 *    rattrape. Un geste vif dépasse le mètre par seconde.
 *
 * ⚠ ESSAYÉ À 1 (1 kHz), ET C'ÉTAIT PIRE. Le plafond d'écrêtage devenait
 * confortable, mais la CHAÎNE ne délivre qu'environ 130 paquets/s : émettre mille
 * fois par seconde pour en faire passer cent trente sature la liaison et fait
 * arriver le mouvement par bouffées irrégulières. Ressenti au banc le
 * 2026-08-26 : « saccadé, par à-coups ». Une cadence RÉGULIÈRE et entièrement
 * délivrée vaut mieux qu'une cadence élevée et perdue aux deux tiers.
 *
 * À 4, soit 250 Hz : sous le plafond de la chaîne, donc chaque trame arrive, et
 * l'écrêtage ne mord qu'au-delà de 127 × 250 = 31 750 comptes/s ≈ 80 cm/s — deux
 * fois la marge des 125 Hz d'origine.
 *
 * Aucune trame n'est émise à l'arrêt (voir mouse_task) : la dépense suit le
 * geste, elle n'est pas permanente. */
#define SENSOR_EVERY_N_SCANS  4

typedef struct {
    gpio_num_t  no, nc;
    bool        pressed;
    const char *nom;
    uint32_t    fronts;
    uint32_t    ambigus;   /* échantillons BOUNCING ou IMPOSSIBLE */
} bouton_t;

static bouton_t s_boutons[3] = {
    { BOARD_SW_LEFT_GPIO,  BOARD_SW_LEFT_NC_GPIO,  false, "gauche", 0, 0 },
    { BOARD_SW_RIGHT_GPIO, BOARD_SW_RIGHT_NC_GPIO, false, "droit",  0, 0 },
    { BOARD_SW_MID_GPIO,   BOARD_SW_MID_NC_GPIO,   false, "milieu", 0, 0 },
};

static int32_t s_wheel;
static uint32_t s_wheel_missed;

/* Anti-tache : jette le mouvement ISOLÉ sur un seul échantillon.
 *
 * ⚠ CE FILTRE NE JUGE PAS SUR L'AMPLITUDE, ET C'EST TOUT L'INTÉRÊT. Un premier
 * essai le 2026-08-26 avait posé un seuil sur la quantité de mouvement : il
 * mangeait les gestes fins tout en laissant passer le tremblement, parce que
 * l'amplitude ne sépare pas les deux. La DURÉE, si.
 *
 * Mesuré au banc, souris immobile, 63 salves de mouvement fantôme : longueur
 * médiane de 1 échantillon, maximum 3, et 42 sur 63 tenant sur un seul
 * échantillon. Un geste réel, à 250 Hz, occupe des DIZAINES d'échantillons
 * consécutifs — même le plus bref. Le critère est donc franc : est isolé ce qui
 * n'a de mouvement ni avant ni après.
 *
 * Coût : un échantillon de retard (4 ms), le temps de voir le suivant avant de
 * décider. Et sur un vrai geste, au pire le tout premier échantillon est perdu
 * s'il est précédé et suivi de rien — soit quelques comptes, borné, contre des
 * dizaines de taches supprimées.
 *
 * ⚠ CECI NE TRAITE QUE LE RÉGIME « TACHES ». Le capteur produit aussi, sur
 * certaines surfaces, des dérives COHÉRENTES de ~1000 comptes sur plusieurs
 * secondes (mesuré : dy=+996 en 2,5 s, souris immobile). Celles-là sont
 * indiscernables d'un geste lent volontaire et AUCUN filtre ne peut les retirer.
 * La cause est optique — voir NOTES-V2 §8, dépôt Conchodytes. */
static void anti_tache(int16_t *dx, int16_t *dy)
{
    static int16_t att_x, att_y;   /* échantillon en attente de décision */
    static bool    avant_bouge;    /* l'échantillon d'avant portait du mouvement */

    bool ici_bouge = (*dx || *dy);
    bool att_bouge = (att_x || att_y);

    int16_t sort_x = att_x, sort_y = att_y;
    if (att_bouge && !avant_bouge && !ici_bouge) {
        sort_x = 0; sort_y = 0;    /* tache isolée : on la jette */
    }

    att_x = *dx; att_y = *dy;
    avant_bouge = att_bouge;

    *dx = sort_x; *dy = sort_y;
}

/* Lissage adaptatif du déplacement — atténue le tremblement sans rogner les gestes.
 *
 * ⚠ POURQUOI PAS UN PORTILLON. Un filtre qui JETTE du mouvement sous un seuil a
 * été essayé au banc le 2026-08-26 puis retiré : il mangeait les gestes fins ET
 * ne calmait pas le tremblement. La raison est que, main posée sur la souris, le
 * micro-mouvement franchit n'importe quel seuil, ouvre le portillon, et tout
 * repasse — pendant que les déplacements lents volontaires, eux, restaient
 * dessous. Le pire des deux mondes.
 *
 * ICI RIEN N'EST JETÉ. Le déplacement est passé dans un filtre passe-bas dont le
 * gain continu vaut 1 : la DISTANCE TOTALE EST CONSERVÉE, seule sa répartition
 * dans le temps change. Le pire cas est donc un léger retard, jamais un geste
 * effacé.
 *
 * Le lissage s'ADAPTE À LA VITESSE, comme le filtre « 1 € » (Casiez, Roussel,
 * Vogel, CHI 2012), conçu pour le bruit de pointage :
 *   - un tremblement est lent et oscillant  → fortement lissé ;
 *   - un geste est rapide et cohérent       → passe tel quel, sans retard ajouté.
 * C'est ce couplage qui évite le compromis habituel entre stabilité au repos et
 * réactivité en mouvement : on n'a pas à choisir, la vitesse tranche.
 *
 * La fraction non encore émise est CONSERVÉE d'un échantillon à l'autre
 * (`reste_*`, en Q8). Sans ça, l'arrondi à l'entier rognerait quelques comptes à
 * chaque rapport et la souris parcourrait moins que la main — le défaut même qui
 * vient d'être corrigé côté radio. */
static void lissage(int16_t *dx, int16_t *dy)
{
#if BOARD_SNS_LISSAGE_ALPHA_MIN < 256
    static int32_t etat_x, etat_y;     /* vitesse lissée, Q8 */
    static int32_t reste_x, reste_y;   /* fraction non émise, Q8 */

    int32_t ax = *dx < 0 ? -*dx : *dx;
    int32_t ay = *dy < 0 ? -*dy : *dy;
    int32_t v  = ax + ay;
    if (v > BOARD_SNS_LISSAGE_VITESSE_MAX) v = BOARD_SNS_LISSAGE_VITESSE_MAX;

    /* alpha va de ALPHA_MIN (immobile) à 256 (rapide) : 256 = laisser passer. */
    int32_t alpha = BOARD_SNS_LISSAGE_ALPHA_MIN
                  + ((256 - BOARD_SNS_LISSAGE_ALPHA_MIN) * v)
                    / BOARD_SNS_LISSAGE_VITESSE_MAX;

    etat_x += ((((int32_t)*dx) * 256) - etat_x) * alpha / 256;
    etat_y += ((((int32_t)*dy) * 256) - etat_y) * alpha / 256;

    reste_x += etat_x;
    reste_y += etat_y;
    int32_t sx = reste_x / 256, sy = reste_y / 256;
    reste_x -= sx * 256;
    reste_y -= sy * 256;

    *dx = (int16_t)sx;
    *dy = (int16_t)sy;
#else
    (void)dx; (void)dy;
#endif
}

static void entrees_init(void)
{
    /* Les tirages sont EXTERNES — 10 k sur les six contacts (R105-R110) comme
     * sur les deux voies de l'encodeur (R103/R104). On n'arme pas ceux du S3,
     * qui se mettraient en parallèle et fausseraient les seuils. */
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << BOARD_SW_LEFT_GPIO)  | (1ULL << BOARD_SW_LEFT_NC_GPIO)  |
                        (1ULL << BOARD_SW_RIGHT_GPIO) | (1ULL << BOARD_SW_RIGHT_NC_GPIO) |
                        (1ULL << BOARD_SW_MID_GPIO)   | (1ULL << BOARD_SW_MID_NC_GPIO)   |
                        (1ULL << BOARD_ENC_A_GPIO)    | (1ULL << BOARD_ENC_B_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in));
}

static inline uint8_t lire_encodeur(void)
{
    return (uint8_t)((gpio_get_level(BOARD_ENC_A_GPIO) << 1) |
                      gpio_get_level(BOARD_ENC_B_GPIO));
}

static void mouse_task(void *arg)
{
    (void)arg;

    uint8_t enc_prev = lire_encodeur();
    unsigned n = 0;
    int32_t acc_dx = 0, acc_dy = 0;

    uint8_t last_squal = 0; uint16_t last_shutter = 0;
    int32_t rf_dx = 0, rf_dy = 0, rf_wheel = 0;   /* reste à émettre */
    uint32_t satur = 0;                            /* rapports écrêtés */
    uint32_t perdues = 0;                          /* trames non acquittées, comptes rendus */
    int32_t wheel_env = 0;

    /* LQ1 était mort sur la v1 et pinçait les deux voies à 0,65 V
     * (NOTES-V2.md §1bis). Tant qu'il n'est pas remplacé, `lire_encodeur` rend
     * toujours 0 et aucun pas n'est compté. On le signale une fois au démarrage
     * plutôt que de laisser croire à un défaut logiciel.
     *
     * 00 au démarrage n'est pas anormal en soi — c'est un état de quadrature
     * valide. Ce qui l'est, c'est qu'il ne change jamais. */
    if (enc_prev == 0)
        ESP_LOGW(TAG, "molette : ENC_A et ENC_B a 0 au demarrage — LQ1 sature "
                      "connu sur la v1, voir Conchodytes/NOTES-V2.md §1bis");

    while (1) {
        for (unsigned i = 0; i < 3; i++) {
            bouton_t *b = &s_boutons[i];
            mouse_contact_t c = mouse_contact_decode(gpio_get_level(b->no),
                                                     gpio_get_level(b->nc));
            if (c == MOUSE_CONTACT_BOUNCING || c == MOUSE_CONTACT_IMPOSSIBLE)
                b->ambigus++;

            bool suivant = mouse_button_next(b->pressed, c);
            if (suivant != b->pressed) {
                b->pressed = suivant;
                b->fronts++;
                ESP_LOGI(TAG, "clic %s : %s", b->nom, suivant ? "appuye" : "relache");
            }
        }

        /* ⚠ PROVISOIRE — ce scrutin est structurellement trop lent.
         *
         * À 240 transitions par tour, un tour par seconde en produit déjà 240,
         * contre 1000 échantillons/s ici : la marge disparaît dès qu'on tourne
         * un peu vite, et les pas perdus sont comptés dans s_wheel_missed puis
         * jetés — la molette « accroche » sans rien signaler.
         *
         * La bonne réponse est le périphérique PCNT du S3, qui décode la
         * quadrature en matériel : compteur signé, filtre anti-glitch, aucun
         * réveil CPU, et aucun pas perdu quelle que soit la vitesse. Non écrit
         * pour l'instant parce que l'encodeur est en court franc à la masse
         * (Conchodytes/NOTES-V2.md §1bis) et qu'aucune des deux approches ne
         * peut être validée sur carte avant réparation. */
        uint8_t enc = lire_encodeur();
        if (enc != enc_prev) {
            int8_t pas = mouse_wheel_step(enc_prev, enc);
            if (pas) s_wheel += pas;
            else     s_wheel_missed++;   /* deux voies changées : pas raté */
            enc_prev = enc;
        }

        if (++n % SENSOR_EVERY_N_SCANS == 0) {
            pmw3389_motion_t m;
            if (pmw3389_read_motion(&m) == ESP_OK) {
                anti_tache(&m.dx, &m.dy); /* jette l'isolé, garde le continu */
                lissage(&m.dx, &m.dy);    /* puis lisse ce qui reste */

                rf_dx += m.dx;
                rf_dy += m.dy;
                /* Le burst vide les compteurs à chaque lecture : on accumule
                 * ici, sinon chaque relevé ne vaut que 8 ms de mouvement et le
                 * journal se remplit de miettes. */
                acc_dx += m.dx;
                acc_dy += m.dy;

                last_squal = m.squal;
                last_shutter = m.shutter;
            }
        }

        /* ── Émission HID vers le slot 2 du dongle ──────────────────────────
         *
         * La trame porte x, y et molette en int8_t : ±127 par rapport. On
         * ACCUMULE le reste plutôt que de l'écrêter sèchement — un mouvement
         * vif se retrouve étalé sur les rapports suivants au lieu d'être perdu.
         * Le compteur `satur` mesure combien de fois la borne est atteinte :
         * c'est le chiffre qui tranchera le §7 de la spec, entre écrêter,
         * accumuler, et étendre la trame à int16_t.
         *
         * Le bouton milieu occupe le bit 2 de l'octet boutons, conformément au
         * descripteur HID souris standard (gauche=0, droit=1, milieu=2). */
        if (mouse_relay_active()) {
            int32_t w = s_wheel, dw = w - wheel_env; wheel_env = w;
            rf_wheel += dw;

            int8_t px = (rf_dx >  127) ? 127 : (rf_dx < -127) ? -127 : (int8_t)rf_dx;
            int8_t py = (rf_dy >  127) ? 127 : (rf_dy < -127) ? -127 : (int8_t)rf_dy;
            int8_t pw = (rf_wheel >  127) ? 127 : (rf_wheel < -127) ? -127 : (int8_t)rf_wheel;
            if (px != rf_dx || py != rf_dy || pw != rf_wheel) satur++;
            rf_dx -= px; rf_dy -= py; rf_wheel -= pw;

            uint8_t btn = (uint8_t)((s_boutons[0].pressed ? 1 : 0) |
                                    (s_boutons[1].pressed ? 2 : 0) |
                                    (s_boutons[2].pressed ? 4 : 0));

            static uint8_t btn_env; static bool jamais;
            if (px || py || pw || btn != btn_env || !jamais) {
                if (mouse_relay_send(btn, px, py, pw)) {
                    btn_env = btn; jamais = true;
                } else {
                    /* ⚠ TRAME NON ACQUITTÉE : ON REND LES COMPTES. Sans
                     * retransmission radio (ARC=0, voir mouse_relay_init), cette
                     * trame est perdue pour de bon. Le déplacement étant RELATIF,
                     * l'accumulateur avait déjà été vidé plus haut : sans ce
                     * retour, ce bout de geste s'efface et le curseur parcourt
                     * moins que la main.
                     *
                     * On ne réémet PAS la trame — rejouer du relatif ferait
                     * avancer deux fois. On remet les comptes, et la trame
                     * suivante porte la somme : une somme de déplacements est un
                     * déplacement, l'opération est juste par construction.
                     *
                     * `btn_env` n'est PAS mis à jour non plus : l'état des
                     * boutons est absolu, il doit être retenté tel quel. */
                    rf_dx += px; rf_dy += py; rf_wheel += pw;
                    perdues++;
                }
            }
        }

        /* Résumé périodique — provisoire, le temps de valider le matériel.
         * Disparaîtra quand la tâche produira de vrais rapports HID. */
        if (n % 500 == 0) {
            ESP_LOGI(TAG, "dx=%+6ld dy=%+6ld SQUAL=%3u Shutter=%5u | G%d D%d M%d"
                          " | molette %+5ld (rates %lu)",
                     (long)acc_dx, (long)acc_dy, last_squal, last_shutter,
                     s_boutons[0].pressed, s_boutons[1].pressed, s_boutons[2].pressed,
                     (long)s_wheel, (unsigned long)s_wheel_missed);
            if (mouse_relay_active()) {
                uint32_t tx, ack;
                mouse_relay_stats(&tx, &ack);
                ESP_LOGI(TAG, "  radio : %lu trames, %lu acquittees (%lu%%) | "
                              "reste dx=%ld dy=%ld | %lu ecretes | %lu rendues",
                         (unsigned long)tx, (unsigned long)ack,
                         (unsigned long)(tx ? ack * 100 / tx : 0),
                         (long)rf_dx, (long)rf_dy, (unsigned long)satur,
                         (unsigned long)perdues);
            }
            acc_dx = acc_dy = 0;
        }

        vTaskDelay(SCAN_PERIOD_TICKS);
    }
}

esp_err_t mouse_task_start(void)
{
    /* La NVS est désormais initialisée par app_main() pour tous les rôles
     * (voir main.c) — plus besoin de le faire ici. */
    entrees_init();

    /* Le capteur AVANT la radio : c'est lui qui initialise le bus SPI partagé
     * (mouse_relay_tx.h explique pourquoi l'ordre compte). */
    esp_err_t err = pmw3389_init();
    if (err != ESP_OK) {
        /* Le capteur peut manquer sans que la souris soit inutilisable : les
         * clics et la molette restent lisibles. On journalise fort et on
         * démarre quand même — une souris qui clique vaut mieux qu'une souris
         * morte, et le message dit quoi regarder. */
        ESP_LOGE(TAG, "capteur PMW3389 absent ou muet (%s) — clics et molette "
                      "seuls", esp_err_to_name(err));
    }

    err = mouse_relay_init();
    if (err != ESP_OK) {
        /* Une souris sans radio reste diagnosticable au port série ; on
         * journalise fort et on démarre quand même. */
        ESP_LOGE(TAG, "radio absente ou muette (%s) — pas de lien avec le dongle",
                 esp_err_to_name(err));
    } else if (!mouse_relay_active()) {
        /* Non appairée : on tente une fois au démarrage. La fenêtre du dongle
         * doit être ouverte (KS_CMD_RF_PAIR_START) — sinon la tentative échoue
         * proprement en vingt essais et la souris démarre quand même.
         *
         * ⚠ Provisoire, pour le banc. En usage réel l'appairage devra être
         * déclenché par un geste explicite — un appui long sur les trois clics,
         * par exemple — plutôt qu'à chaque démarrage : une souris qui cherche à
         * s'appairer en permanence émet sur le rendez-vous pour rien, et sur
         * batterie ça se paie. */
        ESP_LOGW(TAG, "non appairee : tentative d'appairage au demarrage");
        mouse_relay_pair();
    }

    BaseType_t ok = xTaskCreate(mouse_task, "mouse", 4096, NULL, 10, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
