# Firmware Niphargus — design

**Date** : 2026-08-19
**Périmètre** : le firmware des deux moitiés du clavier split Niphargus, développé
dans ce dépôt (KeSp_firmware), aux côtés des claviers monoblocs V1/V2/V2D et du
dongle.

## 1. Contexte

Le clavier split change de matériel et de concept. Les anciennes moitiés KaSe
(`kase_half_left` / `kase_half_right`, e-ink SSD1681 sur SPI2 partagé, canal info
ESP-NOW) ont été retirées de ce dépôt au commit `c107df77` : elles visaient un
matériel qui n'existe plus.

Le nouveau clavier est le **Niphargus**, dessiné dans le dépôt `rili`
(`~/Documents/GitHub/rili`, KiCad). Son firmware vit **ici**, pas dans rili — le
`CLAUDE.md` de rili annonçait l'inverse et doit être corrigé.

Documents de référence :

- `docs/NIPHARGUS_V2_HARDWARE.md` (ce dépôt) — contrat matériel, tables de pins
  vérifiées à la netlist le 2026-08-06. **Source de vérité du brochage.**
- `rili/docs/superpowers/specs/2026-07-29-rouge-gorge-refonte-design.md` — design
  matériel : alimentation, ESD, mécanique, modes.

Matériel : deux moitiés ESP32-S3-WROOM-1-N16R8, une nRF24L01+ chacune, cellule
16340, matrice 4×7 (26 touches). Trackpad Azoteq TPS43 à gauche, écran Sharp
LS011B7DH03 à droite. Lien filaire TRRS (UART1 + 5 V commuté). **Pas de WiFi ni
de BLE** — le budget d'alimentation l'interdit (régulateur HT7833, pics radio
hors budget), donc la configuration et les mises à jour passent par USB.

**Les cartes ne sont pas encore arrivées** (commandées le 2026-08-07). Aucune
validation matérielle n'est possible aujourd'hui.

> **Périmé — mise à jour du 2026-09-01.** La moitié GAUCHE est arrivée et a été
> mise sous tension pour la première fois. Acquis, vérifiés au banc :
>
> - Boot complet sur flash, console applicative sur le connecteur de prog
>   (GPIO43/44), `Boot count: 1`, ni boucle ni safe mode.
> - Table de brochage matrice conforme au contrat, colonne par colonne.
> - **nRF24 fonctionnelle** : `rf_driver_probe()` répond
>   `CONFIG=0x3f RF_SETUP=0x06 -> OK`, puis init PRX complète
>   (`ch=76 addr=KaSe.01`). Les valeurs relues étant celles écrites au boot
>   précédent, les deux sens du bus SPI sont prouvés.
> - Le module posé est un **N16R8**, pas le N8R2 de la ligne ci-dessus.
>
> Restent non validés : diodes de matrice non soudées (aucun appui ne peut
> remonter), USB natif non branché, trackpad sans driver, moitié droite
> inexistante — donc le spike B4 de §5 n'est levé qu'à moitié.

## 2. Décisions d'architecture

### 2.1 Un seul moteur keymap, dans la moitié gauche

La moitié **gauche est le maître en toutes circonstances** : elle porte le moteur
de résolution des keycodes (couches, tap-hold, combos, macros) et elle est la
seule à produire un rapport HID. La droite est un scanner qui remonte sa matrice
brute.

*Pourquoi la gauche* : le trackpad TPS43 y est câblé. En faisant de la gauche le
maître, les données du trackpad ne traversent jamais un lien radio ni le lien
filaire — elles vont du bus I2C au moteur, dans le même MCU.

*Alternative écartée* : un moteur dans le dongle pour le sans-fil et un moteur
dans la moitié pour le filaire, comme le laisse entendre la spec rili. Deux
moteurs à maintenir en parallèle, deux keymaps à provisionner, deux fois les
mêmes bugs de tap-hold à corriger.

### 2.2 En filaire, seule la gauche présente un clavier

Brancher la moitié **gauche** : la droite lui envoie sa matrice par UART1, la
gauche sort le HID sur son USB. Clavier complet.

Brancher la moitié **droite** : **charge uniquement**, aucune énumération HID. Le
chemin clavier reste la radio.

*Alternative écartée* : faire de la droite branchée un tuyau USB bête (matrice
vers la gauche par UART, HID calculé à gauche et renvoyé par UART, émis par la
droite). Élégant sur le papier, mais il double les chemins à tester pour un cas
d'usage que l'utilisatrice contrôle — il suffit de brancher du bon côté.

### 2.3 La matrice de l'esclave passe en direct, sans le dongle

La moitié droite émet **directement vers la gauche**. La gauche reste en
réception (PRX) par défaut et bascule en émission (PTX) vers le dongle seulement
quand elle a un rapport à envoyer.

