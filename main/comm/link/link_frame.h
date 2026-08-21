/* Trame du lien inter-moitiés Niphargus (TRRS, UART1).
 *
 * Logique pure, entièrement en inline : aucune UART, aucun FreeRTOS. La couche
 * transport (B2b) appellera link_encode_matrix() pour émettre et link_decode()
 * sur ce qu'elle a resynchronisé.
 *
 * Format :
 *   [0] SOF 0x4E ('N')
 *   [1] longueur de la charge utile (type + seq + bitmap)
 *   [2] type
 *   [3] seq
 *   [4..8] bitmap 5 octets (format rf_packet, rows 0..3 utilisées)
 *   [9] CRC-8 sur les octets 1..8
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "rf_packet.h"   /* RF_HALF_BITMAP_BYTES — même bitmap qu'en RF */
#include "cdc_binary_protocol.h"   /* ks_crc8() — réutilisé, pas réimplémenté */

#define LINK_SOF          0x4Eu
#define LINK_TYPE_MATRIX  0x01u

/* SOF + len + type + seq + bitmap + crc */
#define LINK_FRAME_MAX  (4 + RF_HALF_BITMAP_BYTES + 1)
#define LINK_PAYLOAD_MATRIX  (2 + RF_HALF_BITMAP_BYTES)   /* type + seq + bitmap */

typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t bitmap[RF_HALF_BITMAP_BYTES];
} link_frame_t;

/* Le lien réutilise ks_crc8() du protocole CDC binaire (cdc_binary_protocol.c)
 * au lieu de réimplémenter un CRC-8 — une seule implémentation à relire dans
 * le dépôt. link_crc8() reste une fonction à part pour préserver l'interface
 * du module et documenter que c'est bien le CRC du dépôt qui est utilisé ici. */
static inline uint8_t link_crc8(const uint8_t *data, uint16_t len)
{
    return ks_crc8(data, len);
}

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
    buf[4 + RF_HALF_BITMAP_BYTES] = link_crc8(&buf[1], 3 + RF_HALF_BITMAP_BYTES);
    return LINK_FRAME_MAX;
}

static inline bool link_decode(const uint8_t *buf, uint16_t len, link_frame_t *out)
{
    if (buf == NULL || out == NULL) return false;
    if (len < 4 + 1) return false;                 /* plus court que le minimum */
    if (buf[0] != LINK_SOF) return false;

    uint8_t payload_len = buf[1];
    if (payload_len != LINK_PAYLOAD_MATRIX) return false;   /* seul type connu */
    uint16_t total = (uint16_t)(2 + payload_len + 1);
    if (len < total) return false;                 /* tronquée */

    if (link_crc8(&buf[1], (uint16_t)(1 + payload_len)) != buf[total - 1])
        return false;

    if (buf[2] != LINK_TYPE_MATRIX) return false;
    out->type = buf[2];
    out->seq  = buf[3];
    memcpy(out->bitmap, &buf[4], RF_HALF_BITMAP_BYTES);
    return true;
}
