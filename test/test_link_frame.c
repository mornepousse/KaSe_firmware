/* Trame du lien inter-moitiés (TRRS / UART1).
 *
 * Le lien est exposé au connecteur : il est débranché à chaud, il prend de
 * l'ESD, et l'octet suivant peut arriver au milieu d'une trame. Le décodeur doit
 * donc rejeter proprement, jamais lire hors des bornes, et se resynchroniser.
 */
#include "test_framework.h"
#include "../main/comm/link/link_frame.h"

static void test_roundtrip_matrix(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {0xA5, 0x00, 0x3C, 0xFF, 0x01};
    uint8_t buf[LINK_FRAME_MAX];

    uint16_t n = link_encode_matrix(buf, bitmap, 42);
    TEST_ASSERT(n > 0, "l'encodage réussit");
    TEST_ASSERT(n <= LINK_FRAME_MAX, "la trame tient dans le buffer annoncé");

    link_frame_t out;
    TEST_ASSERT(link_decode(buf, n, &out), "le décodage réussit");
    TEST_ASSERT_EQ(out.type, LINK_TYPE_MATRIX, "type conservé");
    TEST_ASSERT_EQ(out.seq, 42, "seq conservé");
    for (int i = 0; i < RF_HALF_BITMAP_BYTES; i++)
        TEST_ASSERT_EQ(out.bitmap[i], bitmap[i], "bitmap conservé octet par octet");
}

static void test_rejects_bad_crc(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {1, 2, 3, 4, 5};
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, bitmap, 7);

    buf[n - 1] ^= 0xFF;             /* corrompt le CRC */
    link_frame_t out;
    TEST_ASSERT(!link_decode(buf, n, &out), "CRC faux → rejet");

    uint16_t m = link_encode_matrix(buf, bitmap, 7);
    buf[3] ^= 0x01;                 /* corrompt la charge utile */
    TEST_ASSERT(!link_decode(buf, m, &out), "charge utile corrompue → rejet");
}

static void test_rejects_bad_sof(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {0};
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, bitmap, 0);
    buf[0] = 0x00;
    link_frame_t out;
    TEST_ASSERT(!link_decode(buf, n, &out), "octet de début faux → rejet");
}

static void test_rejects_truncated(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {9, 9, 9, 9, 9};
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, bitmap, 3);
    link_frame_t out;
    /* Toute troncature doit être rejetée, jamais lue au-delà du tampon. */
    for (uint16_t cut = 0; cut < n; cut++)
        TEST_ASSERT(!link_decode(buf, cut, &out), "trame tronquée → rejet");
    TEST_ASSERT(link_decode(buf, n, &out), "trame complète → acceptée");
}

static void test_rejects_length_lie(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {1, 1, 1, 1, 1};
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, bitmap, 1);
    buf[1] = 0xFF;                  /* longueur annoncée absurde */
    link_frame_t out;
    TEST_ASSERT(!link_decode(buf, n, &out), "longueur mensongère → rejet");
}

static void test_null_arguments(void)
{
    uint8_t buf[LINK_FRAME_MAX];
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {0};
    link_frame_t out;
    TEST_ASSERT_EQ(link_encode_matrix(NULL, bitmap, 0), 0, "buf NULL → 0");
    TEST_ASSERT_EQ(link_encode_matrix(buf, NULL, 0), 0, "bitmap NULL → 0");
    TEST_ASSERT(!link_decode(NULL, 8, &out), "buf NULL → rejet");
    TEST_ASSERT(!link_decode(buf, 8, NULL), "sortie NULL → rejet");
}

void test_link_frame(void)
{
    printf("\n-- trame du lien inter-moitiés --\n");
    test_roundtrip_matrix();
    test_rejects_bad_crc();
    test_rejects_bad_sof();
    test_rejects_truncated();
    test_rejects_length_lie();
    test_null_arguments();
}
