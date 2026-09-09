# Epomaker × AULA F75 (modèle classique)

> **État : portage incomplet, non compilable, jamais flashé.**
> Le brochage n'est pas établi. `kbdef.h` refuse volontairement de compiler.

## Identité

| | |
| --- | --- |
| MCU | SinoWealth **SH68F90A**, marquage IC **BYK916** |
| USB | **258A:010C** — lus dans la flash d'usine, descripteur device en `0x5FCE` |
| Flash | 61 440 o firmware + 4 096 o bootloader ISP |
| Sans fil | **BK3632** Beken, relié par **EUART0** |
| Grille LED | **6 lignes × 15 colonnes** = 90 positions, 80 touches |

⚠️ À ne pas confondre avec l'**AULA F75 Ultra** (`FFFE:00A9`), qui est un Westberry WB32FQ95
ARM Cortex-M et relève de QMK.

## Ce qui est établi

| Élément | Valeur | Preuve |
| --- | --- | --- |
| VID / PID | `0x258A` / `0x010C` | descripteur device lu en flash `0x5FC6`, VID/PID en `0x5FCE` |
| Grille LED | 6 × 15 = 90 | `num_leds` lu en direct par le SDK OpenRGB ; index = colonne × 6 + ligne ; index max 89 |
| Nombre de touches | 80 | 80 LED nommées `Key: …` ; la disposition en place 80 sur 90, soit 10 trous |
| Disposition | 75 % ANSI | dérivée de `aula_f75_layout` (OpenRGB), recoupée par le compte de trous |
| Transport sans fil | EUART0 | ISR vecteur 13 = `_INT_EUART0`, `SCON` 0xD8 / `SBUF` 0xAA, half-duplex via `P0CR` + P0.2 |

## Brochage — ÉTABLI

Recoupé par quatre sources indépendantes. **Le firmware compile** :
15 208 o de ROM sur 60 416 (25,1 %), 835 o de XRAM sur 4 096.

### Sources

1. **Init GPIO consolidée du firmware d'usine**, `fcn @ 0xA7EC` :
   `P0CR=0x9C  P1CR=0x3F  P2CR=0x3F  P3CR=0x3F  P4CR=0x4D  P5CR=0x87  P6CR=0xFF  P7CR=0x60`
   (bit à 1 = sortie, cf. `GPIO_OUTPUT` dans `platform/sh68f90/gpio.h`)
2. **Table de saut de sélection de colonne** `@ 0x72F4` : 20 emplacements, 15 peuplés, les 5
   derniers pointant tous sur le handler nul. Chaque handler relâche la colonne précédente
   (`setb`) puis sélectionne la suivante (`clr`) — l'emplacement 0 relâche `P4.3`, ce qui ferme
   la boucle et fixe `P4.3` comme dernière colonne.
3. **Routine de lecture des lignes** `@ 0x73A6` :
   `mov a,P5 ; add a,ACC ; anl a,#0x30 ; mov r7,a ; mov a,P7 ; anl a,#0x0F ; orl a,r7 ; orl a,#0xC0`
