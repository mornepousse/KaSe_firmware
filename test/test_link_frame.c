/* Trame du lien inter-moitiés (TRRS / UART1).
 *
 * Le lien est exposé au connecteur : il est débranché à chaud, il prend de
 * l'ESD, et l'octet suivant peut arriver au milieu d'une trame. Le décodeur doit
 * donc rejeter proprement, jamais lire hors des bornes, et surtout **dire de
 * combien avancer** — sans quoi un flux bruité ne se resynchronise jamais.
 *
 * Il porte trois types : la matrice, et les deux trames de contrôle PROBE / ACK
 * dont la poignée de main 5 V a besoin (link_handshake.h).
 */
#include "test_framework.h"
#include "../main/comm/link/link_frame.h"

static const uint8_t BM[RF_HALF_BITMAP_BYTES] = {0xA5, 0x00, 0x3C, 0xFF, 0x01};

/* ── Aller-retour ────────────────────────────────────────────────── */

static void test_roundtrip_matrix(void)
{
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, BM, 42);
    TEST_ASSERT_EQ(n, LINK_FRAME_MAX, "la trame matrice fait la taille max");

    link_frame_t f; uint16_t used = 0;
    TEST_ASSERT_EQ(link_decode(buf, n, &f, &used), LINK_DECODE_FRAME, "décodée");
    TEST_ASSERT_EQ(used, n, "tous les octets consommés");
    TEST_ASSERT_EQ(f.type, LINK_TYPE_MATRIX, "type conservé");
    TEST_ASSERT_EQ(f.seq, 42, "seq conservé");
    for (int i = 0; i < RF_HALF_BITMAP_BYTES; i++)
        TEST_ASSERT_EQ(f.bitmap[i], BM[i], "bitmap conservé octet par octet");
}

static void test_roundtrip_control_frames(void)
{
    uint8_t buf[LINK_FRAME_MAX];
    link_frame_t f; uint16_t used = 0;

    uint16_t n = link_encode_ctrl(buf, LINK_TYPE_PROBE, 7);
    TEST_ASSERT_EQ(n, LINK_FRAME_MIN, "une trame de contrôle est courte");
    TEST_ASSERT_EQ(link_decode(buf, n, &f, &used), LINK_DECODE_FRAME, "PROBE décodée");
    TEST_ASSERT_EQ(used, n, "consommation exacte d'une trame courte");
    TEST_ASSERT_EQ(f.type, LINK_TYPE_PROBE, "type PROBE");
    TEST_ASSERT_EQ(f.seq, 7, "seq PROBE");

    n = link_encode_ctrl(buf, LINK_TYPE_ACK, 8);
    TEST_ASSERT_EQ(link_decode(buf, n, &f, &used), LINK_DECODE_FRAME, "ACK décodée");
    TEST_ASSERT_EQ(f.type, LINK_TYPE_ACK, "type ACK");
}

/* ── Resynchronisation : le cœur du contrat ──────────────────────── */

static void test_incomplete_input_asks_for_more(void)
{
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, BM, 3);
    link_frame_t f; uint16_t used = 99;

    /* Toute troncature demande plus d'octets, sans jamais rien consommer :
     * consommer une trame partielle la perdrait définitivement. */
    for (uint16_t cut = 0; cut < n; cut++) {
        used = 99;
        TEST_ASSERT_EQ(link_decode(buf, cut, &f, &used), LINK_DECODE_NEED_MORE,
                       "trame tronquée → il en faut plus");
        TEST_ASSERT_EQ(used, 0, "rien n'est consommé sur une trame incomplète");
    }
    TEST_ASSERT_EQ(link_decode(buf, n, &f, &used), LINK_DECODE_FRAME, "complète → décodée");
}

static void test_garbage_byte_is_skipped_one_at_a_time(void)
{
    /* Un octet qui n'est pas un début de trame se jette seul — pas plus, sinon
     * on avalerait le vrai SOF qui le suit peut-être immédiatement. */
    uint8_t buf[4] = {0x00, 0x11, 0x22, 0x33};
    link_frame_t f; uint16_t used = 0;
    TEST_ASSERT_EQ(link_decode(buf, sizeof(buf), &f, &used), LINK_DECODE_SKIP,
                   "octet parasite → à jeter");
    TEST_ASSERT_EQ(used, 1, "un seul octet jeté à la fois");
}

static void test_resyncs_onto_a_frame_buried_in_noise(void)
{
    /* Le cas réel : on rebranche le jack en pleine frappe et le tampon commence
     * par du bruit. En avançant de ce que le décodeur dit avoir consommé, on
     * doit finir par tomber sur la trame. */
    uint8_t stream[3 + LINK_FRAME_MAX];
    stream[0] = 0x00; stream[1] = 0xFF; stream[2] = 0x4E;  /* dont un faux SOF */
    link_encode_matrix(&stream[3], BM, 55);

    uint16_t off = 0;
    link_frame_t f; uint16_t used = 0;
    link_decode_status_t st = LINK_DECODE_SKIP;
    int guard = 0;
    while (off < sizeof(stream) && guard++ < 32) {
        st = link_decode(&stream[off], (uint16_t)(sizeof(stream) - off), &f, &used);
        if (st == LINK_DECODE_FRAME) break;
        TEST_ASSERT(used > 0 || st == LINK_DECODE_NEED_MORE,
                    "un SKIP consomme toujours au moins un octet — sinon boucle infinie");
        off += used;
    }
    TEST_ASSERT_EQ(st, LINK_DECODE_FRAME, "la trame enfouie finit par être trouvée");
    TEST_ASSERT_EQ(f.seq, 55, "et c'est la bonne");
}

