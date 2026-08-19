/* Poignée de main 5 V du lien inter-moitiés.
 *
 * Enjeu matériel : fermer le load switch avant d'avoir reconnu la moitié d'en
 * face, c'est mettre du 5 V sur un connecteur exposé — exactement le tueur
 * historique des splits que ce design cherche à éliminer. La règle testée ici
 * est donc : LINK_5V_EN ne se lève JAMAIS sans une reconnaissance aboutie, et il
 * retombe dès que la moitié d'en face se tait.
 */
#include <string.h>
#include "test_framework.h"
#include "../main/comm/link/link_handshake.h"

static void test_starts_dead(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "au repos au départ");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort au départ");
}

static void test_probe_then_ack_enables_5v(void)
{
    link_hs_t hs;
    link_hs_init(&hs);

    /* On a l'USB : on sonde. */
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 1000);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_SEND_PROBE, "présence USB → sonder");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V toujours mort pendant la sonde");

    /* La moitié d'en face répond. */
    a = link_hs_step(&hs, LINK_HS_EV_PEER_ACK, 1050);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_ENABLE_5V, "pair reconnu → fermer le switch");
    TEST_ASSERT(link_hs_5v_enabled(&hs), "5 V vivant après reconnaissance");
    TEST_ASSERT_EQ(hs.state, LINK_HS_UP, "lien établi");
}

static void test_never_enables_without_ack(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);

    /* Du bruit sur la ligne, pas un ACK : rien ne doit se lever. */
    for (uint32_t t = 1; t < LINK_HS_PROBE_TIMEOUT_MS; t += 10) {
        link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_TICK, t);
        TEST_ASSERT(a != LINK_HS_ACT_ENABLE_5V, "aucun 5 V sans reconnaissance");
        TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V reste mort");
    }
}

static void test_probe_timeout_returns_to_idle(void)
{
    link_hs_t hs;
    link_hs_init(&hs);

    /* Précondition du test : on doit être réellement en PROBING avant le
     * timeout, sinon un stub qui ignorerait USB_PRESENT passerait ce test
     * pour de mauvaises raisons (NONE/IDLE/5V-mort sont aussi sa sortie). */
    link_hs_action_t a0 = link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);
    TEST_ASSERT_EQ(a0, LINK_HS_ACT_SEND_PROBE, "présence USB → sonder");
    TEST_ASSERT_EQ(hs.state, LINK_HS_PROBING, "en sonde avant le timeout");

    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_TICK, LINK_HS_PROBE_TIMEOUT_MS);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_NONE, "pas de réponse → on abandonne");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "retour au repos");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort après abandon");
}

static void test_peer_ack_ignored_in_idle(void)
{
    /* Un ACK non sollicité, avant toute sonde : on ne l'a jamais demandé, on
     * ne le croit pas. */
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_PEER_ACK, 5);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_NONE, "ACK non sollicité en IDLE → rien");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "reste au repos");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort");
}

static void test_peer_frame_ignored_in_probing(void)
{
    /* Une trame reçue avant l'ACK ne doit pas faire transitionner : seul
     * PEER_ACK vaut reconnaissance de la moitié d'en face. */
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_PEER_FRAME, 10);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_NONE, "trame sans ACK en PROBING → rien");
    TEST_ASSERT_EQ(hs.state, LINK_HS_PROBING, "reste en sonde");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort");
}

static void test_usb_gone_already_dead_from_idle(void)
{
    /* USB_GONE alors que le 5 V est déjà mort (jamais monté) : pas de
     * DISABLE_5V redondant, mais pas de crash ni d'état incohérent non plus. */
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_USB_GONE, 5);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_NONE, "5 V déjà mort en IDLE → pas de DISABLE_5V");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "reste au repos");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort");
}

static void test_usb_gone_already_dead_from_probing(void)
{
    /* Même chose depuis PROBING : la sonde est partie mais le 5 V n'est pas
     * encore levé, donc USB_GONE ne doit pas prétendre l'avoir coupé. */
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_USB_GONE, 10);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_NONE, "5 V déjà mort en PROBING → pas de DISABLE_5V");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "retour au repos");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort");
}

