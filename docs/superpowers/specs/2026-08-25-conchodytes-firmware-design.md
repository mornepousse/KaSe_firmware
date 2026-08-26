# Firmware Conchodytes — la souris sur le slot 2

**Date** : 2026-08-25
**Matériel** : `~/Documents/GitHub/Conchodytes` — PCB de remplacement pour coque
Logitech M100, ESP32-S3-WROOM-1U + nRF24L01+ + **PMW3389DM-T3QU**.
**Révisée le 2026-08-25 après validation du capteur au banc** — voir §3 et §5.
**Récepteur** : le dongle KaSe, slot 2 (`docs/superpowers/specs/2026-08-19-dongle-role-niphargus-design.md`, §3).

---

## 1. Ce qui existe déjà, et qu'on n'écrit donc pas

Le lien radio de la souris n'est pas à inventer. La moitié réceptrice est écrite,
compilée et tourne :

| Élément | Où | État |
|---|---|---|
| Trame HID souris sur NRF | `main/comm/rf/rf_packet.h` — `rf_encode_hidreport_mouse()` | 6 octets : type, sous-type, boutons, x, y, molette |
| Décodage côté dongle | `main/comm/rf/rf_rx_task.c:176` | `sub == RF_HID_SUB_MOUSE` → `hid_send_mouse()` |
| Slot dédié | `main/comm/rf/rf_slot.h` — `RF_SLOT_MOUSE` | La perte du lien souris ne relâche **que** les boutons, jamais la frappe |
| Descripteur USB | `main/comm/usb/usb_hid.c` | `REPORT_ID_MOUSE` déjà présent |
| Émetteur PTX | `main/comm/rf/kbd_relay_tx.c` | Radio, appairage NVS, `kbd_relay_send_mouse()` |

**Le firmware souris est un émetteur à écrire, pas un protocole à inventer.**

Deux trous seulement dans ce contrat :

- `rf_pairing.h` ne connaît que `RF_DEV_DUMB_HALF` (0) et `RF_DEV_SMART_KBD` (1) —
  il manque un type d'appareil pour la souris.
- La trame porte x, y et molette en `int8_t`, soit **±127 par rapport**. Mesuré
  au banc le 2026-08-25 : jusqu'à **5373 comptes sur 200 ms**. La saturation
  n'est pas une hypothèse. Voir §7.

## 2. Où le firmware vit

Un board `conchodytes` dans ce dépôt, septième de la série, à côté de
`kase_dongle`, `niphar_left` et `niphar_right`.

```sh
idf.py -B build_conchodytes -DBOARD=conchodytes -DSDKCONFIG=build_conchodytes/sdkconfig build
```

La raison de ne pas en faire un dépôt séparé est le contrat du §1 : le protocole
du slot 2 est implémenté à deux, ici et dans le dongle. Deux dépôts, c'est deux
définitions d'une même trame qui dérivent en silence.

### Un nouveau rôle ✅ **fait le 2026-08-25**

`KASE_DEVICE_ROLE_MOUSE` s'ajoute au `choice KASE_DEVICE_ROLE` de
`main/Kconfig.projbuild`. Il n'embarque ni matrice, ni keymap, ni écran, ni BLE,
ni moteur d'entrée.

**Un symbole dérivé plutôt qu'une liste de rôles répétée.** Les gardes qui
excluent le code clavier s'écrivaient `!CONFIG_KASE_DEVICE_ROLE_DONGLE`, et
elles sont dispersées dans **sept fichiers** — `main.c`, `matrix_scan.h`,
`keymap.h`, `key_stats.h`, `cdc_binary_cmds.c` et deux endroits du
`CMakeLists.txt`. Y ajouter `&& !MOUSE` sept fois aurait été à refaire au
prochain rôle sans clavier. Elles passent donc toutes par
**`CONFIG_KASE_NO_KEYMAP_ENGINE`**, vrai pour le dongle et pour la souris.

⚠ **Ce symbole est négatif à dessein, et il faut résister à l'envie de le
retourner.** Un booléen Kconfig à `n` est *absent* de `sdkconfig.h`, exactement
comme sur le harnais de test hôte où aucun `CONFIG_*` n'existe : le
préprocesseur ne peut pas distinguer les deux cas. Avec la forme négative,
`#if !CONFIG_KASE_NO_KEYMAP_ENGINE` vaut vrai quand le symbole est absent —
donc sur l'hôte **et** sur toute cible à clavier, ce qui est le bon défaut dans
les deux cas. La forme positive a été essayée le 2026-08-25 : l'hôte a aussitôt
cessé de compiler `input/key_processor.c`, faute de voir `keymaps`.

