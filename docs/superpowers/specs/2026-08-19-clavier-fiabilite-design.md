# Fiabilité du chemin de frappe — design

**Date** : 2026-08-19
**Périmètre** : premier des trois chantiers du recentrage du V2D sur l'USB.
Celui-ci ne traite que la **fiabilité** : qu'un appui produise exactement un
caractère et qu'un relâchement soit toujours vu. La latence et la régularité font
l'objet des chantiers 2 et 3.

## 1. Contexte

Le V2D ne sert qu'en USB. Deux allègements viennent d'être faits dans ce sens
(commit `be7f36e5`) : la persistance NVS des bigrams est retirée, et la pile BLE
sort du binaire — 1 103 728 → 584 896 octets.

L'objectif du recentrage est un clavier plus réactif, sur trois axes que
l'utilisatrice veut tous les trois : fiabilité, latence, régularité. L'ordre
retenu place la fiabilité en premier, parce que c'est **le seul axe qui possède
déjà son oracle** — les tests host. La latence et la régularité ne sont pas
falsifiables sans instrumentation, qui est l'objet du chantier 2.

Source des défauts : `docs/CODE_AUDIT_2026-07-07.md`. Ce document a six semaines
et une partie de ses findings a été fermée depuis ; l'état ci-dessous est
**re-vérifié dans le code**, pas recopié.

## 2. État réel des findings

### Ouverts et confirmés

**F3 — front perdu sur `stat_matrix_changed`.** `main/input/keyboard_task.c` lit
la matrice puis remet le drapeau à zéro :

```c
if (stat_matrix_changed == 1) {
    usb_try_remote_wakeup();
    build_keycode_report();     /* lit l'état */
    stat_matrix_changed = 0;    /* efface APRÈS */
```

Un callback de scan (priorité 5) qui tombe entre les deux pose un nouveau `1` et
de nouvelles données ; le `0` les efface. L'itération suivante voit `0` et saute
le bloc : l'appui est perdu. Confirmé par lecture le 2026-08-19.

**CR-1 — couche hors-borne, partiellement gardée.** Analyse approfondie le
2026-08-19, qui corrige la caractérisation initiale de cette spec.

Les sites de consommation sont gardés, mais inégalement :

- `MO` / `TO` : sûrs par construction — l'extraction est bornée par un test de
  plage (`keycode >= MO_L0 && keycode <= MO_L9`, `key_processor.c:50`).
- `LM` : gardé — `if (layer <= 9)` (`key_processor.c:57`). Fragile toutefois :
  c'est un littéral, pas `LAYERS`, donc il mentirait si `LAYERS` changeait.
- `LT` : gardé à l'entrée — `if (layer < LAYERS)` (`tap_hold.c:90`) — **mais le
  garde est contournable.** Il empêche seulement l'appel à `recompute_lt_layer()`
  et le bump d'`activate_seq` ; l'entrée hors-borne reste dans `pending[]` en état
  `TH_HOLD`. Or `recompute_lt_layer()` (`tap_hold.c:60-70`) rebalaye tout le
  tableau sans revérifier la borne, et retient son premier candidat par `!top`
  quel que soit son `activate_seq`. Une LT encodant une couche 10-15 entrée en
  `TH_HOLD` peut donc devenir `current_layout` lors d'un recalcul déclenché par
  une autre LT.

`K_LT_LAYER` et `K_LM_LAYER` masquent sur 4 bits, donc rendent 0-15 alors que
`LAYERS` vaut 10 : les valeurs 10-15 sont atteignables depuis un upload.

Ce qui reste à établir : l'entrée hors-borne atteint-elle réellement `TH_HOLD` ?
Le garde est placé dans la branche d'activation, la transition d'état peut lui
être antérieure. C'est une hypothèse précise et testable, pas une certitude —
elle se tranche par un test qui pilote une LT hors-borne jusqu'au hold et observe
`current_layout`.