static void test_late_ack_after_timeout_still_accepted(void)
{
    /* Comportement épinglé, pas un bug : si aucun TICK n'a fait retomber
     * PROBING avant l'arrivée de l'ACK — même après le délai nominal — l'ACK
     * reste requis et est accepté. Accepter un ACK tardif vaut mieux que
     * forcer une re-sonde. Ne pas changer ce comportement sans décision
     * explicite ; ce test l'épingle. */
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);

    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_PEER_ACK,
                                       LINK_HS_PROBE_TIMEOUT_MS + 50);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_ENABLE_5V, "ACK tardif encore accepté (épinglé)");
    TEST_ASSERT(link_hs_5v_enabled(&hs), "5 V levé malgré le retard");
    TEST_ASSERT_EQ(hs.state, LINK_HS_UP, "lien établi malgré le retard");
}

static void test_unknown_state_falls_safe_without_init(void)
{
    /* Contrat d'appel violé : struct sur pile jamais passée par
     * link_hs_init(), remplie de mémoire arbitraire (ici 0xFF partout). Le
     * `default:` du switch doit retomber côté sûr — 5 V mort — plutôt que de
     * préserver un en_5v de poubelle qui lirait `true` sans qu'aucun
     * ACT_ENABLE_5V n'ait jamais été émis. */
    link_hs_t hs;
    memset(&hs, 0xFF, sizeof(hs));
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_TICK, 0);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_NONE, "état inconnu → aucune action");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "état inconnu → 5 V retombe mort");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "état inconnu → repli sur IDLE");
}

static void test_peer_silence_drops_5v(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);
    link_hs_step(&hs, LINK_HS_EV_PEER_ACK, 10);
    TEST_ASSERT(link_hs_5v_enabled(&hs), "5 V vivant");

    /* Le pair se tait : câble arraché. Le switch doit rouvrir. */
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_TICK, 10 + LINK_HS_PEER_TIMEOUT_MS);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_DISABLE_5V, "silence du pair → rouvrir");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort après arrachage");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "retour au repos");
}

static void test_peer_traffic_keeps_link_up(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);
    link_hs_step(&hs, LINK_HS_EV_PEER_ACK, 10);

    /* Trafic régulier : le lien tient indéfiniment. */
    for (uint32_t t = 10; t < 10 * LINK_HS_PEER_TIMEOUT_MS; t += LINK_HS_PEER_TIMEOUT_MS / 2) {
        link_hs_step(&hs, LINK_HS_EV_PEER_FRAME, t);
        TEST_ASSERT(link_hs_5v_enabled(&hs), "le trafic maintient le lien");
    }
}

static void test_usb_lost_drops_5v(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);
    link_hs_step(&hs, LINK_HS_EV_PEER_ACK, 10);

    /* Débranché : on ne nourrit plus personne. */
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_USB_GONE, 20);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_DISABLE_5V, "USB parti → rouvrir");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort sans USB");
}

static void test_no_probe_without_usb(void)
{
    /* Sur batterie seule, on ne sonde pas : on n'a rien à donner. */
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_TICK, 500);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_NONE, "pas d'USB → pas de sonde");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "on reste au repos");
}

void test_link_handshake(void)
{
    printf("\n-- poignée de main 5 V du lien --\n");
    test_starts_dead();
    test_probe_then_ack_enables_5v();
    test_never_enables_without_ack();
    test_probe_timeout_returns_to_idle();
    test_peer_silence_drops_5v();
    test_peer_traffic_keeps_link_up();
    test_usb_lost_drops_5v();
    test_no_probe_without_usb();
    test_peer_ack_ignored_in_idle();
    test_peer_frame_ignored_in_probing();
    test_usb_gone_already_dead_from_idle();
    test_usb_gone_already_dead_from_probing();
    test_late_ack_after_timeout_still_accepted();
    test_unknown_state_falls_safe_without_init();
}