Le raisonnement du commentaire d'origine vaut mot pour mot : exclure le moteur
keymap à la compilation rend structurellement impossible qu'il existe deux fois
dans le système.

**Bouchons.** La souris réutilise `comm/cdc/cdc_niphar_slave_stubs.c` — mêmes
trous, pas d'écran, pas de BLE, pas de moteur keymap — plus un
`comm/cdc/cdc_mouse_stubs.c` pour `key_stats_total` et `wpm_get`, que l'esclave
Niphargus définit pour de vrai et qui donneraient donc un doublon au lien s'ils
allaient dans le fichier partagé.

### Ce qui a été écrit

| Fichier | Rôle |
|---|---|
| `boards/conchodytes/board.h` | contrat de brochage |
| `test/test_conchodytes_pins.c` | le verrouille — dont l'appariement NO+NC |
| `main/periph/pmw3389.{c,h}` | driver, porté du bring-up validé au banc |
| `main/periph/pmw3389_srom.c` | blob SROM 4094 octets, symboles préfixés |
| `main/input/mouse_buttons.{c,h}` | décodage SPDT — **pur, testé sur l'hôte** |
| `main/input/mouse_wheel.{c,h}` | quadrature — **pure, testée sur l'hôte** |
| `test/test_mouse_input.c` | rebond, transitions impossibles, aller-retour |
| `main/app/mouse_task.{c,h}` | scrutin 1 kHz, assemble le tout |
| `main/comm/cdc/cdc_mouse_stubs.c` | les deux symboles propres au rôle |
| `sdkconfig.defaults.conchodytes` | rôle, console UART0, BLE coupé |

Les deux modules d'entrée sont **volontairement purs** — des niveaux en entrée,
un état en sortie, aucune dépendance ESP-IDF. C'est ce qui rend testable sur
l'hôte la fenêtre de rebond, que le banc ne produit pas à la demande.

### Le brochage, relevé à la netlist

Extrait de `hardware/pcb/` au `kicad-cli`, **pas** repris du README — qui inverse
gauche et droite sur les contacts NC (voir §8, D1).

| Signal | Broche module | GPIO | | Signal | Broche | GPIO |
|---|---|---|---|---|---|---|
| `SCK_L` | 31 | 38 | | `SW_LEFT` | 18 | 10 |
| `MISO_L` | 32 | 39 | | `SW_RIGHT` | 19 | 11 |
| `MOSI_L` | 33 | 40 | | `SW_MID` | 20 | 12 |
| `CSN_1` (nRF) | 38 | 2 | | `SW_LEFT_NC` | 5 | **5** |
| `CE_1` (nRF) | 39 | 1 | | `SW_RIGHT_NC` | 4 | **4** |
| `IRQ_1` (nRF) | 34 | 41 | | `SW_MID_NC` | 6 | 6 |
| `SNS_NCS` | 10 | 17 | | `VBAT_SENSE` | 21 | 13 (ADC2_CH2) |
| `SNS_MOTION` | 11 | 18 | | `D-` / `D+` | 13 / 14 | 19 / 20 |
| `ENC_A` / `ENC_B` | 7 / 17 | 7 / 9 | | J1 `TX`/`RX`/`IO0` | 37/36/27 | 43/44/0 |

Toutes les lignes SPI et de contrôle portent une résistance série de 100 Ω
(R16/R18/R30, R36/R39/R41). À 2 MHz et ~20 pF de parasites, la constante vaut
2 ns : sans effet.

## 3. Le driver PMW3389 — un port, pas une réécriture

> **Validé au banc le 2026-08-25.** Ce qui suit n'est plus un plan : le driver
> tourne, dans `Conchodytes/bringup/`. Identité lue, SROM téléversé, déplacement
> mesuré. Cette section décrit ce qui a marché, et ce qu'il reste à porter dans
> ce dépôt.

**⚠️ La puce est un PMW3389, pas un PMW3360.** `Product_ID = 0x47` lu sur carte,
là où le 3360 vaut 0x42. C'est voulu ; c'est le dépôt matériel qui n'avait pas
suivi. Détail et conséquences : `Conchodytes/NOTES-V2.md` §0.

Le POC `mornepousse/Mase` (`code/src/pmw3360.c`, 195 lignes, dérivé de
`mrjohnk/PMW3360DM-T2QU`) contient un driver complet et éprouvé : carte des
registres, `read_register`/`write_register` avec leurs temporisations,
téléversement SROM, séquence de démarrage. Il est écrit contre le **SDK Pico**.

