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
| Correspondance canal ↔ (ligne, couleur) | ✅ **établie via le driver OpenRGB** |
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

### Correspondance canal ↔ (ligne, couleur) — établie par le driver OpenRGB

**Le code d'OpenRGB *est* le protocole documenté** — inutile de sniffer l'USB.
`SinowealthKeyboard10cController::SetLEDsDirect` construit un feature report de **520 octets** :

```c
buf[0x00] = 0x06;                      // Report ID 6
buf[0x01] = 0x08;                      // commande : écriture de la table de couleurs
buf[0x04] = 0x01;                      // octets d'adresse
buf[0x06] = 0x7A; buf[0x07] = 0x01;    // longueur = 0x017A = 378
buf[0x08 + i*3] = R, G, B;             // 3 octets par LED, i = indice LED
```

Deux corrections à [l'article de xevrion](https://xevrion.dev/blogs/aula-f75-linux-reverse-engineering) :
la commande est **`0x08`**, pas `0x0A`, et les couleurs font **3 octets** par LED, pas 4.

L'indice LED est celui de la table de disposition, soit **`colonne × 6 + ligne`**. Par colonne,
l'hôte envoie donc 18 octets : `ligne0 R,G,B`, `ligne1 R,G,B`, … D'où **`canal = ligne × 3 + couleur`**,
et en combinant avec l'ordre de chargement :

| Canaux | Lignes | Broches |
| --- | --- | --- |
| 0-5 | lignes 0 et 1 | `P1.0`…`P1.5` (`PWM20`…`PWM25`) |
| 6-11 | lignes 2 et 3 | `P2.0`…`P2.5` (`PWM10`…`PWM15`) |
| 12-14 | ligne 4 | `P3.3`…`P3.5` (`PWM03`…`PWM05`) |
| 15-17 | ligne 5 | `P3.0`…`P3.2` (`PWM00`…`PWM02`) |

**Deux lignes par port, six broches chacun.** La régularité du résultat est en soi un argument, et
elle explique la rotation apparente du groupe P3 relevée dans la table de chargement.

Reste une inférence : que le firmware range les octets reçus sans les permuter. C'est
l'implémentation naturelle, et le fait que la permutation vive dans la table de registres plutôt
que dans les données va dans ce sens. À confirmer sur matériel en n'allumant qu'une voie.

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

## Le BK3632 — ce qu'on sait, et avec quel degré de certitude

### Établi sur CET appareil (par désassemblage ou observation directe)

- **Il est présent sur le PCB.** Identifié par le mainteneur de `sinowisp` sur les photos du PCB de
  l'issue #96 : « another package variant for the **BK3632** ».
- **Il porte tout le sans-fil.** C'est pourquoi le support OpenRGB est **filaire uniquement** : le
  driver parle au SH68F90A, qui ne gère pas la radio.
- **Il dialogue avec le 8051 par EUART0** — vecteur 13 (`_INT_EUART0`), `SCON` 0xD8 / `SBUF` 0xAA,
  broches `P5.5` (TXD) et `P5.6` (RXD), trames `01 <cmd> <param> <len> …` de 6 à 32 octets à
  **260 870 bauds, 8N1**.
- **Il détient les noms Bluetooth.** La commande `0x09`, émise deux fois depuis `main()`, lui pousse
  `"AULA-F75 3.0 KB "` et `"AULA-F75 5.0 KB "` depuis la flash du 8051 (`0xAF6D`).

### Datasheet BK3633 — source primaire pour la famille

Le datasheet **BK3633 V0.9** (26 pages, Beken) documente le frère direct du BK3632. Attention :
**ce n'est pas le BK3632**, et les chiffres peuvent différer d'un membre à l'autre.

| | BK3633 (datasheet) |
| --- | --- |
| Cœur | **RISC 32 bits**, jusqu'à **80 MHz** |
| RAM | **80 Ko** de mémoire de données |
| Flash | **500 Ko** programmable |
| Radio | Bluetooth **5.2 dual-mode** + 2,4 GHz propriétaire |
| Consommation | ~5 mA en fonctionnement, < 1 µA en sommeil profond |
| Boîtiers | QFN32 4×4, QFN40 5×5, QFN48 6×6 |
| Interfaces | **2 UART et téléchargement UART**, I²C, SPI jusqu'à 96 MHz, USB hôte et device, 6 PWM, ADC 10 bits, JTAG sécurisé, AES128, eFUSE |

⚠️ **Correction** : une version antérieure de ce document annonçait « ARM9, 20 Ko de RAM, 160 Ko de
flash » pour le BK3632. Ces chiffres venaient d'un **résumé de moteur de recherche**, pas d'une
fiche technique. Le datasheet du BK3633 donne des valeurs très différentes. Les caractéristiques
exactes du **BK3632** restent non vérifiées.

### Trois interfaces de dump, nommées par le datasheet

> « At the beginning of the chip starts up, the chip will enter **programming mode, JTAG mode or
> normal** according received command from **Mode Selecting Pin**. »

Table 4 du datasheet, multiplexage des GPIO :

| GPIO | Normal | Mode PROGRAM | Mode JTAG |
| --- | --- | --- | --- |
| `GPIOA[0]` | `UART_TX` / SCL / USBDN | **`DL_UART_TX`** | — |
| `GPIOA[1]` | `UART_RX` / SDA / USBDP | **`DL_UART_RX`** | — |
| `GPIOA[3]` | SDA | — | `JTAG_NTRST` |
| `GPIOA[4]` | `SPI_SCK` | `SPI_MOSI` | `JTAG_TDI` |
| `GPIOA[5]` | `SPI_MOSI` | `SPI_MISO` | `JTAG_TDO` |
| `GPIOA[6]` | `SPI_MISO` / PWM[5] | `SPI_SCK` | `JTAG_TCK` |
| `GPIOA[7]` | `SPI_NSS` / PWM[4] | `SPI_CS` | `JTAG_TMS` |
| `GPIOB[6]` / `[7]` | `UART2_TX` / `UART2_RX` | — | — |

**« DL » = DownLoad.** Le téléchargement UART passe donc par `GPIOA[0]` et `GPIOA[1]`.

Sur le **QFN32**, ces deux broches sont les n° **23 (`P00_USBDN`)** et **24 (`P01_USBDP`)** — les
mêmes que les lignes USB D−/D+, multiplexées.

Trois voies existent donc, toutes documentées : **UART**, **SPI** et **JTAG**. Le mécanisme de
sélection — « une commande reçue sur la Mode Selecting Pin au démarrage » — n'est **pas détaillé**
dans ce datasheet abrégé. Mais la formulation recoupe exactement ce qu'on sait déjà : sur BK7231 on
spamme `0xD2` en SPI pendant le reset, et le BIM attend un `LINK_CHECK` en UART.

### ⚠️ Le verrou possible : chiffrement et interfaces condamnables

Section 3.8 du datasheet, « Code Encryption and System Security » :

> « There is one times NVM for code encryption and system security. Each unit can have different
> password for code encryption, where hardware will do the decryption automatically.
> **The download and debug interface could be closed permanently by user** to keep system security.
> […] Once the access right is changed, **no roll back is possible** to provide permanent security
> of the system. »

Trois conséquences, et elles priment sur tout ce qui précède :

1. **La flash peut être chiffrée**, avec un mot de passe par unité et déchiffrement matériel à la
   volée. Un dump réussi pourrait ne livrer que du chiffré.
2. **Les interfaces de téléchargement ET de debug peuvent être condamnées définitivement** par le
   fabricant, via une NVM one-time (type eFUSE). Cela fermerait d'un coup l'UART, le JTAG et le SPI.
3. **C'est irréversible.**

**On ne sait pas si Aula/SinoWealth a verrouillé le BK3632 de ce clavier.** Cela ne se détermine
qu'en essayant.

La bonne nouvelle : le test est **peu coûteux et non destructif**. Un adaptateur USB-TTL sur
`GPIOA[0]`/`GPIOA[1]`, une trame `01 E0 FC 01 00` (LINK_CHECK) répétée pendant un reset, et on sait.
Silence = probablement verrouillé, ou mauvaise séquence d'entrée. Réponse = la voie est ouverte.

Beaucoup d'appareils grand public bon marché ne prennent pas la peine de verrouiller. Mais tant que
ce n'est pas testé, **la voie B reste hypothétique, indépendamment de l'outillage**.

### Déduit du BK3432, son proche parent — **pas le même composant**

Le SDK BK3432 (`Cdreamyao/tuya_ble_sdk_Demo_Project_bk3432`) donne, en source primaire :

| | Valeur | Source |
| --- | --- | --- |
| Cœur | **ARM9E-S**, little endian, 60 MHz | `<Cpu>CPUTYPE(ARM9E)</Cpu>` du projet Keil |
| Table de vecteurs | ARM classique : reset, undefined, swi, pabort, dabort, reserved, irq, fiq | `boot_vectors.s` |
| Mapping | ROM `0x00000000`, RAM `0x00400000`, périphériques AHB `0x00800000+` | `BK3432_reg.h` |
| Bootloader | **BIM**, avec téléchargement par UART | `bim_uart.h`, et le PDF du SDK |

**C'est bien un ARM9, pas un Cortex-M** — la table de vecteurs à 8 entrées avec `FIQ` le prouve.
Cela corrobore le « ARM9 » du résumé fabricant, mais pour le BK3432.

**Conséquence pour un éventuel reverse** : ce n'est pas une cible 8051 de 61 Ko comme le SH68F90A,
mais **160 Ko d'ARM9** — une architecture entièrement différente, avec son propre jeu d'outils.

## Sans-fil — transport caractérisé, protocole non résolu

### Ce qui est établi

Liaison **EUART0** vers le BK3632, en **half-duplex** :

| Élément | Valeur |
| --- | --- |
| Trame | **longueur variable**, 6 à 32 octets (r5 avant l'appel) |
| Octet 0 | `0x01` (tête constante) |
| Octet 1 | code de commande — **`0x04`, `0x05`, `0x08`, `0x09`** observés |
| Octets 2+ | charge utile |

Longueurs observées aux points d'appel : `0x1E` (30) @ `0x46C2`, `0x0D` (13) @ `0x4730`,
`0x20` (32) @ `0xA376`, `0x17` (23) @ `0xB0B8`, `0x06` @ `0xED53`. Le maximum de 32 octets
correspond exactement à la taille du buffer TX, qui s'arrête juste avant le buffer RX.
| TXD / RXD | **`P5.5`** / **`P5.6`** (broches distinctes : lien full-duplex) |
| Handshake | **`P0.2`** — mis à 0 avant émission (`0xAB13`), relâché en fin de trame par l'ISR (`0xA580`) |
| Format | mode 1, **8N1**, `Baud = Fsys / 92` (`0xB1CE`) |
| Routine d'envoi | `fcn @ 0xAB08` (charge nulle) / `0xAB09` (charge dans A) → `0xAB0F` |
| Buffers | TX en IDATA `0x33`-`0x52` (**32 o**), RX en IDATA `0x54` (23 o) |
| Drapeau émission | `0x2C.1` |

La commande `0x09` est émise depuis `main()` (`0x9148`, `0x914D`) — probablement l'initialisation
du lien.

### Pas de firmware BK3632 embarqué — vérifié

Hypothèse testée : le 8051 embarquerait-il l'image du BK3632 pour la lui pousser par le lien série ?
**Non**, et c'est mesuré, pas supposé.

1. **Place disponible.** Le firmware d'usine fait 61 440 o et son dernier octet significatif est en
   `0xEFC3` : il reste **60 octets libres**. Le BK3632 a **160 Ko** de flash. Une image complète ne
   peut pas y tenir.
2. **Profil d'entropie.** Balayage par blocs de 512 o : **aucun bloc au-dessus de 6,53**. Pas de
   zone compressée ni chiffrée. Du code ARM9 se situerait autour de 6 — mais les seules zones non
   codées sont à l'inverse *très basses* : `0xB200`-`0xC400` (entropie 1,10, **87 % de zéros**) et
   `0xC600`-`0xEC00` (1,20, **93 % de zéros**). Ce sont des tables creuses — bitmaps et keymaps par
   couche — pas du code.
3. **Débit du lien.** Le buffer TX fait 32 octets et la plus longue trame observée en fait 32.
   Rien n'indique un chemin de transfert en volume.

Conclusion : le firmware du BK3632 réside **dans le BK3632**. Pour l'obtenir il faut le lire sur
la puce (voie B ci-dessous), et pour comprendre le protocole il vaut mieux écouter le lien
(voie A).

### Grammaire des trames — établie

Le format est décodé, et une commande l'est sémantiquement.

```
[0] = 0x01        en-tête constant
[1] = commande
[2] = paramètre / sous-commande
[3] = longueur de la charge utile
[4..] = données
```

#### Commande `0x09` — définir le nom Bluetooth

Construite en `fcn @ 0xA307`, appelée **deux fois depuis `main()`** (`0x9148` et `0x914D`) :

```asm
mov @r0,#0x01     ; [0] en-tête
mov @r0,#0x09     ; [1] commande
mov @r0,#0x01     ; [2] profil  (0x00 dans la branche alternative, selon r7)
mov @r0,#0x10     ; [3] longueur = 16
mov dptr,#0xaf6d  ; copie 16 octets depuis la flash
```

Et `0xAF6D` contient, en clair :

```
AF6D  "AULA-F75 3.0 KB "
AF7D  "AULA-F75 5.0 KB "
```

**Les deux noms d'annonce Bluetooth du clavier**, 16 caractères chacun, complétés par une espace.
Le paramètre `[2]` choisit lequel — d'où les deux appels depuis `main()`.

#### Autres commandes repérées

| Cmd | Où | Forme | Interprétation |
| --- | --- | --- | --- |
| `0x04` | `0xB084` | `01 04 <octet>` | un paramètre lu en XRAM `0x0EF6` |
| `0x06` | `0xED41` | `01 06 00 00 00`, longueur 6 | charge nulle — sonde ou requête d'état ? |
| `0x08` | `0xB09E` | longueur 23 | remplit depuis IDATA `0x48` |
| `0x09` | `0xA307` | longueur 32 | **nom Bluetooth** (ci-dessus) |

*Correction : une version antérieure de ce document attribuait `0x05` au site `0xED53`. C'est
faux — `0xED53` est l'appel d'émission, et la trame qu'il envoie porte la commande `0x06`,
construite juste avant en `0xED41`.*

*Et la piste « HCI over UART » est écartée : le SDK BK3432 décrit bien un transport HCI pour son
bootloader, mais `01 09 01 10` ne parse pas comme du HCI (l'opcode serait invalide). C'est un
protocole propriétaire — dont la grammaire est maintenant connue.*

### Deux voies pour obtenir la sémantique

#### A. Sniffer la liaison EUART0 — **recommandé, et SANS FLASHER**

**Aucun flashage. Le clavier tourne avec son firmware d'usine.** On écoute passivement le fil entre
le 8051 et le BK3632. Risque nul pour l'appareil.

##### Où sonder

Le datasheet donne les broches d'EUART0 :

| Signal | Broche | Sens |
| --- | --- | --- |
| **TXD** | **P5.5** | 8051 → BK3632 |
| **RXD** | **P5.6** | BK3632 → 8051 |

✅ **Remappage écarté par vérification.** Le SH68F90 peut déplacer son EUART0 sur `P3.3`/`P3.4`
(`TXD_M`/`RXD_M`) en écrivant `0x5A` dans le registre **`MAPPING` (SFR `0x8A`)**, dont la valeur de
reset est `0x00`. Une recherche de toute écriture vers `0x8A` — `mov`, `anl`, `orl` — dans
l'intégralité du firmware d'usine ne renvoie **aucun résultat**. `MAPPING` reste donc à `0x00` et
l'UART reste sur ses broches par défaut.

C'était un contrôle nécessaire : `P3.3` et `P3.4` portent aussi `PWM03` et `PWM04`, soit les canaux
12 et 13 du rétroéclairage (ligne 4, rouge et vert). Un remappage aurait signifié que ces broches
sont partagées, et aurait invalidé à la fois le point de sonde UART et la table RGB.
| handshake | `P0.2` | mis à 0 avant chaque trame, relâché à la fin (optionnel, 3ᵉ voie) |

⚠️ **Correction** : une lecture antérieure de ce document décrivait le lien comme *half-duplex avec
la direction sur `P0.2`*. C'est faux. `TXD` et `RXD` sont **deux broches distinctes** — le lien est
full-duplex. `P0.2` est un signal d'attention vers le BK3632, pas un sélecteur de direction :
`clr P0.2` + `P0CR |= 0x04` (sortie) avant l'émission (`0xAB13`), puis `P0CR &= ~0x04` (entrée) +
`setb P0.2` en fin de trame, dans l'ISR (`0xA580`).

Conséquence pratique heureuse : **deux voies d'analyseur capturent les deux directions séparément**,
sans démultiplexage.

##### Comment configurer l'analyseur

Configuration relevée dans le firmware d'usine (`0xB1CE`) :

```asm
mov SCON,  #0x50   ; mode 1 : asynchrone, 10 bits (1 start, 8 data, 1 stop) = 8N1, REN=1
mov SBRTH, #0xFF   ; SBRTEN=1, SBRT[14:8]=0x7F
mov SBRTL, #0xFB   ; SBRT[7:0]=0xFB      -> SBRT = 0x7FFB = 32763
mov SFINE, #0x0C   ; BFINE = 12
```

Formule du datasheet : `Baud = Fsys / (16 × (32768 − SBRT) + BFINE)`

→ `Fsys / (16 × 5 + 12)` = **`Fsys / 92`** = **260 870 baud**.

**`Fsys` = 24 MHz, confirmé par recoupement.** Le driver UART de SMK
(`platform/sh68f90/uart.c`) implémente la même formule ; avec `FREQ_SYS = 24000000` et
`UART_BPS = 260870` il calcule `SBRT_INT = 5` → `SBRT = 32763 = 0x7FFB` → `SBRTH = 0xFF`,
`SBRTL = 0xFB`, et `SFINE = 24000000/260870 − 80 = 12 = 0x0C`. **Les trois registres du firmware
d'usine, à l'identique.** Une valeur de `Fsys` différente ne les reproduirait pas.

**En pratique, ne pas se fier à ce calcul :** un analyseur logique mesure la largeur du bit le plus
court et en déduit le débit. PulseView (sigrok) et Saleae le font automatiquement. Le calcul sert à
vérifier que la mesure est plausible.

##### Avec quoi capturer — l'analyseur logique n'est pas obligatoire

| Outil | Coût | Ce qu'il donne | Limites |
| --- | --- | --- | --- |
| **Adaptateur USB-TTL FTDI** | ~3 € | le flux UART décodé, directement | une voie par adaptateur ; **3,3 V obligatoire** |
| Analyseur logique | ~10 € | les deux voies + le timing exact + `P0.2` | — |
| Arduino / Pi Pico | ~5 € | idem FTDI, avec horodatage et débit libre | à programmer |
| **USBPcap + Wireshark** | **0 €** | le protocole **hôte ↔ clavier** (Report ID 6) | **pas** le lien 8051 ↔ BK3632 |

**L'adaptateur USB-TTL est le meilleur rapport effort/résultat**, et beaucoup de gens en ont déjà un.

Le débit non standard n'est pas un obstacle avec une puce **FTDI** : son générateur fait
`baud = 3 000 000 / diviseur` avec un diviseur fractionnaire par 1/8. Pour 260 870 le diviseur vaut
`11,5` — exactement réalisable. Pour 130 435 il vaut `23` — exact aussi. Sous Linux, `stty -F
/dev/ttyUSB0 260870` suffit. Un CH340 est moins fiable sur les débits exotiques.

⚠️ **Câblage, en RÉCEPTION SEULE :**

- `RX` de l'adaptateur sur `P5.5` (ou `P5.6`), **masse commune** avec le clavier.
- **Ne jamais connecter le `TX` de l'adaptateur.** Y injecter des données perturberait le lien, et
  un adaptateur 5 V détruirait une entrée 3,3 V.
- Un seul adaptateur ne capture qu'une direction. Deux adaptateurs, ou deux passes successives.

##### Et l'option sans aucun matériel

**USBPcap + Wireshark** (Windows) ou `usbmon` + `tshark` (Linux) capturent le trafic USB entre le
PC et le clavier — **gratuitement, sans rien souder**. Mais c'est une **couche différente** : on y
voit le canal HID Report ID 6 (`0x04` write config, `0x84` read, `0x0a` table de couleurs…), pas
les trames EUART0 vers le BK3632.

C'est donc l'outil pour finir de documenter le **protocole de configuration** — celui de l'article
de xevrion, celui qui donne le RGB et le remap. Ce n'est pas celui du sans-fil. Les deux chantiers
sont distincts, et celui-là ne coûte rien.

##### Quoi capturer

Ce qu'on sait déjà chercher (voir le tableau du transport plus haut) : trames commençant par
`0x01`, deuxième octet = commande (`0x04`, `0x05`, `0x08`, `0x09` observés), longueur variable de
6 à 32 octets.

Séquences à enregistrer, chacune isolément :

1. Branchement USB puis bascule en Bluetooth — la commande `0x09` part de `main()` (`0x9148`), donc
   quelque chose passe dès l'initialisation.
2. Appairage d'un hôte Bluetooth.
3. Bascule vers 2.4 G.
4. Quelques appuis de touches dans chaque mode — pour isoler la trame « rapport HID ».
5. Mise en veille et réveil.

En croisant ces captures avec les cinq sites d'émission déjà localisés (`0x46C2` 30 o, `0x4730`
13 o, `0xA376` 32 o, `0xB0B8` 23 o, `0xED53` 6 o), on relie chaque commande à son déclencheur.


Le transport est déjà caractérisé (ci-dessus). Il suffit d'un analyseur logique sur la ligne de
données du lien 8051 ↔ BK3632, en manipulant le clavier : bascule USB / BT / 2.4 G, appairage,
appuis de touches. Les trames font 6 octets, commencent par `0x01`, et la broche `P0.2` indique le
sens — donc on sait quoi chercher et comment séparer les deux directions.

**Coût : un analyseur logique et un point de test.** Aucun flashage, aucun dessoudage, aucun
nouveau binaire à désassembler. C'est la voie qui donne le protocole *tel qu'il est réellement
parlé*.

#### A-bis. Sonder le BK3632 depuis un firmware maison — sans matériel externe

Idée : au lieu d'écouter le fil de l'extérieur, faire tourner **notre propre firmware** sur le 8051
et lui faire parler au BK3632, en journalisant tout sur la console USB de SMK.

**⚠️ L'écoute passive ne marcherait PAS.** Si notre firmware tourne, celui d'usine ne tourne pas :
plus personne n'envoie de commandes, et le BK3632 est un esclave qui attend. On n'entendrait rien.

**Ce qui marche, c'est le sondage actif.** On connaît le format des trames et les codes de
commande, relevés dans le firmware d'usine. Notre firmware peut donc **rejouer les séquences
exactes** que l'usine construit et journaliser les réponses. Le BK3632 réel sert d'oracle.

##### Tout l'outillage existe déjà dans SMK

| Brique | Où |
| --- | --- |
| Driver UART (init, `putc`, `getc`) | `src/platform/sh68f90/uart.c` |
| Console de debug sur USB HID | `src/smk/console.c`, `dprintf()` |
| Lecteur côté hôte | `tools/smk-console` |
| Retour en ISP par USB | `src/smk/usb.c:442` → `isp_jump()` |

Réglages à poser : `UART_BPS = 260870`, `UART_RX_EN = 1`, et la console en sortie USB (pas UART,
puisque l'UART sert à parler au BK3632).

##### Séquences à rejouer

Les cinq sites d'émission repérés donnent les trames à reproduire, avec leur longueur :

| Site | Longueur | Commande | Déclencheur connu |
| --- | --- | --- | --- |
| `0xA376` | 32 | `0x09` | appelé depuis `main()` (`0x9148`) — init du lien |
| `0x46C2` | 30 | ? | — |
| `0xB0B8` | 23 | `0x08` | — |
| `0x4730` | 13 | ? | — |
| `0xED53` | 6 | `0x05` | — |

Le contenu exact des charges utiles se lit dans le désassemblage de chaque site.

##### Profil de risque — **il faut flasher**

C'est la seule voie de cette section qui l'exige. Ce qui l'atténue :

- **Firmware minimal** : USB + UART seulement, ni matrice, ni RGB, ni veille. Moins de code, moins
  de chances de se bloquer avant que l'USB monte.
- **Deux retours ISP vérifiés sur cet appareil** : le bootloader teste `0xEFFB` au reset
  (`0xF024`), et `isp_jump()` fonctionne via l'entrée magique `0xFF00`.
- **Image d'usine disponible** : `assets/f75_full.bin`, MD5 relevé, restaurable.

Ce qui reste : **`sinowisp write` n'a jamais été testé sur ce modèle** (case décochée dans
l'issue #96), et pas de programmateur externe de secours.

##### Comparaison

| | Sonde externe (USB-TTL / analyseur) | Firmware maison |
| --- | --- | --- |
| Flashage | **aucun** | **requis** |
| Matériel | adaptateur ~3 € + 2 points de soudure | aucun |
| Observe | le dialogue **réel** de l'usine | seulement ce qu'on provoque |
| Risque | nul | brique possible |

**Les deux sont complémentaires.** La sonde externe montre ce que l'usine dit *vraiment*, dans
l'ordre et le contexte. Le firmware maison permet d'*interroger* le BK3632 librement, y compris sur
des commandes que l'usine n'émet jamais. Commencer par la sonde externe reste plus sage : elle
donne la vérité de terrain sans rien risquer.

#### B-bis. Le bootloader UART Beken — la voie la plus prometteuse pour un dump

⚠️ **On n'a pas le firmware du BK3632.** Il est dans la puce ; l'obtenir *est* le but de cette
section. Ce qui suit vient du SDK **BK3432** (`Cdreamyao/tuya_ble_sdk_Demo_Project_bk3432`), pas
d'un dump.

##### Le BIM expose un bootloader UART

`bk3432/projects/bim/app/bim_uart.h` déclare :

```c
typedef enum _UART_CMD_STATE {
    UART_CMD_STATE_HEAD, UART_CMD_STATE_OPCODE_ONE, UART_CMD_STATE_OPCODE_TWO,
    UART_CMD_STATE_LENGTH, UART_CMD_STATE_CMD, UART_CMD_STATE_CMD_FLASH,
    UART_CMD_STATE_LENGTH_FLASH_LEN0, UART_CMD_STATE_LENGTH_FLASH_LEN1,
    UART_CMD_STATE_LENGTH_FLASH_SCMD, UART_CMD_STATE_PAYLOAD, ...
} UART_CMD_STATE;

#define LINK_CHECK_CMD     0x00
#define CRC_CHECK_CMD      0x10
#define SET_RESET_CMD      0x0E
#define SET_BAUDRATE_CMD   0x0F
#define STAY_ROM_CMD       0xAA
```

Trame : `HEAD, OPCODE_ONE, OPCODE_TWO, LENGTH, CMD, [payload]`, avec des sous-états dédiés aux
commandes flash.

##### Ce sont les MÊMES codes que le BK7231

`BK7231GUIFlashTool/BK7231Flasher/Flashers/BK7231Flasher.cs` construit ses trames ainsi :

```csharp
ret[0] = 0x01; ret[1] = 0xe0; ret[2] = 0xfc; ret[3] = len; ret[4] = cmd;
enum CommandCode { LinkCheck = 0, ..., CheckCRC = 0x10, SetBaudRate = 0x0f, ... }
```

`HEAD = 0x01`, `OPCODE_ONE = 0xE0`, `OPCODE_TWO = 0xFC` — exactement les états nommés du BK3432.
Et `LinkCheck = 0x00`, `SetBaudRate = 0x0F`, `CheckCRC = 0x10` **coïncident avec les `#define` du
BIM**.

**La famille Beken partage donc son protocole de bootloader UART.**

##### Pourquoi cela change la voie B

| | Voie SPI (décrite plus haut) | Voie UART |
| --- | --- | --- |
| Matériel | CH341 (~5 €) | **adaptateur USB-TTL (~3 €)** |
| Accès physique | pads SPI + CEN + VPP d'un **QFN32** | **les lignes UART, déjà localisées** |
| Séquence d'entrée | inconnue pour le BK3632 | codes connus, à tenter |
| Dessoudage | probable | **non** |

Les lignes UART sont celles reliant le 8051 au BK3632 — les mêmes qu'on sonderait pour écouter le
dialogue (voie A). **Un seul point d'accès physique sert aux deux usages.**

Config UART du BIM, relevée dans `bim_uart.c` : **115200 bauds, 8 bits, parité PAIRE, 1 stop**
(`data_len 0x3`, `parity_en 0x1`, `parity_mode 0x1`, `stop_bits 0x0`). La parité paire est
inhabituelle, donc discriminante : si un `01 E0 FC 01 00` en 8E1 obtient une réponse, on est dans
le bootloader.

Le SDK embarque aussi `bk3432/doc/BK3432 Download by UART User's Guide V3.0.pdf`, qui **contredit
la doc Tuya** (« BK3432 only supports firmware flashing through SPI »).

##### Ce qui reste incertain

- Le `uart_cmd_dispath()` n'est **pas** dans les sources livrées — il est déclaré dans le header
  mais réside vraisemblablement en ROM. Les codes viennent du header et du recoupement BK7231.
- Le BK3632 n'est pas le BK3432.
- Entrer dans le bootloader suppose que la puce reste en ROM au démarrage — `STAY_ROM_CMD 0xAA`
  suggère un mécanisme, non documenté ici.
- Sur le clavier, l'UART du BK3632 est reliée au 8051 : il faudrait probablement empêcher celui-ci
  de parler pendant la tentative.

#### B. Dumper le firmware du BK3632 par SPI — plus lourd

Le BK3632 est un **ARM9**, 20 Ko de RAM, 160 Ko de flash, BLE 5.0 + 2.4 G propriétaire.

Sur le **BK3432**, son frère, la [doc Tuya](https://developer.tuya.com/en/docs/iot/burn-and-authorize-BK3432-chip?id=Katc9lq3p4w1t)
donne le câblage : MOSI/MISO sur **P0.4/P0.5**, deux lignes de contrôle sur **P0.6/P0.7**, **VPP sur
RST**, plus 3 V et masse. Il faut un dongle *BEKEN SPI flasher* et le *HID Download Tool* en mode
« SPI SOFT ». Le firmware s'y présente en trois parties : boot, stack, app.

Ce que ça implique concrètement pour le F75 :

1. Accéder aux pads `P0.4`-`P0.7`, `RST`, `VCC`, `GND` d'un **QFN32** sur le PCB — loupe, pointes
   fines, voire dessoudage.
2. Un programmateur. Le dongle *BEKEN SPI flasher* (vendeur), ou la voie communautaire décrite
   plus bas — **mais la séquence d'init du BK3632 n'est pas établie**.
3. Puis désassembler un binaire **ARM9** de 160 Ko : une cible entièrement neuve.

##### La méthode Beken « SPI flash mode », et sa limite

Le mode SPI des Beken est documenté en détail pour le **BK7231T**
([elektroda, p.kaczmarek2](https://www.elektroda.com/rtvforum/topic3931424.html)). Principe : la
puce **expose sa propre flash interne comme une mémoire SPI esclave**, qui s'identifie comme un
EN25QH16B.

```
SPI mode 3, 30 kHz          (les fréquences plus hautes échouent)
CEN bas -> attendre 1 s -> CEN haut
envoyer 0xD2 x 250          -> réponse : un 0xD2 puis 249 x 0x00
identifier : 9F 00 00 00    -> 00 15 70 1C
```

Ensuite, commandes de flash standard : `0x03` lecture page, `0x02` programmation page,
`0x20` effacement secteur, pages de **256 octets**, adresses sur 3 octets. L'écriture exige
l'effacement préalable.

Broches sur BK7231 : `P20`-`P23` = SCK, CSN, SI, SO, plus **CEN** (reset).

Outillage recommandé aujourd'hui : [`BK7231GUIFlashTool`](https://github.com/openshwprojects/BK7231GUIFlashTool)
avec un **programmateur CH341** (~5 €), en mode « Beken SPI » et non générique, avec **D2 du CH341
relié à CEN** pour piloter le reset. C'est nettement plus accessible que le Raspberry/Banana Pi de
l'article d'origine, ou que le dongle vendeur.

**⚠️ Aucun outil ne *déclare* la famille BK3xxx — mais le code d'entrée SPI est générique.**

Vérifié :
[`BK7231GUIFlashTool`](https://github.com/openshwprojects/BK7231GUIFlashTool) (428 ★, maintenu,
dernier push 2026-08-10) liste ses modes — BK7231M/N/T/U, BK7236, BK7238, BK7252, BK7252N, BK7258,
« Beken SPI CH341 » et « Generic SPI CH341 » — soit **uniquement la famille WiFi BK72xx**. Une
recherche de `BK3xxx` dans l'intégralité de ses sources C# ne renvoie **aucune occurrence**.

Le mode « Generic SPI CH341 » sert à lire une flash SPI *externe* (boîtier SOIC8). Celle du BK3632
est **interne** : il faudrait la séquence d'entrée en mode esclave SPI, qui est spécifique à la
famille BK72xx.

#### Nuance : l'entrée SPI Beken est paramétrable

`BK7231Flasher/Flashers/SPIFlasher_Beken.cs` fait **90 lignes** et se révèle largement agnostique :

```csharp
ChipReset()                  // CEN bas via D2 du CH341 -> 100 ms -> CEN haut
BK_EnterSPIMode(byte data)   // <-- l'octet magique est un PARAMÈTRE
    // envoie `data` x 250 (10 blocs de 25)
    // puis 9F 00 00 00
    // succès si resp[0] != 0x00 ET resp[1..3] == 0x00
    // puis vidange : 25 000 zéros
Sync()                       // 10 tentatives de (reset + entrée), 1 s d'intervalle
```

Deux points comptent :

1. **`0xD2` n'est pas codé en dur** — c'est l'argument de `BK_EnterSPIMode`. Essayer une autre
   valeur est une modification d'une ligne.
2. **Le test de succès ne vérifie pas l'ID `00 15 70 1C`** — seulement « premier octet non nul,
   trois suivants nuls ». Il accepterait donc l'identifiant d'une autre puce.

L'outil ne *déclare* pas la famille BK3xxx, mais rien dans ce chemin ne l'exclut techniquement.
**Tenter coûte un CH341 (~5 €) et une modification triviale.**

#### Ce qui reste incertain

- Le BK3632 expose-t-il seulement un mode esclave SPI ? Non établi.
- La doc Tuya du BK3432 mentionne **VPP sur RST**, ce qui suggère un mécanisme différent d'un
  simple basculement de CEN — possiblement une tension de programmation.
- Les broches SPI diffèrent déjà entre BK7231 (`P20`-`P23`) et BK3432 (`P0.4`-`P0.7`). Celles du
  BK3632 sont inconnues.

*(`BK7231Flasher.cs` lui-même est le protocole **bootloader UART** du BK72xx — trames
`01 e0 fc <len> <cmd>` — sans rapport avec la voie SPI.)*

**La transposition à la famille BK34xx n'est PAS établie.** La question a été posée
textuellement sur ce fil le 21 juillet 2025 — « I have a bk3432 chip […] is the initialization
sequence the same? » — et la seule réponse, en octobre 2025, a été « Just try ». Personne n'a
publié de confirmation.

Deux indices qu'elle **diffère** : les broches SPI sont `P20`-`P23` sur BK7231 mais `P0.4`-`P0.7`
sur BK3432, et VPP passe par RST. Le BK3632 est encore un autre membre de la famille.

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