4. **Portage indépendant** [`tiagoluizo/smk@aula-f75-port`](https://github.com/tiagoluizo/smk/tree/aula-f75-port),
   qui aboutit au même brochage par une analyse séparée.

### Résultat

| | Broches |
| --- | --- |
| **Lignes** (entrées, actives basses) | `P7.0` `P7.1` `P7.2` `P7.3` `P5.3` `P5.4` |
| **Colonnes** (sorties, actives basses, dans l'ordre de scan) | `P6.0`…`P6.7`, `P5.0` `P5.1` `P5.2` `P5.7`, `P4.0` `P4.2` `P4.3` |
| **Rétroéclairage** | 18 sorties PWM = 6 lignes LED × R/G/B : `PWM00`-`PWM05` (P3), `PWM10`-`PWM15` (P2), `PWM20`-`PWM25` (P1) |

L'octet de lignes place la ligne N sur le bit N (`P7 & 0x0F` pour les lignes 0-3, `P5` décalé d'un
cran et masqué `0x30` pour les lignes 4-5), bits inutilisés forcés à 1 — exactement la convention
attendue par `src/smk/matrix.c`, qui inverse ensuite l'échantillon.

**Question 15 vs 16 colonnes : tranchée.** La table `@ 0x72F4` dimensionne 20 emplacements mais
n'en peuple que 15. C'est bien **15** colonnes physiques.

## Ce qui reste NON établi

| Élément | État |
| --- | --- |
| Correspondance matrice de touches ↔ grille LED | ✅ **vérifiée** par capture HID indépendante — voir ci-dessous |
| Ordre de chargement des 18 canaux PWM | ✅ **relevé et implémenté** (`aula_rgb.c`) |
| Correspondance canal ↔ (ligne, couleur) | ❌ demande une observation sur matériel |
| Câblage du rendu dans la boucle SMK | ❌ inadéquation d'architecture, voir ci-dessous |
| Rotation d'encodeur | ❌ non implémentée (phases identifiées : `P0.5` / `P0.6`) |
| Broches au rôle inconnu | ❓ `P0.0` `P0.1` `P4.1` `P4.4` `P4.5` `P4.7` `P5.5` `P5.6` `P7.4` `P7.7` |
| Veille | ✅ **implémentée** — transcrite du firmware d'usine, non testée sur matériel |
| Sans-fil | ❌ hors périmètre, voir ci-dessous |

## Matrice : 81 touches, pas 80 — l'encodeur

Une **capture HID physique indépendante**
([tiagoluizo/smk](https://github.com/tiagoluizo/smk/blob/aula-f75-port/docs/superpowers/evidence/2026-08-07-aula-f75-keymap-capture.md))
a mappé la matrice 6×15 sur des usages HID uniques `0x04..0x5D`. Confrontée rangée par rangée à la
disposition dérivée d'OpenRGB, elle **correspond position par position** — avec une différence :

**Il y a 81 commutateurs, pas 80.** La position `[ligne 0][colonne 14]` est l'**appui d'encodeur**.
C'est une vraie position de matrice, invisible pour OpenRGB parce qu'elle n'a **pas de LED** — d'où
90 LED pour 81 touches, et 9 coordonnées réellement inutilisées.

La **rotation** est hors matrice. Poller `@ 0x7928`, appelé depuis le tick :

```asm
0x792b   mov  a, P0
0x792f   swap a          ; P0.5 -> bit1, P0.6 -> bit2
0x7930   rrc  a          ; -> bit0, -> bit1
0x7931   anl  a, #0x03   ; garde les deux phases
```

→ **phases sur `P0.5` et `P0.6`**, tous deux en entrée sous `P0CR = 0x9C`. Vérifié ici, et
cohérent avec l'init GPIO relevée indépendamment. Non implémentée dans ce portage.

## RGB : pourquoi le rendu de l'Air60 n'est pas réutilisable

Les deux claviers n'ont pas la **même topologie de matrice LED**, et c'est ça qui bloque, pas un
détail de câblage.

| | NuPhy Air60 | AULA F75 |
| --- | --- | --- |
| Colonnes LED | canaux **PWM** (`LED_PWM_C0` = `PWM40`, …) | colonnes de matrice en **GPIO simple**, sélecteurs de multiplexage |
| Lignes LED | broches GPIO simples (`RGB_R0R`, `RGB_R0G`, …) | les **18 canaux PWM** = 6 lignes × R/G/B |

C'est exactement l'inverse. Le rendu par défaut de SMK (`src/user/indicators_render.c`) est un
stub vide, et celui de l'Air60 (`layouts/default/indicators.c`) est écrit pour sa topologie.
**Ce portage n'a donc aucun RGB** — les `LED_PWM_C0..C17` de `kbdef.h` sont déclarés mais inutilisés
tant qu'un `indicators.c` propre n'est pas écrit.

### Ordre de chargement des canaux — relevé

`fcn @ 0x6E61` lit **36 octets consécutifs** du framebuffer (base XRAM `0x05A2` + colonne × 36) et
les écrit dans les registres `DUTY2`, en **gros-boutiste** (poids fort d'abord), en alternant les
deux DPTR via `INSCON` :

| Octets | Canaux | Broches |
| --- | --- | --- |
| 0-11 | `PWM20`…`PWM25` | P1.0…P1.5 |
| 12-23 | `PWM10`…`PWM15` | P2.0…P2.5 |
| 24-29 | **`PWM03`, `PWM04`, `PWM05`** | P3.3…P3.5 |
| 30-35 | **`PWM00`, `PWM01`, `PWM02`** | P3.0…P3.2 |

**Une seule irrégularité, et elle est délibérée : rotation de 3 dans le groupe P3** — `PWM03-05`
sont chargés *avant* `PWM00-02`. Les adresses ont été recoupées avec la carte XDATA `0xFF80-0xFFFF`
du datasheet SH68F90 CV2.0.

C'est le contenu de `aula_rgb.c`. **Ce qui reste inconnu : quel canal allume quelle
(ligne, couleur).** On sait dans quel ordre l'usine charge les registres, pas ce que chacun pilote.
Trancher demande d'écrire une seule voie et de regarder quelle LED s'allume — donc du matériel.

### Inadéquation d'architecture avec SMK

Le rendu par colonne exige de recharger les 18 duties **à chaque avance de colonne**. Dans le
firmware d'usine, c'est l'ISR `_INT_PWM0` qui possède l'avance de colonne — et qui en profite pour
lire les lignes de la matrice au passage (`0x73A6`).

SMK est bâti pour la topologie inverse : `src/smk/matrix.c` possède sa propre boucle de colonnes,
et `pwm_interrupt_handler` n'est qu'un **stub vide**. Faire cohabiter les deux demande un choix
d'architecture — soit l'ISR reprend la main sur les colonnes comme en usine, soit le rechargement
se greffe dans la boucle de scan — et ce choix ne se valide pas sans matériel.

### Ce qui est établi côté PWM

- Le MCU a **25 canaux** : PWM0 (00-05), PWM1 (10-15), PWM2 (20-25), PWM3 (30-33), PWM4 (40-42).
- Les **18** utilisés pour le RGB sont PWM0/1/2, confirmés par `P1CR=P2CR=P3CR=0x3F` dans l'init
  d'usine.
- La correspondance broche↔canal est déduite du NuPhy Air60 : `PWM0x↔P3_x`, `PWM1x↔P2_x`,
  `PWM2x↔P1_x`, `PWM4x↔P5_x`. **Déduite, pas confirmée par datasheet.**
- **Période PWM d'usine : `0x04B0` = 1200**, lue en `0x6707` (`PWM0PERDH=0x04`) et `0x670D`
  (`PWM0PERDL=0xB0`). Le portage `tiagoluizo` reprogramme la sienne à `0x0400` = 1024 et calcule
  ses rapports cycliques en conséquence (`0x0400 - (v << 2)`) — cohérent chez lui, mais **ce n'est
  pas la valeur d'usine**.
- Rapport cyclique **inversé** : 0 = éteint, 255 ≈ plein. Les LED sont donc à anode commune, le PWM
  fait office de sink.
- `PWM00CON` d'usine = `0x89` (`0x68D3`) = `PWM_MODE_ENABLE | PWM_SS | diviseur 1`.

## Veille — implémentée

Transcrite de la séquence d'usine. Le mécanisme de réveil est : **toutes les colonnes tenues
BASSES, toutes les lignes en entrée avec pull-up**. Un appui quelconque tire une ligne au 0, ce
qui déclenche INT4.

| Étape | Adresse d'usine |
| --- | --- |
| Parking du panneau (lignes entrée+pull-up, colonnes basses puis sorties) | `fcn @ 0x006E` |
| Parking hors matrice : P0.0/0.1, P0.5/0.6, P7.4/7.7, P4.5 → sortie basse ; P0.7 haut ; P7.6 bas | `0x7DCB`-`0x7DF1` |
| `EXF1 = 0` | `0x7DF4` |
| `IENC = 0xF3` | `0x7DF6` |
| `EXF0 = 0x40` | `0x7DF9` |
| `IEN0 \|= 0x02` (EX4) | `0x7DFC` |
| `PCON \|= 0x02` (power-down) | `0x7E34` |

**`IENC = 0xF3` et `EXF0 = 0x40` sont exactement les valeurs que `platform/sh68f90/extint.c`
documente** comme « transcrites du firmware d'usine » — confirmation indépendante sur cet appareil.
`extint_wake_arm()` est donc réutilisable tel quel.

`fcn @ 0x006E` est aussi une **quatrième confirmation du brochage** : il énumère littéralement les
6 lignes (`P7CR`/`P7PCR` bits 0-3, `P5CR`/`P5PCR` bits 3-4) puis les 15 colonnes, dans l'ordre.

⚠️ **Non testée sur matériel.** Un parking faux = un clavier qui ne se réveille pas.

## Sans-fil — transport caractérisé, protocole non résolu

### Ce qui est établi

Liaison **EUART0** vers le BK3632, en **half-duplex** :

| Élément | Valeur |
| --- | --- |
| Trame | **6 octets** (`mov r5,#0x06` @ `0xAB0B`) |
| Octet 0 | `0x01` (tête constante) |
| Octet 1 | code de commande — **`0x04`, `0x05`, `0x08`, `0x09`** observés |
| Octets 2-5 | charge utile |
| Broche de direction | **`P0.2`** — `clr P0.2` + `orl P0CR,#0x04` avant émission (`0xAB13`), relâchée en fin de trame par l'ISR (`anl P0CR,#0xFB` puis `setb P0.2`) |
| Routine d'envoi | `fcn @ 0xAB08` (charge nulle) / `0xAB09` (charge dans A) → `0xAB0F` |
| Buffers | TX en IDATA `0x33`, RX en IDATA `0x54` (23 o) |
| Drapeau émission | `0x2C.1` |

La commande `0x09` est émise depuis `main()` (`0x9148`, `0x914D`) — probablement l'initialisation
du lien.

### Deux voies pour obtenir la sémantique

#### A. Sniffer la liaison EUART0 — **recommandé**

Le transport est déjà caractérisé (ci-dessus). Il suffit d'un analyseur logique sur la ligne de
données du lien 8051 ↔ BK3632, en manipulant le clavier : bascule USB / BT / 2.4 G, appairage,
appuis de touches. Les trames font 6 octets, commencent par `0x01`, et la broche `P0.2` indique le
sens — donc on sait quoi chercher et comment séparer les deux directions.

**Coût : un analyseur logique et un point de test.** Aucun flashage, aucun dessoudage, aucun
nouveau binaire à désassembler. C'est la voie qui donne le protocole *tel qu'il est réellement
parlé*.

#### B. Dumper le firmware du BK3632 — beaucoup plus lourd

Le BK3632 est un **ARM9**, 20 Ko de RAM, 160 Ko de flash, BLE 5.0 + 2.4 G propriétaire.

Sur le **BK3432**, son frère, la [doc Tuya](https://developer.tuya.com/en/docs/iot/burn-and-authorize-BK3432-chip?id=Katc9lq3p4w1t)
donne le câblage : MOSI/MISO sur **P0.4/P0.5**, deux lignes de contrôle sur **P0.6/P0.7**, **VPP sur
RST**, plus 3 V et masse. Il faut un dongle *BEKEN SPI flasher* et le *HID Download Tool* en mode
« SPI SOFT ». Le firmware s'y présente en trois parties : boot, stack, app.

Ce que ça implique concrètement pour le F75 :

1. Accéder aux pads `P0.4`-`P0.7`, `RST`, `VCC`, `GND` d'un **QFN32** sur le PCB — loupe, pointes
   fines, voire dessoudage.
2. Le dongle BEKEN, ou adapter [`BK7231_SPI_Flasher`](https://github.com/openshwprojects/BK7231_SPI_Flasher)
   (Raspberry Pi, GPIO SPI) — **mais sa séquence d'init est celle du BK7231T**, et les fils
   elektroda comparant BK3432 / BK3431 / BK7231 existent précisément parce qu'**elles diffèrent**.
   Celle du BK3632 n'est pas établie.
3. Puis désassembler un binaire **ARM9** de 160 Ko : une cible entièrement neuve.

Et au bout, on obtiendrait l'implémentation du protocole, pas les échanges réels — alors que la
voie A donne directement ces derniers.

**Conclusion : la voie A d'abord.** La voie B ne se justifie que si l'on veut modifier le firmware
du BK3632 lui-même.

*(Les fils elektroda cités ici sont inaccessibles depuis cet environnement — timeouts systématiques,
probablement un filtrage des IP datacenter. À lire depuis un navigateur.)*

### Pourquoi ce n'est pas livrable

1. La **sémantique des commandes** n'est pas établie : on a les codes, pas leur signification.
2. Les **réponses du BK3632** ne sont pas analysées (buffer RX, drapeau `0x24.4`).
3. L'appairage et la gestion de lien sont hors de portée sans capture du trafic réel.
4. `src/platform/bk3632/rf_controller.c` de SMK suppose le **SPI bit-bangé** de l'Air60. Il
   faudrait lui écrire un transport EUART0 complet.
5. Rien de tout cela ne se valide sans **flasher et itérer sur le matériel**.

Le transport est donc documenté pour qui voudra le reprendre, mais **le sans-fil reste hors
périmètre de ce portage**.

## Pourquoi le sans-fil est hors périmètre

Le NuPhy Air60 parle à son BK3632 en **SPI bit-bangé** (`RF_BB_SPI_*`, `src/platform/bb_spi.c`).
L'AULA F75 utilise **EUART0**. `src/platform/bk3632/rf_controller.c` n'est donc **pas réutilisable
tel quel** : il faudrait lui écrire un transport EUART0. C'est pour cette raison que l'entrée
`meson.build` ne déclare **pas** `'wireless': 'bk3632'`.

## Récupération : ce que le bootloader garantit vraiment

Vérifié par désassemblage du bootloader de **cet** appareil (`assets/f75_full.bin`, zone
`0xF000-0xFFFF`). Deux chemins de retour existent.

### 1. Au reset — le bootloader décide

```asm
0xf000   clr EA ; mov SP,#0x70 ; init horloge
0xf017   mov 0x3b,#0xef ; mov 0x3c,#0xfb   ; DPTR <- 0xEFFB
0xf024   movc a, @a+dptr                   ; lit l'octet en 0xEFFB
0xf025   xrl  a, #0x02                     ; opcode LJMP ?
0xf027   jz   0xf053                       ; oui  -> part vers le firmware
         ...                               ; non  -> init USB + PLL, RESTE EN ISP
```

Le bootloader tourne **toujours** en premier (`0x0000: LJMP 0xF000`). Si l'octet en
`firmware_size-5` n'est pas `0x02`, il s'énumère en USB en mode ISP. Et `sinowisp write` ne
touche **jamais** la zone bootloader.

**Limite** : un firmware SMK correctement lié *aura* ce `0x02`. Ce chemin ne sauve donc que d'une
image tronquée ou mal écrite, pas d'un firmware qui démarre puis se plante.

### 2. Depuis le firmware — l'entrée magique `0xFF00`

```asm
0xff00   xrl a, #0x5a     ; A doit valoir 0x5A
0xff02   jnz 0xff36
0xff04   mov a, B         ; B doit valoir 0xA5
0xff06   xrl a, #0xa5
0xff08   jnz 0xff36
0xff0a   clr EA ; mov SP,#0x70
0xff15   mov USBCON,a ; USBIF1,a ; USBIF2,a ; USBIE1,a ; USBIE2,a ; USBADDR,a
```

C'est **exactement** le protocole de `isp_jump()` de SMK
(`src/platform/sh68f90/isp.c` : `mov B,#0xa5 ; mov A,#0x5a ; ljmp 0xff00`), et SMK l'expose déjà
par USB (`src/smk/usb.c:442`). **Ce bootloader l'implémente**, donc la porte de sortie logicielle
fonctionnera — à condition que la pile USB du firmware flashé démarre.

### Risque résiduel, énoncé précisément

| Scénario | Récupérable ? |
| --- | --- |
| Image tronquée, pas de `0x02` en `0xEFFB` | ✅ le bootloader reste en ISP au branchement |
| Firmware qui démarre et dont l'USB monte | ✅ `isp_jump()` par USB |
| **Firmware qui démarre mais se plante avant sa pile USB** | ❌ **programmateur externe requis** |

Le seul cas perdant est le troisième — c'est précisément celui qu'un **brochage faux** provoque :
le scan de matrice part sur de mauvaises broches, le firmware boucle ou se bloque, et l'USB ne
monte jamais.

**Conclusion : le brochage est le seul vrai verrou.** Une fois établi et vérifié, le risque
devient acceptable même sans programmateur externe. Tant qu'il ne l'est pas, ne rien écrire.

État du filet de sécurité :

| | |
| --- | --- |
| Dump firmware d'usine | ✅ `assets/f75_full.bin`, bootloader identique au bit près à la référence amont |
| Bootloader préservé par `sinowisp write` | ✅ documenté en amont, zone `0xF000+` non touchée |
| Retour ISP au reset | ✅ vérifié sur cet appareil (`0xF024`) |
| Retour ISP logiciel (`isp_jump`) | ✅ vérifié sur cet appareil (`0xFF00`) |
| Programmateur externe | ❌ absent — [sinodude-serial](https://github.com/carlossless/sinodude) sur Arduino Nano |
| `sinowisp` write testé sur ce modèle | ❌ annoncé ✅ en amont mais décoché dans l'issue #96 |

Un shim `tools/shim/sinowisp` (dans le projet parent) **refuse les écritures** tant que le
brochage n'est pas établi. Le retirer volontairement, pas par accident.