Ce qui se reprend : la carte des registres et la séquence de démarrage.
Ce qui se réécrit : la couche SPI.
**Ce qui ne se reprend PAS : le blob SROM.** Celui du POC est le firmware du
3360. Le 3389 a le sien — 4094 octets également, mais **99,6 % des octets
diffèrent**. Source retenue : `mrjohnk/PMW3389DM`,
`Arduino Examples/PMW3389DM-polling/SROM.ino`, même auteur que le driver d'amont.
Téléverser l'un dans l'autre revient à charger le firmware d'une autre puce.

Destination : `main/periph/pmw3389.{c,h}` et `main/periph/pmw3389_srom.c`.

### La couche SPI

Calquée sur `rf_driver.c:199-206`, qui fait déjà le bon geste — `spi_master`
ESP-IDF avec `spics_io_num = -1`, CSN piloté à la main. Deux différences :

- **mode 3** (CPOL=1, CPHA=1) au lieu du mode 0 du nRF24 ;
- **CS manuel obligatoire**, et ce n'est pas un choix de style : `tSRAD` tombe
  *entre* l'octet d'adresse et l'octet de donnée. Le CS matériel d'ESP-IDF le
  relèverait au milieu de l'attente.

### Trois écarts par rapport au POC — dont deux sont des corrections de bugs

**Horloge à 2 MHz** au lieu de 500 kHz. C'est le maximum de la datasheet
(`fSCLK`, Table 4, p. 15) et le POC se privait d'un facteur quatre sans raison.
Validé au banc.

**Les temporisations, qui étaient fausses.** Le 3389 exige `tSRAD` = **160 µs**
et `tSWW`/`tSWR` = **180 µs** (Table 5, p. 16), contre 35 et 120 sur le 3360. Le
code de référence attend 100 µs dans les deux cas — hors spécification sur cette
puce. Ça passait au banc, ce qui est le pire des cas : un écart qui ne se voit
qu'en défaillance intermittente plus tard.

**L'ordre de démarrage, qui était faux aussi.** La remise à zéro du port série
(triple battement de NCS) puis `Power_Up_Reset` doivent précéder **la toute
première lecture de registre**. Constaté sur carte : en lisant `Product_ID`
avant cette séquence, la puce renvoyait `0x11` là où elle vaut `0x47` — soit
`0x47 >> 2`, un décalage de deux bits d'horloge. Les lectures suivantes étaient
alignées, ce qui rend le défaut d'autant plus sournois.

**`Motion_Burst` (0x50)** au lieu de quatre lectures de registres séparées : à
faire, ni le bring-up ni le driver porté ne l'utilisent encore.

⚠ **La datasheet du 3389 ne documente pas la charge utile du burst.** Ses
20 pages n'en donnent que les contraintes de temps — `tSRAD_MOTBR` = 35 µs et
`tBEXIT` = 500 ns, Table 5, p. 16. La disposition des octets circule dans les
implémentations communautaires, mais aucune source citable ici.

Méthode retenue pour la combler sans deviner : **lire le burst et comparer ses
octets, un à un, aux mêmes registres lus individuellement sur une scène
immobile.** Si l'octet *n* du burst vaut toujours `SQUAL`, `Delta_X_L`, etc.,
la correspondance est établie par la mesure et non par un article de blog. À
faire au banc avant d'écrire le décodage. Sur une souris sans fil tenue à une
cadence de rapport, quatre allers-retours à 20 µs de garde chacun se paient —
et sur le 3389 c'est pire, chaque lecture normale coûtant 160 µs de `tSRAD`
contre 35 µs en mode burst.

**La résolution ne se règle pas comme sur le 3360.** Le 3389 utilise
`Resolution_L`/`Resolution_H` (0x0E/0x0F) sur 16 bits, pas un `Config1` 8 bits.
Le code de référence du 3389 appelle encore 0x0F « Config1 » et y écrit 0x15 :
c'est un copier-coller du 3360 et il écrit dans `Resolution_H`. Le bring-up n'y
touche pas et laisse les valeurs de reset.

### Un ajout que le POC n'avait pas

**`SNS_MOTION` (GPIO18)** — la sortie d'interruption de mouvement du capteur.
Elle existe sur cette carte et c'est ce qui permet à une souris sur batterie de
dormir au lieu d'interroger le capteur en boucle. Le POC était filaire ; la
Conchodytes ne l'est pas.

