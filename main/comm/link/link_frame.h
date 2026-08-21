/* Trame du lien inter-moitiés Niphargus (TRRS, UART1).
 *
 * Logique pure, entièrement en inline : aucune UART, aucun FreeRTOS. La couche
 * transport appelle les encodeurs pour émettre, et link_decode() sur ce qu'elle
 * a reçu.
 *
 * ── Format ───────────────────────────────────────────────────────────────────
 *   [0]        SOF 0x4E ('N')
 *   [1]        longueur de la charge utile (type + seq [+ bitmap])
 *   [2]        type
 *   [3]        seq
 *   [4..]      bitmap 5 octets, pour MATRIX seulement
 *   [dernier]  CRC-8 sur les octets [1] à l'avant-dernier
 *
 * ── Pourquoi un décodeur qui rend ce qu'il a consommé ────────────────────────
 *
 * Le lien est exposé au connecteur : on le débranche à chaud, il prend de l'ESD,
 * et le premier octet reçu tombe volontiers au milieu d'une trame. Un décodeur
 * qui répond seulement « valide / pas valide » ne dit pas à l'appelant de combien
 * avancer, et un flux bruité ne se resynchronise alors jamais — c'est justement
 * ce qu'un format préfixé par sa longueur est censé offrir.
 *
 * D'où le contrat à trois issues :
 *   NEED_MORE  rien n'est consommé, rappeler avec plus d'octets ;
 *   FRAME      une trame valide est dans *out, `consumed` octets consommés ;
 *   SKIP       `consumed` octets à jeter (bruit, CRC faux, trame incohérente).
 *
 * Un SKIP consomme toujours au moins un octet : sans cette garantie, une boucle
 * de resynchronisation tournerait indéfiniment sur le même octet.
 *
 * ── Compatibilité ascendante ─────────────────────────────────────────────────
 *
 * Une trame bien cadrée dont le type est inconnu est consommée EN ENTIER plutôt
 * que resynchronisée octet par octet. Une moitié plus récente peut donc parler à
 * une plus ancienne sans lui embrouiller le flux.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "rf_packet.h"             /* RF_HALF_BITMAP_BYTES — même bitmap qu'en RF */
#include "cdc_binary_protocol.h"   /* ks_crc8 */

#define LINK_SOF          0x4Eu

#define LINK_TYPE_MATRIX  0x01u    /* état de la demi-matrice */
#define LINK_TYPE_PROBE   0x02u    /* « t'es bien ma moitié ? » (poignée de main 5 V) */
#define LINK_TYPE_ACK     0x03u    /* réponse à une sonde */

/* Charge utile = type + seq, plus le bitmap pour MATRIX. */
#define LINK_PAYLOAD_CTRL    2
#define LINK_PAYLOAD_MATRIX  (2 + RF_HALF_BITMAP_BYTES)
#define LINK_PAYLOAD_MAX     LINK_PAYLOAD_MATRIX

/* SOF + longueur + charge utile + CRC */
#define LINK_FRAME_MAX  (2 + LINK_PAYLOAD_MAX + 1)
#define LINK_FRAME_MIN  (2 + LINK_PAYLOAD_CTRL + 1)

typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t bitmap[RF_HALF_BITMAP_BYTES];   /* rempli pour MATRIX, sinon zéro */
} link_frame_t;

typedef enum {
    LINK_DECODE_NEED_MORE = 0,   /* rien consommé, rappeler avec plus d'octets */
    LINK_DECODE_FRAME,           /* trame valide dans *out */
    LINK_DECODE_SKIP,            /* octets à jeter */
} link_decode_status_t;

/* Le lien réutilise ks_crc8() du protocole CDC binaire plutôt que de
 * réimplémenter un CRC-8 : une seule implémentation à relire dans le dépôt.
 * link_crc8() reste une fonction à part pour préserver l'interface du module et
 * documenter que c'est bien le CRC du dépôt qui est utilisé ici. */
