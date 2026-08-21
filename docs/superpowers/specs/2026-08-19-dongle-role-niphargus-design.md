# Rôle du dongle dans le système Niphargus — design

**Date** : 2026-08-19
**Périmètre** : ce que le dongle fait, et ne fait pas, une fois le Niphargus en
place. Décide la répartition entre clavier et dongle, les deux protocoles de
lien, et la cadence du battement.

## 1. Contexte

Le dongle a porté trois identités contradictoires (`docs/DONGLE_ARCHI_ET_HALF_TYPING_2026-07-13.md`) :
cerveau du split, répéteur bête, co-processeur de sécurité. La question de la
répartition s'est reposée trois fois en deux mois. Ce document la ferme.

Deux contraintes nouvelles la tranchent, toutes deux posées par l'utilisatrice :

- **Le clavier doit fonctionner en USB, donc sans le dongle.**
- **Le coffre quitte le dongle** : le Niphargus embarque son propre ESP32-P4
  dédié. Le dongle perd la seule fonction qui lui était propre. L'option D de la
  spec rili — le coffre greffé derrière le hub CH334R du dongle — est morte.

S'y ajoute une décision déjà prise : **la configuration passe par USB**, en
direct sur la moitié concernée. Le dongle n'a donc pas à être un pont de config.

## 2. Le principe qui décide tout

Si le clavier doit fonctionner sans le dongle, alors **rien de fonctionnel ne
peut vivre côté dongle** — sinon le clavier serait diminué dès qu'on le
débranche.

La conséquence est plus forte qu'il n'y paraît. La moitié gauche doit de toute
façon appliquer la keymap **et** la gestuelle du trackpad pour fonctionner en
USB. Ces traitements existent donc déjà côté clavier. Les dupliquer côté dongle
serait deux chemins de code à maintenir, deux moteurs à garder synchronisés —
pour économiser quatre octets sur le fil (voir §6).

**Le dongle ne garde que ce que lui seul peut faire : être la radio côté hôte.**

## 3. Répartition

| | Responsabilités |
|---|---|
| Moitié **gauche** | matrice, trackpad, keymap, gestuelle → produit le **HID final** |
| Moitié **droite** | matrice, écran Sharp → envoie sa demi-matrice à la gauche |
| **Dongle** | reçoit du HID **déjà fini**, le repousse à l'hôte ; appairage des deux slots ; lit les heartbeats et expose l'état par CDC |
| **Souris** (Conchodytes) | envoie son propre HID sur le slot 2 |

En USB, la gauche sort le même HID par son propre port. **Un seul chemin de
code, quelle que soit la sortie** — c'est ce qui garantit que le mode filaire ne
soit pas un clavier au rabais.

Le dongle ne décode aucune matrice, n'applique aucune keymap, ne fusionne rien.

## 4. Deux liens, deux protocoles

Le système a deux liens radio, et ils n'ont pas les mêmes besoins.

**Droite → gauche : matrice brute.** La gauche a besoin de l'état des touches
pour appliquer la keymap. Le protocole KaSe existant convient tel quel :
`PKT_TYPE_KEY` pour les événements, `PKT_TYPE_HEARTBEAT` portant le bitmap
complet de la demi-matrice, et `hb_reconcile()` qui force les press/release
divergents. La réconciliation par bitmap garde donc tout son sens — **entre les
deux moitiés**, là où il y a une matrice.

**Gauche → dongle : HID final.** Le type `PKT_TYPE_HIDREPORT` existe déjà dans
`rf_packet.h`, décrit comme « keyboard-agnostic relay: final HID report ». Il a
été construit en juin pour l'expérience du relais V2D ; c'est exactement ce dont
le Niphargus a besoin. `rf_encode_hidreport_kbd()` produit **9 octets** : type,
sous-type, modificateur, six keycodes.

Ce rapport porte l'état complet, donc il est **idempotent** : tout paquet perdu
est réparé par le suivant, sans mécanisme de réconciliation.

## 5. Le battement, et pourquoi il est inévitable

Deux défaillances menacent un lien radio, et la retransmission n'en couvre
qu'une.

**Un rapport perdu.** L'ESB réémet jusqu'à 15 fois et signale `MAX_RT` s'il
renonce : la perte silencieuse n'existe pas, et l'émetteur sait. Comme chaque
rapport porte l'état complet, le suivant répare. Sauf si le rapport perdu était
**le dernier** — celui qui dit « tout relâché ». Rien ne suit, et la touche reste
enfoncée sur l'hôte. C'est le défaut F1 corrigé sur l'USB le 2026-08-19, mais ici
la perte est normale et non exceptionnelle.

**Le lien meurt.** Hors de portée, batterie vide, interrupteur coupé. Le dongle
doit relâcher toutes les touches.

Le second cas rend un battement périodique **obligatoire**, et ce n'est pas une
question de protocole mais de logique : le dongle ne peut pas distinguer « elle
ne tape pas » de « elle est morte » si elle ne dit rien dans les deux cas.
`hb_check_timeout()` implémente déjà ce relâchement sur silence.

Donc : **pas de connexion permanente, mais un battement périodique.** La radio
dort entre deux.

### Cadence adaptative

Le battement porte le **dernier rapport HID émis**, ce qui répare la release
perdue sans mécanisme supplémentaire. Sa cadence est adaptative :

- **Toute émission remet le compteur à zéro.** Pendant la frappe, les rapports
  eux-mêmes font office de battements : aucun trafic supplémentaire.
