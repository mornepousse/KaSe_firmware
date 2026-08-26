/* Entrées de la souris Conchodytes : décodage des clics SPDT et quadrature.
 *
 * Ces deux logiques sont pures — des niveaux en entrée, un état en sortie — et
 * c'est délibéré : ce sont elles qui portent le raisonnement, pas le GPIO.
 * Les tester sur l'hôte permet de couvrir des cas que le banc ne produit pas à
 * la demande, en particulier la fenêtre de rebond et les transitions de
 * quadrature impossibles.
 *
 * Le comportement attendu vient d'une observation sur carte réelle du
 * 2026-08-25 : sur 24 transitions des trois clics, aucun front parasite n'a été
 * produit. La DURÉE de l'état ambigu, elle, n'est pas mesurée — la campagne
 * croyait scruter à 1 kHz alors que CONFIG_FREERTOS_HZ vaut 100 par défaut.
 * Ces tests ne dépendent d'aucune durée : ils raisonnent en nombre
 * d'échantillons, ce qui reste valable quelle que soit la cadence.
 */
#include "test_framework.h"
#include "../main/input/mouse_buttons.h"
#include "../main/input/mouse_wheel.h"

/* ── Décodage d'un contact SPDT ───────────────────────────────────────────
 *
 * COM à la masse, NO et NC tirés chacun au 3,3 V par 10 k.
 *   repos  : NC collé sur COM -> bas ; NO ouvert -> haut
 *   appuyé : NO collé sur COM -> bas ; NC ouvert -> haut
 *   rebond : le contact mobile n'est collé sur RIEN -> les deux hauts
 *   les deux bas : électriquement impossible (les deux contacts fermés)
 */
static void test_contact_decode(void)
{
    TEST_ASSERT_EQ(mouse_contact_decode(1, 0), MOUSE_CONTACT_RELEASED,
                   "NO haut + NC bas = repos");
    TEST_ASSERT_EQ(mouse_contact_decode(0, 1), MOUSE_CONTACT_PRESSED,
                   "NO bas + NC haut = appuye");
    TEST_ASSERT_EQ(mouse_contact_decode(1, 1), MOUSE_CONTACT_BOUNCING,
                   "les deux hauts = contact en l'air, rebond");
    TEST_ASSERT_EQ(mouse_contact_decode(0, 0), MOUSE_CONTACT_IMPOSSIBLE,
                   "les deux bas = impossible physiquement");
}

/* Le cœur de l'anti-rebond : pendant la fenêtre ambiguë, on garde l'état
 * précédent. C'est ce qui supprime le double-clic sans aucun filtrage
 * temporel — pas de compteur, pas de constante à régler. */
static void test_bounce_keeps_previous_state(void)
{
    TEST_ASSERT(mouse_button_next(false, MOUSE_CONTACT_BOUNCING) == false,
                "rebond depuis relache : reste relache");
    TEST_ASSERT(mouse_button_next(true, MOUSE_CONTACT_BOUNCING) == true,
                "rebond depuis appuye : reste appuye");

    /* Idem pour l'état impossible : on ne conclut rien plutôt que d'inventer. */
    TEST_ASSERT(mouse_button_next(false, MOUSE_CONTACT_IMPOSSIBLE) == false,
                "etat impossible depuis relache : on ne conclut rien");
    TEST_ASSERT(mouse_button_next(true, MOUSE_CONTACT_IMPOSSIBLE) == true,
                "etat impossible depuis appuye : on ne conclut rien");
}

static void test_button_transitions(void)
{
    TEST_ASSERT(mouse_button_next(false, MOUSE_CONTACT_PRESSED)  == true,  "relache -> appuye");
    TEST_ASSERT(mouse_button_next(true,  MOUSE_CONTACT_RELEASED) == false, "appuye -> relache");
    TEST_ASSERT(mouse_button_next(true,  MOUSE_CONTACT_PRESSED)  == true,  "appuye maintenu");
    TEST_ASSERT(mouse_button_next(false, MOUSE_CONTACT_RELEASED) == false, "relache maintenu");
}

/* Un appui réel tel que la carte le produit : repos, quelques échantillons de
 * rebond, appui franc, rebond au relâchement, repos.
 * Un seul front descendant et un seul front montant doivent en sortir. */
static void test_realistic_press_produces_exactly_two_edges(void)
{
    const mouse_contact_t sequence[] = {
        MOUSE_CONTACT_RELEASED, MOUSE_CONTACT_RELEASED,
        MOUSE_CONTACT_BOUNCING, MOUSE_CONTACT_BOUNCING, MOUSE_CONTACT_BOUNCING,
        MOUSE_CONTACT_PRESSED,  MOUSE_CONTACT_PRESSED,   MOUSE_CONTACT_PRESSED,
        MOUSE_CONTACT_BOUNCING, MOUSE_CONTACT_BOUNCING,
        MOUSE_CONTACT_RELEASED, MOUSE_CONTACT_RELEASED,
    };
    bool etat = false;
    unsigned fronts = 0;
    for (unsigned i = 0; i < sizeof(sequence) / sizeof(sequence[0]); i++) {
        bool suivant = mouse_button_next(etat, sequence[i]);
        if (suivant != etat) fronts++;
        etat = suivant;
    }
    TEST_ASSERT_EQ(fronts, 2, "un appui = exactement deux fronts, jamais quatre");
    TEST_ASSERT(etat == false, "on finit relache");
}

