/* Relais HID de la souris Conchodytes vers le slot 2 du dongle.
 *
 * Le protocole n'est pas à inventer : `rf_encode_hidreport_mouse()` existe dans
 * rf_packet.h, le dongle le décode déjà (rf_rx_task.c) et appelle
 * `hid_send_mouse()`. À la perte du lien il ne relâche QUE les boutons, jamais
 * la frappe en cours (rf_slot.h). Ce module ne fait qu'émettre.
 *
 * ⚠ Pourquoi un fichier séparé de kbd_relay_tx.c, qui porte pourtant déjà un
 * `kbd_relay_send_mouse()` : celui-là est soudé au clavier — il interroge
 * `kbd_active_route()`, appelle `usb_presence_poll()` et rafraîchit
 * périodiquement le dernier rapport clavier. Une souris n'a rien de tout ça, et
 * son déplacement est RELATIF donc non idempotent : le réémettre en boucle
 * ferait dériver le curseur. Ce qui est réellement commun — le driver radio et
 * l'appairage — est réutilisé, pas recopié.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Initialise la radio en PTX et restaure l'appairage depuis la NVS.
 *
 * ⚠ N'initialise PAS le bus SPI : le capteur l'a déjà fait dans
 * `pmw3389_init()`, qui tourne avant. Le bus est partagé, et deux appels à
 * `spi_bus_initialize` ne cohabitent pas. L'ordre compte donc, et
 * `mouse_task_start()` le garantit.
 *
 * Rend ESP_OK même si la carte n'est pas appairée — l'appairage est une
 * démarche séparée, voir `mouse_relay_pair()`. Rend une erreur seulement si la
 * radio elle-même ne répond pas. */
esp_err_t mouse_relay_init(void);

/* true quand la radio répond ET que l'appairage est chargé. Faux tant que la
 * souris n'a pas été appairée : les rapports sont alors simplement jetés. */
bool mouse_relay_active(void);

/* Émet un rapport HID souris (6 octets, PKT_TYPE_HIDREPORT / RF_HID_SUB_MOUSE).
 *
 * ⚠ x, y et molette sont des `int8_t` : ±127 par rapport. Mesuré au banc le
 * 2026-08-25, le capteur produit jusqu'à 5373 comptes sur 200 ms — soit ~27 par
 * milliseconde. À 8 ms de cadence cela ferait 215 et saturerait franchement ;
 * à 1 ms cela passe. **Le choix de la cadence et celui de l'encodage sont le
 * même choix**, et il n'est pas tranché : voir la spec
 * docs/superpowers/specs/2026-08-25-conchodytes-firmware-design.md §7.
 * L'appelant est responsable de l'écrêtage ou de l'accumulation.
 *
 * ⚠ REND l'acquittement radio (TX_DS), ET L'APPELANT DOIT LE REGARDER. Sans
 * retransmission (voir mouse_relay_init), une trame non acquittée est PERDUE.
 * Comme le déplacement est RELATIF, la jeter revient à effacer ce bout de geste :
 * le curseur parcourt moins que la main. Il ne faut pas pour autant réémettre la
 * trame telle quelle — rejouer du relatif fait avancer deux fois — mais REMETTRE
 * les comptes dans l'accumulateur, pour que la trame suivante porte la somme.
 * C'est juste par construction : une somme de déplacements est un déplacement. */
bool mouse_relay_send(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel);

/* Compteurs d'émission depuis le démarrage : trames envoyées, et parmi elles
 * celles ACQUITTÉES au niveau radio (TX_DS du nRF24).
 *
 * Un acquittement dit que quelqu'un écoute sur cette adresse et ce canal — pas
 * que le dongle a compris la trame. Mais son ABSENCE est sans ambiguïté :
 * personne n'est en face. C'est la différence entre « le dongle ne décode pas »
 * et « la souris parle dans le vide », et sans ce compteur on ne peut pas la
 * faire. `rf_driver_send()` rend déjà l'information ; elle était simplement
 * jetée. */
void mouse_relay_stats(uint32_t *envoyes, uint32_t *acquittes);

/* Lance l'échange d'appairage : émission de PKT_PAIR_REQ sur le rendez-vous
 * (canal 0x28, adresse "KSPR\xFF") en déclarant le slot 0x02 et le type
 * RF_DEV_MOUSE, puis attente du PKT_PAIR_ACK du dongle.
 *
 * La fenêtre d'appairage du dongle doit être OUVERTE — commande CDC
 * KS_CMD_RF_PAIR_START (0xB2). Sans elle le dongle ignore les requêtes.
 *
 * En cas de succès, enregistre set_id / slot / MAC du dongle en NVS et rend
 * ESP_OK ; l'appairage ne devient effectif qu'au redémarrage suivant, comme
 * pour les moitiés du clavier. */
esp_err_t mouse_relay_pair(void);