/* ── Rejets ──────────────────────────────────────────────────────── */

static void test_bad_crc_is_skipped_not_trusted(void)
{
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, BM, 7);
    buf[n - 1] ^= 0xFF;                       /* CRC corrompu */
    link_frame_t f; uint16_t used = 0;
    TEST_ASSERT_EQ(link_decode(buf, n, &f, &used), LINK_DECODE_SKIP, "CRC faux → jetée");
    TEST_ASSERT_EQ(used, 1, "on repart à l'octet suivant, le vrai SOF est peut-être dedans");

    n = link_encode_matrix(buf, BM, 7);
    buf[4] ^= 0x01;                           /* charge utile corrompue */
    TEST_ASSERT_EQ(link_decode(buf, n, &f, &used), LINK_DECODE_SKIP, "charge corrompue → jetée");
}

static void test_absurd_length_is_skipped(void)
{
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, BM, 1);
    link_frame_t f; uint16_t used = 0;

    buf[1] = 0xFF;                            /* longueur annoncée impossible */
    TEST_ASSERT_EQ(link_decode(buf, n, &f, &used), LINK_DECODE_SKIP, "longueur absurde → jetée");
    TEST_ASSERT_EQ(used, 1, "ce SOF était du bruit, on avance d'un octet");

    n = link_encode_matrix(buf, BM, 1);
    buf[1] = 0;                               /* trop courte pour porter un type */
    TEST_ASSERT_EQ(link_decode(buf, n, &f, &used), LINK_DECODE_SKIP, "longueur nulle → jetée");
}

/* ── Compatibilité ascendante ────────────────────────────────────── */

static void test_unknown_type_is_consumed_whole(void)
{
    /* Une moitié plus récente enverra des types que celle-ci ne connaît pas.
     * Ils doivent être avalés proprement, pas resynchronisés octet par octet :
     * c'est toute la raison d'un format préfixé par sa longueur. */
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_ctrl(buf, LINK_TYPE_ACK, 9);
    buf[2] = 0x7E;                            /* type inconnu */
    buf[n - 1] = link_crc8(&buf[1], (uint16_t)(1 + buf[1]));   /* CRC recalculé */

    link_frame_t f; uint16_t used = 0;
    link_decode_status_t st = link_decode(buf, n, &f, &used);
    TEST_ASSERT(st == LINK_DECODE_FRAME || st == LINK_DECODE_SKIP,
                "un type inconnu mais bien formé ne bloque pas le flux");
    TEST_ASSERT_EQ(used, n, "il est consommé EN ENTIER, pas octet par octet");
}

static void test_matrix_type_with_wrong_length_is_rejected(void)
{
    /* Type MATRIX mais longueur de trame de contrôle : bien encadré, mais
     * incohérent. On consomme la trame annoncée et on la jette. */
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_ctrl(buf, LINK_TYPE_MATRIX, 4);
    buf[n - 1] = link_crc8(&buf[1], (uint16_t)(1 + buf[1]));

    link_frame_t f; uint16_t used = 0;
    TEST_ASSERT_EQ(link_decode(buf, n, &f, &used), LINK_DECODE_SKIP,
                   "MATRIX sans son bitmap → incohérente, jetée");
    TEST_ASSERT_EQ(used, n, "mais consommée en entier, le cadrage était bon");
}

/* ── Arguments ───────────────────────────────────────────────────── */

static void test_null_arguments(void)
{
    uint8_t buf[LINK_FRAME_MAX];
    link_frame_t f; uint16_t used = 0;
    TEST_ASSERT_EQ(link_encode_matrix(NULL, BM, 0), 0, "buf NULL → 0");
    TEST_ASSERT_EQ(link_encode_matrix(buf, NULL, 0), 0, "bitmap NULL → 0");
    TEST_ASSERT_EQ(link_encode_ctrl(NULL, LINK_TYPE_PROBE, 0), 0, "ctrl buf NULL → 0");
    TEST_ASSERT_EQ(link_decode(NULL, 8, &f, &used), LINK_DECODE_NEED_MORE, "buf NULL");
    TEST_ASSERT_EQ(link_decode(buf, 8, NULL, &used), LINK_DECODE_NEED_MORE, "sortie NULL");
    TEST_ASSERT_EQ(link_decode(buf, 8, &f, NULL), LINK_DECODE_NEED_MORE, "compteur NULL");
}

/* Le CRC du lien est celui du protocole CDC, pas une réimplémentation. */
static void test_crc_is_the_repo_one(void)
{
    const uint8_t v1[] = {0x01, 0x02, 0x03};
    const uint8_t v2[] = {0xFF, 0x00, 0xA5, 0x5A};
    TEST_ASSERT_EQ(link_crc8(v1, sizeof(v1)), ks_crc8(v1, sizeof(v1)), "link_crc8 == ks_crc8");
    TEST_ASSERT_EQ(link_crc8(v2, sizeof(v2)), ks_crc8(v2, sizeof(v2)), "idem sur un autre vecteur");
}

void test_link_frame(void)
{
    printf("\n-- trame du lien inter-moitiés --\n");
    test_roundtrip_matrix();
    test_roundtrip_control_frames();
    test_incomplete_input_asks_for_more();
    test_garbage_byte_is_skipped_one_at_a_time();
    test_resyncs_onto_a_frame_buried_in_noise();
    test_bad_crc_is_skipped_not_trusted();
    test_absurd_length_is_skipped();
    test_unknown_type_is_consumed_whole();
    test_matrix_type_with_wrong_length_is_rejected();
    test_null_arguments();
    test_crc_is_the_repo_one();
}
