/* PMW3389DM-T3QU — capteur optique de la souris Conchodytes.
 *
 * Datasheet PixArt, Version 1.0 | 07 sep 2017. Validé sur carte le 2026-08-25 :
 * Product_ID 0x47, Inverse 0xB8, Revision 0x01, SROM_ID 0xE8 après
 * téléversement, déplacement lu, SQUAL ~80 sur surface.
 *
 * ⚠ CE N'EST PAS UN PMW3360, malgré ce que disent encore certains noms de
 * fichiers du dépôt matériel. Les différences qui comptent pour ce driver :
 *
 *   - blob SROM entièrement différent (99,6 % des octets) ;
 *   - `tSRAD` = 160 µs et `tSWW`/`tSWR` = 180 µs, contre 35 et 120 sur le 3360.
 *     Le code de référence dont ce driver dérive attend 100 µs : hors spec ici,
 *     et c'est le genre d'écart qui passe au banc puis lâche par intermittence ;
 *   - la résolution se règle par `Resolution_L`/`_H` (0x0E/0x0F) sur 16 bits,
 *     pas par un `Config1` 8 bits.
 *
 * Le bus SPI est PARTAGÉ avec le nRF24, qui travaille en mode 0 quand ce
 * capteur veut le mode 3. ESP-IDF reconfigure le mode par appareil, mais
 * l'exclusion reste à notre charge — voir pmw3389.c.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Valeurs d'identité, Table du §5.1, p. 20.
 *
 * ⚠ La datasheet annonce `Inverse_Product_ID` = 0xB9, mais 0x47 ^ 0xFF = 0xB8,
 * et c'est bien 0xB8 que la puce renvoie. Coquille de PixArt : on attend le
 * complément exact, qui est en prime une vérification de bus gratuite —
 * `id ^ inv == 0xFF` échoue sur une ligne coupée comme sur une ligne collée. */
#define PMW3389_PRODUCT_ID          0x47
#define PMW3389_INVERSE_PRODUCT_ID  0xB8

typedef struct {
    int16_t dx, dy;    /* comptes depuis la lecture précédente */
    uint8_t squal;     /* qualité de surface ; ~80 sur un bon tapis */
    uint16_t shutter;  /* obturateur ; au plafond = le capteur ne voit rien */
    bool     motion;   /* bit MOT du burst : faux ⇒ dx/dy forcés à 0, voir le .c */
} pmw3389_motion_t;

/* Initialise le bus (si besoin), remet le port série du capteur à zéro,
 * vérifie l'identité, puis téléverse le SROM.
 *
 * L'ordre compte et n'est pas celui du code de référence : la remise à zéro
 * doit précéder LA TOUTE PREMIÈRE lecture de registre. Constaté sur carte —
 * en lisant `Product_ID` avant, la puce renvoyait 0x11 là où elle vaut 0x47,
 * soit `0x47 >> 2` : deux bits d'horloge de décalage. Les lectures suivantes
 * étaient alignées, ce qui rend le défaut d'autant plus sournois. */
esp_err_t pmw3389_init(void);

/* Règle la résolution en cpi (50 à 16000, arrondi au pas de 50 inférieur).
 * `pmw3389_init()` l'appelle avec BOARD_SNS_CPI. Voir le corps : l'encodage
 * n'est pas documenté dans la datasheet disponible et se valide à l'usage. */
void pmw3389_set_cpi(uint16_t cpi);

/* Lit l'identité sans rien modifier. Utile au diagnostic ; `pmw3389_init` la
 * vérifie déjà et échoue si elle ne correspond pas. */
esp_err_t pmw3389_probe(uint8_t *id, uint8_t *inverse, uint8_t *revision);

/* Relève le déplacement accumulé et remet les compteurs à zéro, en UNE
 * transaction (`Motion_Burst`, 0x50).
 *
 * ⚠ Ne jamais mélanger cet appel avec des lectures de registres de déplacement
 * dans la même boucle : les deux consomment les mêmes compteurs et se volent le
 * mouvement. Constaté au banc le 2026-08-25 — une boucle qui faisait les deux
 * ne voyait que des miettes.
 *
 * Le déplacement est rendu tel que la PUCE le voit. Sur la v1, le capteur est
 * monté à 180°, donc les deux axes sont inversés par rapport à la convention
 * HID (+X à droite, +Y vers le bas). Mesuré : vers la droite -> X négatif,
 * vers l'avant -> Y positif. La correction se fera soit dans le layout v2
 * (rotation de 180°), soit ici — mais PAS aux deux endroits. Voir
 * Conchodytes/NOTES-V2.md §1. */
esp_err_t pmw3389_read_motion(pmw3389_motion_t *out);

/* true quand la broche MOTION est active (basse) : la puce a du déplacement à
 * fournir. C'est ce qui permet à une souris sur batterie de dormir au lieu
 * d'interroger le capteur en boucle. */
bool pmw3389_motion_pending(void);