Deux réponses possibles, et elles ne demandent pas le même travail. Si le chemin
existe : garde dans `recompute_lt_layer()` **et** validation à l'upload, en
défense de profondeur. S'il n'existe pas : seule la validation à l'upload reste
utile, comme garde de surface, et `layer <= 9` devient `layer < LAYERS`.

L'audit note que cette absence de validation est la racine commune de CR-1 et de
M6 : un seul garde à l'upload ferme les deux.

### Structurellement confirmé, effet à démontrer

**F2 — lecture déchirée des tableaux de scan.** Vérifié : `matrix_scan.c` et
`key_processor.c` ne contiennent **aucune** section critique ni sémaphore. Le
callback de scan (priorité 5) écrit `MATRIX_STATE`, `current_press_row/col/stat[]`
et `keycodes[]` pendant que `build_keycode_report` les relit depuis la tâche
clavier (priorité 3). La condition de course existe donc par construction.

Ce qui reste à établir : produit-elle un rapport HID faux observable, ou les
passes de lecture sont-elles en pratique assez courtes pour que la fenêtre ne se
referme jamais sur un état incohérent ? Les indices sont bornés, donc il n'y a
pas de lecture hors tableau — le risque est un keycode incohérent, pas un crash.

### Non vérifié

**EL-1 — modificateur coupé quand une de deux tap-hold du même modificateur est
relâchée (home-row mods).** Marqué `agent (fort)` dans l'audit : c'est du
raisonnement, jamais recoupé dans le code.

### Fermés depuis l'audit

F1 (report jeté sur endpoint occupé), F4 (appels TinyUSB réentrants) et F5
(report perdu si la queue est pleine) ont été corrigés le 2026-08-19, commits
`67c21b89` et `82250655`.

### Ajouté par cette analyse

**Le délai de grâce de la mise en queue n'existe pas.** `hid_report.c:183` écrit
`xQueueSend(hid_queue, &msg, pdMS_TO_TICKS(5))`. Le tick FreeRTOS est à 100 Hz,
donc `pdMS_TO_TICKS(5)` vaut **0 tick** : l'appel ne patiente pas. La branche
d'erreur ajoutée ce matin fonctionne, mais elle se déclenche plus tôt qu'écrit.
Le même piège existe partout où le code demande une attente inférieure à 10 ms.

Corriger la **cause** — le tick à 100 Hz — appartient au chantier 3. Ici on se
contente de rendre l'intention honnête : soit une valeur qui vaut au moins un
tick, soit l'aveu explicite qu'on ne patiente pas.

## 3. Règle de travail

**Aucun correctif n'est écrit avant qu'un test host reproduise le défaut.**

Cette règle n'est pas de la cérémonie. Deux findings de cette liste (F2, EL-1)
sont du raisonnement non vérifié, et la journée a montré ce que valent les
affirmations plausibles : le plan de la phase 1 Niphargus certifiait que le CRC
du lien reprenait le polynôme du protocole CDC. C'était faux, le code recopiait
fidèlement l'erreur, tous les tests passaient, et seule une relecture qui a
confronté le code à sa propre promesse l'a vu.

Conséquences pratiques :

- Un finding qui **se reproduit** obtient son test rouge, puis son correctif,
  puis le test vert.
- Un finding qui **ne se reproduit pas** est fermé comme non confirmé, et
  l'analyse qui le montre est consignée. Il n'est pas « corrigé par précaution » :
  un correctif spéculatif sur le chemin de frappe ajoute du risque sans retirer
  de défaut.
- Chaque test doit **prouver qu'il mord** — on casse le correctif, on constate le
  rouge, on rétablit. Un test qui passe quoi qu'il arrive éteint la question au
  lieu de la garder ouverte.

## 4. Ce que le chantier livre

Dans l'ordre, du plus certain au moins certain :

1. **F3** — test-and-clear du drapeau avant lecture plutôt qu'après.
2. **CR-1** — garde de validation des keycodes à l'upload, couvrant
   `KS_CMD_SETLAYER` et `KS_CMD_SETKEY`. Tout keycode encodant une couche hors
   `[0, LAYERS)` est rejeté avec `KS_STATUS_ERR_RANGE`.
