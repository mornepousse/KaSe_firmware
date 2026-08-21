# kase_dongle — récepteur USB du Niphargus

Récepteur USB pour le clavier split Niphargus. Il reçoit du **HID déjà fini** par
NRF24L01+ et le présente à l'hôte en USB HID composite + CDC binaire.

Matériel : `~/Documents/PCB-esp/dongle/dongle/` (projet KiCad 9, format M.2 Key B 3042).

## Ce qu'il fait, et ce qu'il ne fait pas

Le dongle **n'a pas de matrice, pas de keymap, pas de moteur d'entrée**. La moitié
gauche du Niphargus fait tourner le sien — elle doit de toute façon le faire pour
fonctionner en USB sans dongle — et n'envoie ici que des rapports HID terminés.
Le dongle les repousse tels quels.

Ce n'est pas un détail d'implémentation : c'est ce qui rend **structurellement
impossible l'existence de deux moteurs keymap dans le système**, question qui
s'était reposée trois fois. Le moteur, le bloc CLAVIER du protocole CDC et la
keymap de carte ne sont pas compilés pour ce rôle ; les commandes CDC de keymap
ne sont pas enregistrées et répondent `KS_STATUS_ERR_UNKNOWN`, ce qui dit
franchement au logiciel de contrôle que cet appareil ne fait pas ça.

Il garde : l'appairage, la réception RF sur deux slots, la présentation HID, le
CDC et la supervision (batterie, qualité de lien).

Conception complète : `docs/superpowers/specs/2026-08-19-dongle-role-niphargus-design.md`.

## Les deux slots

Ce ne sont plus deux moitiés d'un même clavier :

| Slot | Radio | Appareil |
|---|---|---|
| 0x01 | NRF#1 | le clavier — moitié maître du Niphargus |
| 0x02 | NRF#2 | la souris **Conchodytes** |

D'où la règle que `main/comm/rf/rf_slot.h` verrouille : **la perte d'un slot ne
relâche que ce que ce slot tenait**. Une souris qui sort de portée ne doit pas
effacer la frappe en cours.

Les clés NVS d'appairage gardent leurs noms d'origine (`mac_left` / `mac_right`) :
les renommer désapparierait le matériel déjà appairé pour un gain cosmétique.

## Build & flash

```bash
source ~/esp/esp-idf/export.sh
idf.py -B build_kase_dongle -DBOARD=kase_dongle \
       -DSDKCONFIG=build_kase_dongle/sdkconfig build
```

Chaque carte a son propre dossier de build **et son propre `sdkconfig`**. Ne
jamais construire deux cartes dans le même `build/` avec le `sdkconfig` racine :
la configuration fuit de l'une à l'autre.

⚠️ **Le numéro `/dev/ttyUSBN` du dongle et celui du V2D s'échangent d'un
branchement à l'autre.** Vérifier la MAC avant tout flash — le dongle est
`ac:a7:04:18:81:ec` — plutôt que de se fier au numéro de port.

```bash
idf.py -B build_kase_dongle -p /dev/ttyUSB0 flash
```

## Brochage (ESP32-S3-WROOM-2)

Relevé sur la netlist `dongle.kicad_sch` :

| Signal | GPIO | Notes |
|---|---|---|
| SPI MOSI | GPIO5 | partagé, R15 100 Ω série |
| SPI MISO | GPIO6 | partagé, R16 100 Ω série |
| SPI SCK | GPIO7 | partagé, R17 100 Ω série |
| NRF#1 (clavier) CSN / CE / IRQ | GPIO13 / GPIO14 / GPIO8 | |
| NRF#2 (souris) CSN / CE / IRQ | GPIO1 / GPIO4 / GPIO2 | |
| USB D+ / D− | GPIO20 / GPIO19 | OTG natif, full-speed |
| Bootstrap IO0 | GPIO0 | mode flash via RTS du CH340 |

GPIO1 est une broche de strapping UART0 qui flotte à l'état bas au reset : les
deux CSN sont forcés à l'état haut avant d'initialiser la moindre radio, sinon
NRF#2 capte en parallèle le trafic SPI destiné à NRF#1 et le corrompt.

## État

- **Bring-up** ✅ énumère `303a:4001`, CDC PING/FEATURES répondent.
- **Pile NRF RX** ✅ les deux radios probent sur la vraie carte, canaux 76/82,
  `rf_rx_task` démarre.
- **Relais HID** — codé ; le critère de validation au banc est
  `ack ≈ 100 %` avec `maxrt = 0` en frappe soutenue. Il ne peut pas être vérifié
  avant les cartes Niphargus : le V2D n'a plus son module nRF.

L'ESP-NOW a été retiré avec le firmware des anciennes moitiés.