- **Après le dernier changement** : trois battements rapprochés, ~50 ms, qui
  couvrent la fenêtre où une release perdue ferait mal.
- **Au repos** : environ un battement par seconde — présence, `batt_dV`,
  `link_q`.

Ce n'est pas seulement une économie de batterie. La radio de la gauche partage
son temps entre écouter la droite et émettre vers le dongle ; des battements
pendant la frappe la rendraient sourde exactement quand la droite a le plus à
dire.

## 6. Budget de latence

Chiffré, en distinguant ce qui est mesuré, sourcé, ou calculé.

Une transaction ESB, d'après la datasheet nRF24L01+ (Table 19, p. 42, indexée
dans lemia sous `doc_id 6832`) : `TESB = TUL + 2·Tstby2a + TOA + TACK + TIRQ`,
avec `Tstby2a = 130 µs` et `TIRQ = 8,2 µs` à 1 Mbps. Pour une charge utile de
9 octets, adresse de 5 octets et CRC 16 bits :

- `TOA` = 8 × (1 + 5 + 9 + 2) + 9 bits = **145 µs**
- `TACK` (ACK sans charge utile) = 8 × (1 + 5 + 0 + 2) + 9 bits = **73 µs**
- `TUL` ≈ 10 µs sur SPI à 8 MHz
- **`TESB` ≈ 496 µs**, soit une demi-milliseconde par transaction.

La chaîne complète d'une frappe en sans-fil :

| Étage | Coût | Origine |
|---|---|---|
| Détection au scan | 0-1 ms | `BOARD_MATRIX_SCAN_INTERVAL_US 1000` |
| **Anti-rebond** | **3 ms** | `BOARD_DEBOUNCE_TICKS 3` |
| Droite → gauche | ~0,5 ms | datasheet, calcul ci-dessus |
| Bascule PRX/PTX du maître | 130 µs | datasheet, `Tstby2a` |
| Gauche → dongle | ~0,5 ms | datasheet |
| Dongle → hôte | 0-1 ms | endpoint interrupt à 1 ms, `usb_hid.c` |

**Environ 5 à 6 ms**, dont 3 ms d'anti-rebond. La radio pèse un peu plus d'une
milliseconde sur l'ensemble.

### Ce que ça dit du débit

Le débit soutenable est de l'ordre de 800 Hz côté radio et 1000 Hz côté USB.
À 120 mots/minute, une frappe produit une vingtaine d'événements par seconde :
**2 % de la capacité**. Le débit n'est pas un sujet.

Seule la latence d'un événement isolé compte, et **la radio n'en est pas le
premier poste**. Les deux vrais postes sont l'anti-rebond et, sur les claviers
actuels, le tick FreeRTOS à 100 Hz qui quantifie tout délai. La contrainte
« trames légères » est réelle mais déjà satisfaite : 9 octets, on ne fera pas
mieux.

**Ces 5-6 ms sont un calcul, pas une mesure.** L'instrumentation prévue au
chantier 2 du recentrage V2D dira si le calcul tient.

## 7. Ce que le dongle perd

Il compile aujourd'hui 37 fichiers de `main/`, dont les dix de `input/` — le
moteur, tap-hold, tap-dance, combo, leader, keymap — plus
`comm/rf/dongle_engine_state.c`, qui n'existe que pour fabriquer les globales
que `matrix_scan.c` fournit sur un clavier, et le mapping trackpad.

Tout cela devient sans objet. Le gain n'est pas la taille du binaire — 338 Ko,
ce n'est pas un problème — mais qu'**il ne puisse plus exister deux moteurs
keymap dans le système**. La question qui s'est reposée trois fois devient
structurellement impossible à reposer.

Il garde : l'appairage (`set_id`, adresses, deux slots), la réception RF, la
présentation HID à l'hôte, le CDC, et la lecture des heartbeats.

## 8. Risques

**R1 — les commandes CDC de keymap du dongle deviennent vides de sens.** Elles
doivent répondre une erreur explicite plutôt que de faire semblant : sinon le
logiciel de contrôle croira configurer un clavier qui ne l'écoute pas. C'est le
même raisonnement que pour la validation à l'upload — un rejet muet est pire que
le défaut.

**R2 — le partage de temps de la radio gauche reste non prouvé.** Elle doit
écouter la droite et émettre vers le dongle avec une seule radio. C'est le pari
pris le 2026-08-19 et il ne peut pas être levé sans cartes. Le repli documenté
reste le slot 2 partagé en multiceiver — écarté parce que la radio 2 appartient
à la souris, mais techniquement valide.

**R3 — le rip-out du moteur côté dongle est irréversible en pratique.** C'est son
intérêt, mais un retour à un dongle-cerveau demanderait de tout remonter.

**R4 — la cadence adaptative est un réglage, pas une vérité.** Trois battements
à 50 ms et un par seconde au repos sont des points de départ raisonnables, pas
des valeurs mesurées. À réévaluer au banc, avec la consommation réelle et le
taux de perte observé.

## 9. Hors périmètre

Le bring-up matériel, le driver de l'écran Sharp, le trackpad, l'énergie et le
sommeil : phase 2 du firmware Niphargus, bloquée sur la livraison des cartes.

Le coffre P4 : il quitte ce document avec le dongle. Sa place dans le clavier est
une question de la spec matérielle rili, pas d'ici.
