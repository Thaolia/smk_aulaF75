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
| Correspondance matrice de touches ↔ grille LED | ⚠️ le keymap suppose une correspondance 1:1 avec la grille OpenRGB — **non vérifiée** |
| Rendu RGB (permutation ligne/couleur) | ❌ les 18 broches PWM sont déclarées, mais `indicators.c` n'est pas porté |
| Broches inutilisées identifiées | ❓ `P0.0` `P0.1` `P0.5` `P0.6` `P4.1` `P4.4` `P4.5` `P4.7` `P5.5` `P5.6` `P7.4` `P7.7` — rôles inconnus (switches ? batterie ?) |
| Veille | ❌ désactivée volontairement (`USER_SLEEP_NONE`) |
| Sans-fil | ❌ hors périmètre, voir ci-dessous |

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