### API

```c
esp_err_t pmw3389_init(void);                      /* bus, reset, SROM */
esp_err_t pmw3389_probe(uint8_t *id, uint8_t *inv);/* 0x47 / 0xB8 */
esp_err_t pmw3389_motion_burst(pmw3389_motion_t *out);
esp_err_t pmw3389_set_cpi(uint16_t cpi);           /* Resolution_L/H, 16 bits */
```

### Valeurs de référence, datasheet PMW3389DM-T3QU, Version 1.0 | 07 sep 2017

| Grandeur | Valeur | Page | Lu sur carte |
|---|---|---|---|
| `Product_ID` (0x00) | **0x47** | 20 | ✅ 0x47 |
| `Inverse_Product_ID` (0x3F) | 0xB9 *annoncé* | 20 | ✅ **0xB8** |
| `Revision_ID` (0x01) | 0x01 | 20 | ✅ 0x01 |
| `Config2` défaut | 0x20 | 20 | |
| `fSCLK` max | 2,0 MHz | 15 | ✅ tourne à 2 MHz |
| `tSRAD` (lecture normale) | **160 µs** | 16 | |
| `tSRAD_MOTBR` (burst) | 35 µs | 16 | |
| `tSWW` / `tSWR` | **180 µs** | 16 | |
| VDD min/typ/max | 1,80 / 1,90 / **2,10** V | 15 | ✅ rail à 2,01 V |
| VDD **maximum absolu** | **2,10 V** | 15 (Table 3) | |

⚠️ **La datasheet se trompe sur `Inverse_Product_ID`.** Elle annonce 0xB9, mais
`0x47 ^ 0xFF = 0xB8`, et c'est 0xB8 que la carte renvoie. Attendre le complément
exact, pas la valeur imprimée.

`Product_ID ^ Inverse_Product_ID == 0xFF` est une vérification de bus gratuite :
elle échoue sur une ligne coupée comme sur une ligne collée. C'est ce test qui a
permis d'affirmer que 0x47 était une vraie lecture et non du bruit.

Note sur le POC : son commentaire « *Write 0 to Rest_En bit of Config2 to disable
Rest mode* » écrit en réalité 0x20, qui est la valeur par défaut du registre. Le
commentaire ment, l'écriture est inoffensive — ne pas recopier le commentaire.

## 4. Le bus partagé — le vrai piège, absent du POC

Le POC avait un bus pour lui seul. Ici le PMW3360 et le nRF24 partagent
`SCK_L`/`MOSI_L`/`MISO_L`, et n'ont pas le même mode SPI. ESP-IDF sait
reconfigurer le mode par appareil ; ce qu'il ne sait pas faire à notre place,
c'est l'exclusion. Trois règles :

1. **Les deux CS tirés haut au démarrage**, avant toute initialisation de l'un ou
   de l'autre. Un CS flottant pendant que l'autre appareil parle, c'est deux
   esclaves sur le bus.
2. **Un seul CS bas à la fois**, jamais de recouvrement.
3. **`spi_device_acquire_bus()` autour du burst SROM.** Le blob fait
   **4094 octets** (`firmware_length` du POC), à 15 µs l'octet : **61 ms** pendant
   lesquelles la radio ne doit pas s'intercaler — une trame nRF au milieu corrompt le SROM,
   et le capteur démarre alors sur un firmware invalide sans forcément le dire.

## 5. Jalons

### Jalon 1 — le capteur répond ✅ **atteint le 2026-08-25**

`Product_ID = 0x47`, `Inverse = 0xB8`, `Revision = 0x01`, puis `SROM_ID = 0xE8`
après téléversement.

### Jalon 2 — le capteur voit ✅ **atteint le 2026-08-25**

| | avant pontage de D100 | après |
|---|---|---|
| `Shutter` | **4500** (plafond) | **~50** |
| `SQUAL` | 3-5 | **48-81** |
| `dx` / `dy` | 0 en permanence | jusqu'à ±5000 par fenêtre de 200 ms |
| `MOTION` (GPIO18) | jamais actif | bascule à 0 sur mouvement |

Les deux jalons ont été atteints avec un firmware de banc autonome
(`Conchodytes/bringup/`, projet ESP-IDF minimal), **pas** avec le board
`conchodytes` de ce dépôt, qui reste à créer. C'est l'objet du §2, désormais le
seul travail logiciel restant sur le capteur.

### Jalon 3 — la souris parle au dongle