static inline uint8_t link_crc8(const uint8_t *data, uint16_t len)
{
    return ks_crc8(data, len);
}

/* ── Encodeurs : écrivent dans buf, rendent le nombre d'octets (0 si erreur) ── */

static inline uint16_t link_encode_matrix(uint8_t *buf,
                                          const uint8_t bitmap[RF_HALF_BITMAP_BYTES],
                                          uint8_t seq)
{
    if (buf == NULL || bitmap == NULL) return 0;
    buf[0] = LINK_SOF;
    buf[1] = LINK_PAYLOAD_MATRIX;
    buf[2] = LINK_TYPE_MATRIX;
    buf[3] = seq;
    memcpy(&buf[4], bitmap, RF_HALF_BITMAP_BYTES);
    buf[LINK_FRAME_MAX - 1] = link_crc8(&buf[1], 1 + LINK_PAYLOAD_MATRIX);
    return LINK_FRAME_MAX;
}

/* Trames de contrôle sans charge utile : PROBE et ACK. */
static inline uint16_t link_encode_ctrl(uint8_t *buf, uint8_t type, uint8_t seq)
{
    if (buf == NULL) return 0;
    buf[0] = LINK_SOF;
    buf[1] = LINK_PAYLOAD_CTRL;
    buf[2] = type;
    buf[3] = seq;
    buf[LINK_FRAME_MIN - 1] = link_crc8(&buf[1], 1 + LINK_PAYLOAD_CTRL);
    return LINK_FRAME_MIN;
}

/* ── Décodeur resynchronisant ───────────────────────────────────────────────── */

static inline link_decode_status_t link_decode(const uint8_t *buf, uint16_t len,
                                               link_frame_t *out, uint16_t *consumed)
{
    if (consumed != NULL) *consumed = 0;
    if (buf == NULL || out == NULL || consumed == NULL) return LINK_DECODE_NEED_MORE;

    if (len == 0) return LINK_DECODE_NEED_MORE;

    /* Pas un début de trame : on jette CET octet seulement. En jeter plus
     * risquerait d'avaler le vrai SOF qui suit peut-être immédiatement. */
    if (buf[0] != LINK_SOF) { *consumed = 1; return LINK_DECODE_SKIP; }

    if (len < 2) return LINK_DECODE_NEED_MORE;      /* la longueur manque encore */

    uint8_t payload_len = buf[1];
    if (payload_len < LINK_PAYLOAD_CTRL || payload_len > LINK_PAYLOAD_MAX) {
        /* Longueur impossible : ce 0x4E était du bruit, pas un SOF. */
        *consumed = 1;
        return LINK_DECODE_SKIP;
    }

    uint16_t total = (uint16_t)(2 + payload_len + 1);
    if (len < total) return LINK_DECODE_NEED_MORE;  /* trame incomplète */

    if (link_crc8(&buf[1], (uint16_t)(1 + payload_len)) != buf[total - 1]) {
        /* CRC faux : on ne peut pas se fier au cadrage annoncé — un vrai SOF se
         * cache peut-être à l'intérieur. On repart à l'octet suivant. */
        *consumed = 1;
        return LINK_DECODE_SKIP;
    }

    /* À partir d'ici le cadrage est authentifié : quoi qu'on décide du contenu,
     * on consomme la trame entière. */
    *consumed = total;

    uint8_t type = buf[2];
    if (type == LINK_TYPE_MATRIX && payload_len != LINK_PAYLOAD_MATRIX)
        return LINK_DECODE_SKIP;    /* MATRIX sans son bitmap : incohérente */

    out->type = type;
    out->seq  = buf[3];
    if (type == LINK_TYPE_MATRIX)
        memcpy(out->bitmap, &buf[4], RF_HALF_BITMAP_BYTES);
    else
        memset(out->bitmap, 0, RF_HALF_BITMAP_BYTES);

    return LINK_DECODE_FRAME;
}