*Pourquoi pas via le dongle* : le dongle a deux radios, et la seconde appartient
à la **souris Conchodytes** (`~/Documents/GitHub/Conchodytes`, ESP32-S3 +
PMW3360, même récepteur). Le clavier ne dispose donc que d'un slot. La ligne §9
de la spec rili — « Rouge-Gorge occupe les 2 slots d'un dongle » — est périmée et
doit être corrigée là-bas.

*Ce qui rend la bascule viable* : les émissions sont événementielles, pas
périodiques. La gauche n'émet que sur changement de matrice ; elle écoute le
reste du temps. Les collisions (les deux moitiés changeant au même instant) sont
absorbées par les retransmissions ESB de la droite (ARC 15, ARD 500 µs).

**Ce pari n'est pas prouvé et ne peut pas l'être sans matériel.** Voir §6.

## 3. Rôles et flux

| | GAUCHE (U6) — maître | DROITE (U5) — esclave |
|---|---|---|
| Matrice | 4×7, 26 touches | 4×7, 26 touches |
| Périphérique | trackpad TPS43 (I2C + RDY) | écran Sharp LS011B7DH03 |
| Moteur keymap | oui, le seul | non |
| USB | HID + CDC config, hub CH334R | charge seulement |
| Radio | PRX par défaut, PTX vers dongle sur événement | PTX vers la gauche |
| Lien TRRS | UART1, reçoit la matrice | UART1, émet la matrice |

```
SANS FIL
  droite ──PKT_KEY (RF)──▶ gauche │ keymap
                                   ▼ HID final (RF)
                          dongle radio1 ──USB HID──▶ hôte
                          dongle radio2 ◀── souris Conchodytes

FILAIRE (gauche branchée)
  droite ──matrice (UART1)──▶ gauche │ keymap ──USB HID──▶ hôte
         ◀──5 V (poignée de main)────┘
```

## 4. Découpage en bricks

Chaque brick est indépendamment livrable et se termine sur `./scripts/check.sh`
vert.

**B0 — Boards et brochage.** `boards/niphar_left/`, `boards/niphar_right/` :
`board.h`, `board_keymap.c`, `board_layout.c`. Rôles Kconfig
(`KASE_DEVICE_ROLE_NIPHAR_MASTER` / `_SLAVE`). `scripts/check.sh`,
`scripts/perf-size.sh` et `.esp-dev.yml` repassent à 6 boards.

Les deux tables de pins du contrat matériel **diffèrent** (permutations de
routage) et une inversion ne casserait aucun build. B0 embarque donc un test host
qui encode le contrat et vérifie `board.h` valeur par valeur, sur le modèle de
`test/test_board_config.c`. C'est le seul garde-fou disponible avant le matériel.

**B1 — Demi-clavier filaire.** Scan 4×7 sur la gauche → moteur existant → USB
HID. Réutilise `key_processor`, `keymap`, `key_features`, `tap_hold`,
`tap_dance`, `combo`, `leader`, `hid_report`, `hid_transport` sans modification.
Premier jalon où l'on tape réellement.

**B2 — Lien TRRS.** UART1 (GPIO17/18) avec le **swap TX/RX obligatoire d'un
côté** (câble droit : TX arrive sur TX), trame matrice, machine d'état de la
poignée de main 5 V sur `LINK_5V_EN` (GPIO21). Le framing et la machine d'état
sont de la logique pure, testés en premier. Clavier complet en filaire.

**B3 — Radio esclave → maître.** Droite en PTX, gauche en PRX. Réutilise
`rf_driver`, `rf_packet`, `rf_pairing`, `heartbeat` — protocole KaSe intact
(ESB 1 Mbps, CRC 16, adresses 5 octets, DPL). La jauge batterie remonte dans le
champ `batt_dV` du heartbeat, déjà prévu au protocole.

**B4 — Radio maître → dongle.** Bascule PRX/PTX sur la radio unique de la
gauche. Brick à risque (§6).

**B5 — Trackpad.** Driver IQS572 en I2C (GPIO47/48) avec handshake RDY (GPIO42),
branché sur `trackpad_map` / `trackpad_cfg`, conservés au rip-out. Le NRST est un
RC matériel, pas un GPIO.

**B6 — Écran.** Sharp LS011B7DH03 sur le SPI partagé avec la nRF24 (SCK/MOSI
38/40, CS GPIO14 **actif haut**, LSB-first, write-only), plus un canal d'état
gauche → droite pour la couche courante et les niveaux de batterie.