Clics NO/NC, encodeur en quadrature, `rf_encode_hidreport_mouse` vers le slot 2,
appairage. Hors périmètre de cette spec (§9).

### État de la construction au 2026-08-25

`idf.py -B build_conchodytes -DBOARD=conchodytes build` passe : **294 Ko**, 86 %
de la partition applicative libre. `conchodytes` est ajouté aux variantes de
`scripts/check.sh`, qui en compte donc sept.

La tâche ne produit encore **aucun rapport HID** — c'est le jalon 3.

### Ce qui se teste sans rien attendre

Le build, le démarrage, l'UART sur J1, et `rf_line_test` (`main/comm/rf/rf_line_test.c`)
sur le nRF24 — qui est alimenté en 3,3 V, donc sain. Il prouve
`SCK`/`MOSI`/`MISO`/`CSN`/`CE`/`IRQ` sur du silicium qui n'a pas été maltraité, et
**innocente le bus avant que le capteur y arrive**. Sans ça, le premier test du
capteur mêle trois inconnues : le build, le bus, et l'état de la puce.

## 6. Prérequis matériels — **tous levés le 2026-08-25**

La carte v1 était montée sans aucune rework. Les trois points ci-dessous ont été
corrigés au fer et vérifiés. Ils restent documentés parce que **la v2 doit les
intégrer au layout** — pour l'instant ce sont des fils volants.

### 6.1 Déposer U100 ✅ — c'était un bloquant absolu

`U100` porte un symbole au brochage HT7833 (1=GND, 2=VI, 3=VO) sur une empreinte
**SOT-23**, boîtier où ce brochage n'existe pas. Le PCB câble pad 2 → +3,3 V et
pad 3 → rail capteur. Or le XC6206 en SOT-23 a VIN en 3 et VOUT en 2 : le
régulateur est monté à l'envers.

Le pass PMOS a sa diode de substrat orientée drain→source, soit VOUT→VIN. Avec
3,3 V forcé sur VOUT, le rail capteur monte à ≈ 2,6-2,7 V — au-dessus des 2,10 V
de la plage d'emploi (p. 14). Les notes de banc du 25/08 confirment que le
HT7833 sort bien son 3,3 V : **c'est l'état actuel de la carte.**

**Correction du 2026-08-25 sur ce point.** Une version antérieure de cette spec
disait que 2,10 V n'était que le haut de la plage d'emploi et que la datasheet ne
donnait pas de maximum absolu. C'est vrai du 3360 ; **c'est faux du 3389**, dont
la *Table 3, Absolute Maximum Ratings*, p. 15 donne `VDD −0,5 … 2,10 V`. La puce
tournait donc au-dessus de son maximum absolu.

**Mesuré : 2,72 V** avant correction — la déduction par la diode de substrat
prévoyait ~2,7 V. **Résolu** en déposant `U100` et en le resoudant pins 2 et 3
croisées en l'air : **2,01 V mesuré**, et le capteur fonctionne.

Il n'existe **aucun composant de remplacement** qui corrigerait ça : le brochage
que la carte câble (`1=GND, 2=VIN, 3=VOUT`) est la convention SOT-89, qu'aucun
LDO SOT-23-3 n'adopte. La seule issue est de croiser les deux pistes — gratuit
en v2.

Détail complet : `Conchodytes/NOTE-LDO-PMW3360.md`.

### 6.2 Le rail capteur ✅

Rendu inutile par 6.1 : le `U100` recâblé fait le travail et sort **2,01 V**, ce
qui confirme au passage que le composant monté est bien un XC6206**P202**.
`VDDIO` reste sur le 3,3 V de la carte, ce qui est correct (Table 4, p. 15 :
VDDIO 1,80-3,60 V, avec VDDIO ≥ VDD).

### 6.3 Ponter D100 et corriger R101 ✅ — c'était toute la cause de l'aveuglement

`D100` est une LED IR externe qui n'a pas lieu d'être : le PMW3389 embarque la
sienne (p. 4), et la broche 15 (`LED_P`) en est l'anode (Table 1, p. 5). Pire,
elle est routée cathode vers le +3,3 V — elle **bloque** le courant, et le capteur
est aveugle.

**Appliqué** : `D100` pontée, `R101` remplacée par **22 + 39 = 61 Ω** depuis le
3,3 V, soit ≈ 30,7 mA. Le `Shutter` est passé de 4500 à ~50 — un facteur 88 de
lumière en plus.

