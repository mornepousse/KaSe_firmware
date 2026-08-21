/* Drapeau « la matrice a changé » — audit F3, front perdu.
 *
 * Le drapeau relie un producteur prioritaire (callback de scan matrice en
 * priorité 5, ou réception RF sur le dongle) à un consommateur moins
 * prioritaire (la tâche clavier, priorité 3). Le producteur préempte le
 * consommateur à tout moment.
 *
 * La faute que ces tests interdisent : effacer le drapeau APRÈS avoir lu l'état.
 * Un producteur qui tombe entre la lecture et l'effacement voit son signal
 * écrasé, et son front disparaît — la touche n'arrive jamais à l'hôte.
 */
#include "test_framework.h"
#include "../main/input/matrix_flag.h"

static void test_idle_flag_has_nothing_to_take(void)
{
    volatile uint8_t f = 0;
    TEST_ASSERT(!matrix_flag_take(&f), "rien à prendre sur un drapeau au repos");
    TEST_ASSERT(!matrix_flag_take(&f), "et toujours rien au second appel");
}

static void test_take_consumes_the_signal(void)
{
    volatile uint8_t f = 0;
    matrix_flag_signal(&f);
    TEST_ASSERT(matrix_flag_take(&f), "le signal est pris");
    /* S'il n'était pas consommé, le consommateur retraiterait le même état en
     * boucle et le drapeau ne redescendrait jamais. */
    TEST_ASSERT(!matrix_flag_take(&f), "le signal a bien été consommé");
}

/* LE test de régression de F3. */
static void test_signal_during_the_read_survives(void)
{
    volatile uint8_t f = 0;

    matrix_flag_signal(&f);                          /* le scan détecte un front */
    TEST_ASSERT(matrix_flag_take(&f), "premier front pris");

    /* Ici le consommateur lit l'état de la matrice — c'est long, et le callback
     * de scan (prio 5) le préempte. Un nouveau front arrive PENDANT la lecture. */
    matrix_flag_signal(&f);

    /* Avec un effacement après lecture, le consommateur écraserait ce signal en
     * finissant son tour, et le front serait perdu pour toujours : rien ne le
     * réémet. En effaçant d'abord, il survit. */
    TEST_ASSERT(matrix_flag_take(&f),
                "le front arrivé pendant la lecture n'est pas perdu");
}

static void test_bursts_collapse_to_one_take(void)
{
    volatile uint8_t f = 0;
    /* Plusieurs fronts avant que le consommateur ne repasse : ils fusionnent.
     * C'est voulu — le consommateur relit l'état complet de la matrice, pas un
     * journal d'événements, donc un seul passage suffit à tout voir. */
    for (int i = 0; i < 5; i++) matrix_flag_signal(&f);
    TEST_ASSERT(matrix_flag_take(&f), "la rafale donne un passage");
    TEST_ASSERT(!matrix_flag_take(&f), "et un seul");
}

void test_matrix_flag(void)
{
    printf("\n-- drapeau matrice (F3 : front perdu) --\n");
    test_idle_flag_has_nothing_to_take();
    test_take_consumes_the_signal();
    test_signal_during_the_read_survives();
    test_bursts_collapse_to_one_take();
}