**B7 — Énergie.** Jauge sur ADC2_CH2 (GPIO13, diviseur 1M/1M — ADC2 est
utilisable parce qu'il n'y a pas de WiFi). Watchdog et reprise brownout en
≤ 200 ms, exigence non négociable du contrat matériel. Sommeil profond avec
réveil EXT1 sur les rows, scan en domaine RTC, cible < 50 µA par moitié.
Brick à risque (§6).

**B8 — Config USB.** Commandes KS/KR sur le rôle maître : keymap, macros,
layout, tap-dance, combos. L'essentiel du code existe (`cdc_binary_cmds.c`) ;
c'est du câblage et du provisionnement.

## 5. Ordre des travaux

**Phase 1 — sans les cartes.** Uniquement ce qu'un oracle mécanique valide :
compilation et tests host.

1. B0 — boards et brochage (avec son test de contrat)
2. B2a — protocole du lien : framing et poignée de main, en logique pure
3. B1a — constantes matrice 4×7, keymap par défaut, layout JSON
4. B8 — câblage de la config USB

**Phase 2 — au déballage des cartes.**

5. B1b — bring-up du scan et du HID sur la gauche
6. B4 — **spike d'abord** : la bascule PRX/PTX tient-elle ? Le résultat commande
   B3 et toute la répartition. À lever avant d'écrire B3 pour de bon.
7. B3 — lien esclave → maître
8. B5, B6 — trackpad et écran
9. B7 — énergie et sommeil

## 6. Risques et plans de repli

**R1 — La bascule PRX/PTX de la radio gauche (B4).** La gauche est sourde
pendant qu'elle émet. Si les pertes de paquets de la droite sont trop nombreuses
en frappe rapide, la répartition choisie ne tient pas.

*Repli documenté* : partager le slot 2 du dongle entre la souris et la moitié
droite, en multiceiver (le nRF24 en réception gère 6 pipes sur une radio, à
adresses distinctes). La gauche récupère alors la matrice droite dans la charge
utile de l'ACK de ses propres émissions et n'est plus jamais sourde. Prix :
souris et moitié droite partagent un canal RF et se disputent le temps d'antenne.
Cette option a été écartée au design mais reste techniquement valide.

*Repli de dernier recours* : remettre le moteur keymap dans le dongle pour le
sans-fil, en acceptant deux moteurs.

**R2 — Budget d'énergie (B7).** La cible < 50 µA par moitié suppose un scan en
domaine RTC et la radio en power-down. Rien ne permet de la vérifier avant le
matériel. Un dépassement ne casse pas l'architecture, il coûte de l'autonomie.

**R3 — Erreur de brochage.** Les deux tables de pins diffèrent et le compilateur
n'en sait rien. Mitigé par le test de contrat de B0, qui est la seule barrière
avant le banc.

**R4 — Swap TX/RX du lien TRRS.** Le contrat matériel l'appelle exigence non
négociable : ne jamais piloter les deux TX sans le swap. Une erreur ici met deux
sorties en conflit sur le même fil. À traiter comme une contrainte de code
explicite, pas comme un détail d'initialisation.

## 7. Ce que l'on réutilise de KaSe

Le moteur d'entrée en entier (`key_processor`, `keymap`, `key_features`,
`tap_hold`, `tap_dance`, `combo`, `leader`, `hid_report`, `hid_transport`), la
pile RF (`rf_driver`, `rf_packet`, `rf_pairing`, `heartbeat`), le mapping
trackpad côté maître (`trackpad_map`, `trackpad_cfg`), le protocole CDC binaire
KS/KR en entier, le safe boot et le pipeline anti-régression.

Le dongle reste inchangé : il est le récepteur côté hôte, radio 1 pour le
clavier, radio 2 pour la souris.

## 8. Tests

La norme TDD du dépôt s'applique : toute logique pure nouvelle (framing du lien,
machine d'état de la poignée de main, décodage de la matrice, tables de
brochage) a son test host écrit d'abord, ajouté à `test/CMakeLists.txt` et
déclaré dans `test/test_main.c`. Un test doit prouver qu'il mord avant d'être
gardé.

Sans matériel, le test host et la compilation sont les **seuls** oracles. Toute
brick de phase 1 dont le comportement ne serait vérifiable qu'au banc doit être
repoussée en phase 2 plutôt qu'écrite à l'aveugle.

## 9. Documents à corriger

- `CLAUDE.md` de ce dépôt : la section « Périmètre » dit que le firmware du split
  vivra dans rili. Faux — il vit ici.
- `rili/CLAUDE.md` : annonce un dossier `firmware/` à venir dans rili. À
  remplacer par un renvoi vers ce dépôt.
- `rili/docs/.../2026-07-29-rouge-gorge-refonte-design.md` §9 : « Rouge-Gorge
  occupe les 2 slots d'un dongle » — périmé, le slot 2 est la souris Conchodytes.