3. **F2** — tentative de reproduction. Si elle aboutit : snapshot atomique ou
   section critique courte autour de la lecture. Sinon : fermeture documentée.
4. **EL-1** — tentative de reproduction sur le moteur tap-hold. Même règle.
5. **Attente de mise en queue** — rendre l'intention honnête.

## 5. Hors périmètre

Le tick FreeRTOS, les priorités de tâches, l'instrumentation de latence, les
écritures NVS restantes : chantiers 2 et 3. **Rien de temporel ne bouge ici**,
pour que l'instrumentation à venir mesure un chemin déjà fiabilisé — sinon on ne
saura pas distinguer un gain de fiabilité d'un gain de latence.

Les findings de l'audit classés ⚪ ou « peut-être by-design » (MO-2, F6, B3) ne
sont pas traités.

## 5 bis. Ces correctifs partent chez les utilisateurs du V2

Les allègements du V2D (bigrams, BLE) sont isolés par `sdkconfig.defaults.v2_debug`
et ne concernent que cette carte. **Les correctifs de fiabilité, eux, vivent dans
le code commun** — `keyboard_task.c`, `key_processor.c`, `matrix_scan.c`,
`cdc_binary_cmds.c`. Ce qu'on répare ici répare aussi le V1, le V2 et le dongle.

Le V2 est la carte de production, distribuée en binaires par GitLab Releases. Un
défaut de frappe corrigé ici profite donc à ses utilisateurs, et **une régression
introduite ici les atteint aussi**. Trois conséquences sur la méthode :

- Aucun correctif ne doit être conditionné à un board. S'il en faut un, c'est le
  signe que le diagnostic est incomplet.
- `./scripts/check.sh` complet — tous les boards — et non `--board kase_v2_debug`,
  avant chaque commit.
- La validation matérielle se fait sur le V2D parce qu'il est sur le bureau, mais
  `docs/HARDWARE_SMOKE_TEST.md` doit être déroulé sur le V2 avant la release qui
  emportera ces correctifs.

Le chantier se termine donc naturellement par une release, pas par un merge.

## 6. Tests

Tests host dans `test/`, ajoutés à `test/CMakeLists.txt` et déclarés dans
`test/test_main.c`, comme le veut la norme du dépôt. Ils doivent être
parallel-safe : pas d'état global muté, pas de chemin temporaire partagé.

La difficulté propre à ce chantier est que **F3 et F2 sont des courses entre deux
contextes d'exécution**, ce qu'un test host mono-thread ne reproduit pas
naturellement. L'approche retenue est d'extraire la logique de séquencement en
fonction pure et de lui injecter l'entrelacement à la main — le test simule le
callback qui tombe entre la lecture et l'effacement, plutôt que d'espérer que le
vrai ordonnanceur le produise. C'est le même motif que `hid_dedup.h`, qui a servi
à démontrer la perte de report ce matin.

## 7. Risques

**R1 — un correctif de course en introduit une autre.** F3 et F2 touchent la
synchronisation entre le callback de scan et la tâche clavier. Un snapshot mal
placé peut déplacer la fenêtre au lieu de la fermer. Mitigation : la revue de
chaque correctif doit ré-énumérer les entrelacements, pas seulement constater que
le test passe.

**R2 — la validation à l'upload casse un contrôleur existant.** Si `KeSp_controller`
envoie aujourd'hui des keycodes que le nouveau garde rejette, la configuration
cessera de fonctionner. Mitigation : le garde rejette uniquement ce qui est
réellement hors-borne, et le message d'erreur doit dire quel keycode et quelle
couche — un rejet muet serait pire que le défaut.

**R3 — l'absence de matériel n'est pas une excuse ici.** Contrairement à
Niphargus, le V2D est sur le bureau. Tout correctif de ce chantier doit être
flashé et frappé avant d'être considéré comme livré, en plus des tests host.
