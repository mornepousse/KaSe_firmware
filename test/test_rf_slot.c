/* Les deux slots RF du dongle : supervision de présence et repli sûr.
 *
 * Le dongle n'a plus deux moitiés de clavier en face de lui. Il a un clavier
 * (la moitié maître Niphargus, qui lui envoie du HID déjà fini) et une souris
 * (Conchodytes). Ces deux-là n'ont rien à voir l'un avec l'autre, et c'est
 * exactement ce que ces tests verrouillent : **perdre la souris ne doit pas
 * relâcher les touches**. Sous l'ancienne lecture — deux moitiés d'un même
 * clavier — relâcher tout à la perte de l'une ou l'autre était juste. Ça ne
 * l'est plus, et rien dans le code ne le rappelle sinon ici.
 *
 * Le repli existe parce qu'un lien qui se tait fige le dernier rapport reçu sur
 * l'hôte : si la dernière chose reçue était « Super enfoncé », il reste enfoncé
 * jusqu'au rebranchement.
 */
#include "test_framework.h"
#include "../main/comm/rf/rf_slot.h"

/* ── Le fond : un slot n'entraîne pas l'autre ────────────────────── */

static void test_losing_the_mouse_never_releases_keys(void)
{
    rf_slot_link_t mouse = {0};
    rf_slot_link_rx(&mouse, 1000);
    TEST_ASSERT_EQ(rf_slot_link_check(&mouse, RF_SLOT_MOUSE, 4000, 2500),
                   RF_SAFE_RELEASE_BUTTONS,
                   "souris muette → on relâche ses boutons");

    rf_slot_link_t kbd = {0};
    rf_slot_link_rx(&kbd, 1000);
    TEST_ASSERT_EQ(rf_slot_link_check(&kbd, RF_SLOT_KBD, 4000, 2500),
                   RF_SAFE_RELEASE_KEYS,
                   "clavier muet → on relâche ses touches");
}

/* ── Présence ────────────────────────────────────────────────────── */

static void test_a_slot_never_seen_has_nothing_to_release(void)
{
    /* Avant le premier paquet, il n'y a rien à relâcher : émettre un rapport
     * vide au démarrage écraserait ce qu'un autre chemin vient d'envoyer. */
    rf_slot_link_t l = {0};
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 999999, 2500), RF_SAFE_NONE,
                   "jamais vu → rien à relâcher");
}

static void test_a_fresh_link_is_left_alone(void)
{
    rf_slot_link_t l = {0};
    rf_slot_link_rx(&l, 1000);
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 1000, 2500), RF_SAFE_NONE, "à l'instant");
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 3499, 2500), RF_SAFE_NONE,
                   "juste avant l'échéance");
    TEST_ASSERT(l.up, "et le lien est toujours déclaré présent");
}

static void test_the_deadline_is_inclusive(void)
{
    rf_slot_link_t l = {0};
    rf_slot_link_rx(&l, 1000);
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 3500, 2500), RF_SAFE_RELEASE_KEYS,
                   "à l'échéance pile, le lien est perdu");
    TEST_ASSERT(!l.up, "et marqué absent");
}

/* ── Une seule fois ──────────────────────────────────────────────── */

static void test_the_release_fires_once_not_every_tick(void)
{
    /* La boucle RF appelle ceci toutes les 10 ms. Si la perte se redéclenchait
     * à chaque tour, un lien coupé noierait l'endpoint HID de rapports vides. */
    rf_slot_link_t l = {0};
    rf_slot_link_rx(&l, 1000);
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 4000, 2500), RF_SAFE_RELEASE_KEYS,
                   "première constatation de la perte");
    for (uint32_t t = 4010; t < 4500; t += 10)
        TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, t, 2500), RF_SAFE_NONE,
                       "puis plus rien tant que le lien ne revient pas");
}

static void test_a_returning_link_can_be_lost_again(void)
{
    rf_slot_link_t l = {0};
    rf_slot_link_rx(&l, 1000);
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 4000, 2500), RF_SAFE_RELEASE_KEYS, "perdu");
    rf_slot_link_rx(&l, 5000);
    TEST_ASSERT(l.up, "un paquet reçu remet le lien debout");
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 6000, 2500), RF_SAFE_NONE, "frais à nouveau");
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 8000, 2500), RF_SAFE_RELEASE_KEYS,
                   "et re-perdable");
}

/* ── Le compteur de millisecondes déborde ────────────────────────── */

static void test_survives_the_millisecond_counter_wrapping(void)
{
    /* esp_timer_get_time()/1000 tronqué en uint32 repasse par zéro après ~49
     * jours. Un dongle branché en permanence y arrive. Une soustraction non
     * signée traverse le débordement ; une comparaison `now > last + timeout`
     * n'y survivrait pas. */
    rf_slot_link_t l = {0};
    rf_slot_link_rx(&l, 0xFFFFFF00u);
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 0x00000300u, 2500), RF_SAFE_NONE,
                   "1 s après, à cheval sur le débordement : encore frais");
    TEST_ASSERT_EQ(rf_slot_link_check(&l, RF_SLOT_KBD, 0x00001000u, 2500), RF_SAFE_RELEASE_KEYS,
                   "et la perte est vue au bon moment, pas 49 jours plus tard");
}

/* ── Arguments ───────────────────────────────────────────────────── */

static void test_null_is_inert(void)
{
    rf_slot_link_rx(NULL, 1000);   /* ne doit pas planter */
    TEST_ASSERT_EQ(rf_slot_link_check(NULL, RF_SLOT_KBD, 9999, 2500), RF_SAFE_NONE, "NULL → rien");
}

static void test_the_two_slots_are_distinct(void)
{
    TEST_ASSERT(RF_SLOT_KBD != RF_SLOT_MOUSE, "clavier et souris ne sont pas le même slot");
    TEST_ASSERT_EQ(RF_SLOT_COUNT, 2, "le dongle en a deux, pas plus");
}

void test_rf_slot(void)
{
    printf("\n-- slots RF du dongle --\n");
    test_losing_the_mouse_never_releases_keys();
    test_a_slot_never_seen_has_nothing_to_release();
    test_a_fresh_link_is_left_alone();
    test_the_deadline_is_inclusive();
    test_the_release_fires_once_not_every_tick();
    test_a_returning_link_can_be_lost_again();
    test_survives_the_millisecond_counter_wrapping();
    test_null_is_inert();
    test_the_two_slots_are_distinct();
}
