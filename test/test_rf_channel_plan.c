/* Plan de canaux 2,4 GHz du projet (logique pure).
 *
 * Quatre liens radio coexistent, et rien ne les empechait jusqu'ici de tomber
 * sur le meme canal : chacun etait defini dans son coin — rf_pairing.h pour
 * l'appairage, les board.h pour les liens vers le dongle. Une collision ne
 * casse aucune compilation ; elle se manifeste par un lien qui se tait, ou pire
 * par des paquets qui passent par intermittence selon le trafic.
 *
 * Ce test rassemble les quatre et verrouille leur separation. Le prochain qui
 * ajoute un lien se heurte a un rouge plutot qu'a un silence.
 *
 * Regle d'espacement, nRF24L01+ Product Specification §6.3 p.25 : « The channel
 * occupies a bandwidth of less than 1MHz at 250kbps and 1Mbps ». Le driver
 * emet a 1 Mbps (RF_SETUP = 0x06), donc 1 MHz d'ecart suffit. La contrainte de
 * 2 MHz ne vaut qu'a 2 Mbps.
 */
#include "test_framework.h"
#include "../main/comm/rf/rf_slot.h"
#include "../main/comm/rf/rf_pairing.h"

/* Bande ISM 2,4 GHz : 2400 a 2483,5 MHz. La puce monterait a 2525 (§6.3) mais
 * emettre au-dela sort de la bande, ce qui n'est pas qu'une affaire de
 * paperasse : on y brouille d'autres services. */
#define ISM_CH_MAX  83   /* 2483 MHz */

static void test_les_canaux_sont_dans_la_bande(void)
{
    const int ch[] = { RF_PAIR_CHANNEL, RF_CH_KBD_DONGLE,
                       RF_CH_MOUSE_DONGLE, RF_CH_HALF_LINK };
    const int n = (int)(sizeof(ch) / sizeof(ch[0]));
    for (int i = 0; i < n; i++) {
        TEST_ASSERT(ch[i] >= 0, "canal >= 2400 MHz");
        TEST_ASSERT(ch[i] <= ISM_CH_MAX, "canal dans la bande ISM (<= 2483 MHz)");
    }
}

static void test_aucune_collision_ni_recouvrement(void)
{
    /* LE test de ce fichier. Deux liens sur le meme canal, ou a moins d'1 MHz,
     * se brouillent — et le symptome est un lien qui se tait, pas une erreur. */
    const int ch[] = { RF_PAIR_CHANNEL, RF_CH_KBD_DONGLE,
                       RF_CH_MOUSE_DONGLE, RF_CH_HALF_LINK };
    const int n = (int)(sizeof(ch) / sizeof(ch[0]));
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            int d = ch[i] > ch[j] ? ch[i] - ch[j] : ch[j] - ch[i];
            TEST_ASSERT(d >= 1, "deux liens espaces d'au moins 1 MHz (1 Mbps)");
        }
}

static void test_le_lien_inter_moities_est_hors_wifi(void)
{
    /* Le WiFi 2,4 GHz occupe au plus jusqu'a ~2473 MHz (canal 11). Au-dessus la
     * bande est nettement plus calme — c'est pourquoi les liens du dongle y
     * sont deja. Le lien inter-moities les rejoint plutot que de redescendre. */
    TEST_ASSERT(RF_CH_HALF_LINK > 73, "au-dessus du WiFi canal 11 (~2473 MHz)");
    TEST_ASSERT(RF_CH_HALF_LINK < RF_CH_MOUSE_DONGLE, "sous le lien souris");
    TEST_ASSERT(RF_CH_HALF_LINK > RF_CH_KBD_DONGLE, "au-dessus du lien clavier");
}

static void test_le_suffixe_suit_la_convention(void)
{
    /* 0x01 clavier, 0x02 souris (rf_slot.h) — 0x03 pour le lien inter-moities. */
    TEST_ASSERT_EQ(RF_ADDR_HALF_LINK, 0x03, "suffixe du lien inter-moities");
}

void test_rf_channel_plan(void)
{
    printf("\n-- plan de canaux 2,4 GHz --\n");
    test_les_canaux_sont_dans_la_bande();
    test_aucune_collision_ni_recouvrement();
    test_le_lien_inter_moities_est_hors_wifi();
    test_le_suffixe_suit_la_convention();
}
