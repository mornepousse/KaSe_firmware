# Smoke-test hardware KaSe

À cocher AVANT chaque release / merge vers `main`. Couvre le runtime que les
tests host ne peuvent pas attraper. Flasher le board, cocher, garder une trace
dans la PR/release.

## Commun (tous boards)
- [ ] Boot sans boot loop (pas de Guru Meditation en boucle)
- [ ] USB HID : toutes les touches tapent le bon keycode (layer base)
- [ ] Layers MO/TO/LT/MT : bascule et retour OK
- [ ] Scan : aucune touche fantôme, aucune touche morte
- [ ] NVS préservée après reflash app-only (keymaps/macros intacts)

## V1 (round display + LED)
- [ ] Écran rond GC9A01 affiche sans corruption
- [ ] LED strip : animation par défaut OK
- [ ] Tama : sprite s'affiche et réagit

## V2 / V2D (OLED I2C)
- [ ] OLED SSD1306 affiche sans artefact
- [ ] V2D : COLS7/8 (GPIO21/4) scannent correctement

## OLED refonte multi-écrans (V2 / V2D)
- [ ] Boot : splash « KaSe » + version ~2s, puis HOME
- [ ] Splash UNIQUEMENT au boot : éteindre/rallumer l'écran (veille→réveil) → PAS de splash, retour direct sur HOME
- [ ] HOME (dashboard dense) : connexion (USB/BLE/RF + slot) + caps ; nom de couche ; KPM ; barres tama faim/joie ; **sprite du pet à droite**
- [ ] Changement de couche → HOME met à jour son champ couche (PLUS d'écran LAYER plein écran)
- [ ] Rien ne déborde / ne se chevauche sur aucun écran (texte, sprite, barres)
- [ ] Sprites tama = style rempli (pas de contour), lisibles ; le pet respire/cligne
- [ ] Touche K_DISP_NEXT (0x3F00, à mapper) : cycle HOME → STATS → TAMA → HOME
- [ ] STATS : KPM/WPM bougent en tapant, sparkline se remplit, total croît
- [ ] Idle ~10s (tama activé) → écran TAMA (sprite + barres + niveau, sans chevauchement) ; 1ʳᵉ frappe → retour HOME
- [ ] V2D : écran s'éteint à ~30s d'inactivité (> 10s → le TAMA a le temps de s'afficher) ; réveil à la 1ʳᵉ frappe
- [ ] Idle avec tama désactivé → reste sur HOME (pas de TAMA), pas de barres/sprite tama sur HOME
- [ ] Pas de scintillement / rebuild en boucle ; pas de reliquat (sprite tama) par-dessus l'écran suivant
- [ ] Presser une touche BT (switch/pair) pendant que HOME s'affiche → pas de crash, l'écran se reconstruit

## Dongle
- [ ] Lien RF s'établit avec une half (pairing < 120s)
- [ ] NRF ne se wedge pas après 5 min (watchdog OK)
- [ ] set_id survit à un erase_flash

## Half (left / right)
- [ ] e-ink affiche le splash 'PAIRED' au pairing
- [ ] Dashboard e-ink : L/R/USB + batterie, sans corruption
- [ ] Trackpad (si présent) : curseur, clic G/D/M, scroll
- [ ] BLE pairing OK + reconnexion après deep sleep
- [ ] Power Phase 1 : frappe reste instantanée (scan/TX inchangés)
- [ ] Power Phase 1 : après ~3 s sans frappe, le heartbeat ralentit (console half : TX heartbeat espacée) sans perte de lien
- [ ] Power Phase 1 : reprise immédiate du heartbeat 100 ms à la première frappe
- [ ] Power Phase 2 : entre en light-sleep après ~15 s d'inactivité (console muette)
- [ ] Power Phase 2 : chute de courant mesurée (WiFi + NRF off) — noter la valeur
- [ ] Power Phase 2 : 1re frappe réveille + s'enregistre (latence acceptable), suivantes plein régime
- [ ] Power Phase 2 : e-ink lisible gelé pendant le sommeil, redevient live au réveil
- [ ] Power Phase 2 : aucune touche bloquée/fantôme au réveil ; relâchement géré
- [ ] Power Phase 2 : si le réveil ne déclenche pas sur touche, inverser la polarité GPIO colonnes (cf. half_scan_arm_key_wake BENCH-TUNE)
- [ ] Power Phase 2 : left + right indépendamment

## BLE (boards concernés)
- [ ] Appairage host OK, tape sans drop pendant 1 min
- [ ] Bascule de slot BT OK