⚠️ **Pas 150 Ω.** Cette valeur, que portait une version antérieure de cette spec
et que porte encore l'historique de `NOTES-V2.md`, visait les 12 mA du **3360**.
Le 3389 en veut ~36 mA (référence : 13 Ω depuis 1,9 V, Figure 10, p. 14) ; 150 Ω
le sous-alimente d'un facteur 3.

Sans cette correction le capteur **répond quand même en SPI** : c'est ce qui a
permis de séparer les jalons 1 et 2, et de savoir que le bus était bon avant de
soupçonner l'optique.

Détail : `Conchodytes/NOTES-V2.md`, point 6.

### 6.4 Le lien de développement ✅

L'USB natif du S3 est bien câblé (`D+`/`D-` sur GPIO19/20) mais la carte ne
s'énumère pas. Le développement passe par le **FT2232 sur `/dev/ttyUSB2`**
(canal B), relié à J1 — le 2×3 qui porte `EN`, `3V3`, `TX`, `RX`, `GND`, `IO0`.
Le reset automatique par DTR/RTS fonctionne, `esptool` et `idf.py flash`
passent sans manipulation. MAC de la carte : `ac:a7:04:18:82:3c`.

Diagnostiquer l'USB natif reste un travail à part, hors de cette spec.

## 7. Question ouverte — la saturation à ±127

`rf_encode_hidreport_mouse` porte x, y et molette en `int8_t`.

**Chiffres mesurés au banc le 2026-08-25**, à la résolution de reset, mouvement
à la main : jusqu'à **`dx = −5373` sur une fenêtre de 200 ms**, soit ~27 comptes
par milliseconde en moyenne — et les pointes instantanées sont au-dessus.

Conséquence directe : à une cadence de rapport de **8 ms, cela fait 215 comptes**
et l'`int8_t` sature franchement. À **1 ms, cela fait 27** et ça passe. **Le
choix de la cadence et le choix de l'encodage sont donc le même choix.**

Trois issues, toujours non tranchées — la mesure dit que le problème est réel,
elle ne dit pas lequel des trois compromis on veut :

- **écrêter** et accepter la perte sur les mouvements vifs ;
- **accumuler** le reste et l'étaler sur les rapports suivants — la souris
  « glisse » après un geste rapide ;
- **étendre la trame** à `int16_t`, ce qui la fait passer de 6 à 8 octets et
  **change le contrat du §1 des deux côtés**, dongle compris.

À trancher au jalon 3, avec des chiffres.

## 8. Défauts de documentation relevés

**D1 — le README de Conchodytes inversait gauche et droite sur les contacts NC.**
✅ **Corrigé le 2026-08-25.** Il annonçait `SW_LEFT_NC` = GPIO4 et
`SW_RIGHT_NC` = GPIO5 ; la netlist dit l'inverse — GPIO4 va à `SW2.3` (clic
droit, R109) et GPIO5 à `SW1.3` (clic gauche, R108). Comme l'anti-rebond repose
sur l'appariement NO+NC du *même* bouton, écrire le firmware sur ce tableau
aurait croisé les deux clics.

**D2 — tout le dépôt matériel décrivait un PMW3360.** ✅ **Corrigé le
2026-08-25** dans `README.md`, `NOTES-V2.md` (nouveau §0) et
`NOTE-LDO-PMW3360.md`. **Restent à reprendre** : la valeur du symbole KiCad `U2`,
les noms de fichiers `PMW3360.lib` / `PMW3360.pretty` / `NOTE-LDO-PMW3360.md`
(l'empreinte 16 broches est identique, seul le nom ment), le BOM et la commande
LCSC.

**D3 — `SQUAL` fluctue au repos**, de 12 à 73 sans que rien ne bouge, carte nue
sur l'établi. La Table 4 (p. 15) veut 2,2 à 2,6 mm entre le plan de référence de
la lentille et la surface : hors coque, la carte n'y est pas. À revérifier une
fois dans la M100 — si ça persiste, c'est la lentille ou sa hauteur. Sans effet
sur le firmware, mais à ne pas confondre plus tard avec un défaut de driver.

## 9. Hors périmètre

Les clics, l'encodeur de molette, la mesure batterie, la gestion d'énergie et
l'émission radio vers le slot 2 — c'est le jalon 3, et il aura sa propre spec.
La correction d'axe du capteur (`NOTES-V2.md` point 1, rotation de 180°) ne se
tranche qu'une fois qu'on aura vu du déplacement. Le diagnostic de l'USB natif
et la rework matérielle elle-même ne sont pas du logiciel.