/* Le pire cas : un rebond qui traverse l'état appuyé sans s'y fixer. Sans le
 * maintien d'état, chaque aller-retour produirait une paire de fronts. */
static void test_chattering_does_not_multiply_clicks(void)
{
    const mouse_contact_t sequence[] = {
        MOUSE_CONTACT_RELEASED,
        MOUSE_CONTACT_BOUNCING, MOUSE_CONTACT_BOUNCING,
        MOUSE_CONTACT_BOUNCING, MOUSE_CONTACT_BOUNCING,
        MOUSE_CONTACT_BOUNCING, MOUSE_CONTACT_BOUNCING,
        MOUSE_CONTACT_PRESSED,
    };
    bool etat = false;
    unsigned fronts = 0;
    for (unsigned i = 0; i < sizeof(sequence) / sizeof(sequence[0]); i++) {
        bool suivant = mouse_button_next(etat, sequence[i]);
        if (suivant != etat) fronts++;
        etat = suivant;
    }
    TEST_ASSERT_EQ(fronts, 1, "six echantillons de rebond = toujours un seul front");
}

/* ── Quadrature de la molette ─────────────────────────────────────────────
 * L'état est (A << 1) | B. Un pas valide ne change qu'une voie à la fois. */
static void test_quadrature_forward(void)
{
    /* 00 -> 10 -> 11 -> 01 -> 00 : un sens complet. */
    TEST_ASSERT_EQ(mouse_wheel_step(0b00, 0b10), 1, "00->10 = +1");
    TEST_ASSERT_EQ(mouse_wheel_step(0b10, 0b11), 1, "10->11 = +1");
    TEST_ASSERT_EQ(mouse_wheel_step(0b11, 0b01), 1, "11->01 = +1");
    TEST_ASSERT_EQ(mouse_wheel_step(0b01, 0b00), 1, "01->00 = +1");
}

static void test_quadrature_backward(void)
{
    TEST_ASSERT_EQ(mouse_wheel_step(0b00, 0b01), -1, "00->01 = -1");
    TEST_ASSERT_EQ(mouse_wheel_step(0b01, 0b11), -1, "01->11 = -1");
    TEST_ASSERT_EQ(mouse_wheel_step(0b11, 0b10), -1, "11->10 = -1");
    TEST_ASSERT_EQ(mouse_wheel_step(0b10, 0b00), -1, "10->00 = -1");
}

static void test_quadrature_no_change_is_zero(void)
{
    for (uint8_t v = 0; v < 4; v++)
        TEST_ASSERT_EQ(mouse_wheel_step(v, v), 0, "pas de changement = pas de pas");
}

/* Les deux voies changeant dans le même intervalle : on a raté un pas. Rendre
 * 0 plutôt qu'un sens inventé — se tromper de sens est pire que de perdre un
 * cran, parce que ça fait reculer la page au lieu de ne rien faire. */
static void test_quadrature_impossible_transitions(void)
{
    TEST_ASSERT_EQ(mouse_wheel_step(0b00, 0b11), 0, "00->11 impossible = 0");
    TEST_ASSERT_EQ(mouse_wheel_step(0b11, 0b00), 0, "11->00 impossible = 0");
    TEST_ASSERT_EQ(mouse_wheel_step(0b01, 0b10), 0, "01->10 impossible = 0");
    TEST_ASSERT_EQ(mouse_wheel_step(0b10, 0b01), 0, "10->01 impossible = 0");
}

/* Un tour complet dans un sens puis dans l'autre doit ramener le compteur à
 * zéro : la table ne doit pas être asymétrique. */
static void test_quadrature_round_trip_is_neutral(void)
{
    const uint8_t avant[]  = { 0b00, 0b10, 0b11, 0b01, 0b00 };
    const uint8_t arriere[] = { 0b00, 0b01, 0b11, 0b10, 0b00 };
    int total = 0;
    for (unsigned i = 0; i + 1 < sizeof(avant) / sizeof(avant[0]); i++)
        total += mouse_wheel_step(avant[i], avant[i + 1]);
    TEST_ASSERT_EQ(total, 4, "un cycle complet en avant = +4 pas de quadrature");
    for (unsigned i = 0; i + 1 < sizeof(arriere) / sizeof(arriere[0]); i++)
        total += mouse_wheel_step(arriere[i], arriere[i + 1]);
    TEST_ASSERT_EQ(total, 0, "puis un cycle en arriere ramene a zero");
}

void test_mouse_input(void)
{
    printf("\n-- entrees souris Conchodytes : clics SPDT et quadrature --\n");
    test_contact_decode();
    test_bounce_keeps_previous_state();
    test_button_transitions();
    test_realistic_press_produces_exactly_two_edges();
    test_chattering_does_not_multiply_clicks();
    test_quadrature_forward();
    test_quadrature_backward();
    test_quadrature_no_change_is_zero();
    test_quadrature_impossible_transitions();
    test_quadrature_round_trip_is_neutral();
}
