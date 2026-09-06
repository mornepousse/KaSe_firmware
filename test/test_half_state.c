/* Etat de la moitie distante vu par le maitre (logique pure).
 *
 * La gauche fusionne sa propre matrice avec celle que la droite lui envoie par
 * radio. Trois choses doivent etre justes, et la troisieme est la plus
 * dangereuse :
 *
 *   1. ce qui est recu est restitue tel quel ;
 *   2. un paquet plus recent remplace le precedent — l'etat est absolu, pas
 *      differentiel, donc une trame perdue se rattrape a la suivante ;
 *   3. QUAND LE LIEN SE TAIT, LES TOUCHES SE RELACHENT. Sans cela une moitie
 *      qui sort de portee ou dont la pile meurt laisse l'hote sur le dernier
 *      etat recu — et si c'etait « Maj enfoncee », il le reste. rf_slot.h
 *      documente ce repli et previent qu'un relachement mal cible serait pire
 *      que le mal : on ne relache QUE ce que cette moitie tenait.
 */
#include "test_framework.h"
#include "../main/comm/rf/half_link.h"

static void bitmap_avec(uint8_t *bm, uint8_t r, uint8_t c)
{
    memset(bm, 0, RF_HALF_BITMAP_BYTES);
    rf_bitmap_set(bm, r, c, true);
}

static void test_etat_neuf_est_vide(void)
{
    half_state_t st = {0};
    for (uint8_t r = 0; r < RF_HALF_ROWS; r++)
        for (uint8_t c = 0; c < RF_HALF_COLS; c++)
            TEST_ASSERT(!half_state_pressed(&st, r, c), "rien d'enfonce au depart");
}

static void test_ce_qui_est_recu_est_restitue(void)
{
    half_state_t st = {0};
    uint8_t bm[RF_HALF_BITMAP_BYTES];
    bitmap_avec(bm, 2, 5);
    half_state_recu(&st, bm, 1000);
    TEST_ASSERT(half_state_pressed(&st, 2, 5), "la touche recue est enfoncee");
    TEST_ASSERT(!half_state_pressed(&st, 2, 4), "sa voisine ne l'est pas");
    TEST_ASSERT(!half_state_pressed(&st, 1, 5), "celle du dessus non plus");
}

static void test_un_paquet_remplace_le_precedent(void)
{
    /* L'etat est absolu : une trame perdue se rattrape a la suivante, sans
     * accumulation ni derive. */
    half_state_t st = {0};
    uint8_t bm[RF_HALF_BITMAP_BYTES];
    bitmap_avec(bm, 0, 0);
    half_state_recu(&st, bm, 1000);
    bitmap_avec(bm, 3, 6);
    half_state_recu(&st, bm, 1010);
    TEST_ASSERT(half_state_pressed(&st, 3, 6), "la nouvelle touche est enfoncee");
    TEST_ASSERT(!half_state_pressed(&st, 0, 0), "l'ancienne est relachee");
}

static void test_le_silence_relache_tout(void)
{
    /* LE test de ce fichier. */
    half_state_t st = {0};
    uint8_t bm[RF_HALF_BITMAP_BYTES];
    bitmap_avec(bm, 1, 1);
    half_state_recu(&st, bm, 1000);

    TEST_ASSERT(!half_state_timeout(&st, 1200, 500), "pas encore expire (200 < 500)");
    TEST_ASSERT(half_state_pressed(&st, 1, 1), "la touche tient toujours");

    TEST_ASSERT(half_state_timeout(&st, 1600, 500), "expire (600 > 500) -> changement");
    TEST_ASSERT(!half_state_pressed(&st, 1, 1), "tout est relache");
}

static void test_expiration_ne_se_repete_pas(void)
{
    /* Un second appel ne doit PAS re-signaler un changement : le moteur
     * relacherait des touches deja relachees a chaque cycle. */
    half_state_t st = {0};
    uint8_t bm[RF_HALF_BITMAP_BYTES];
    bitmap_avec(bm, 1, 1);
    half_state_recu(&st, bm, 1000);
    TEST_ASSERT(half_state_timeout(&st, 1600, 500), "premiere expiration signalee");
    TEST_ASSERT(!half_state_timeout(&st, 1700, 500), "la seconde ne signale rien");
    TEST_ASSERT(!half_state_timeout(&st, 9999, 500), "ni les suivantes");
}

static void test_le_lien_peut_revenir(void)
{
    half_state_t st = {0};
    uint8_t bm[RF_HALF_BITMAP_BYTES];
    bitmap_avec(bm, 1, 1);
    half_state_recu(&st, bm, 1000);
    half_state_timeout(&st, 1600, 500);
    /* La moitie revient : elle doit etre entendue de nouveau. */
    bitmap_avec(bm, 2, 2);
    half_state_recu(&st, bm, 2000);
    TEST_ASSERT(half_state_pressed(&st, 2, 2), "la moitie revenue est entendue");
    TEST_ASSERT(!half_state_timeout(&st, 2100, 500), "et n'expire pas aussitot");
}

void test_half_state(void)
{
    printf("\n-- etat de la moitie distante (fusion + repli) --\n");
    test_etat_neuf_est_vide();
    test_ce_qui_est_recu_est_restitue();
    test_un_paquet_remplace_le_precedent();
    test_le_silence_relache_tout();
    test_expiration_ne_se_repete_pas();
    test_le_lien_peut_revenir();
}
