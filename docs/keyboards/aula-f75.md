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

## Ce qui n'est PAS établi — feuille de relevé

Rien de ce qui suit n'est devinable depuis le firmware d'usine sans travail supplémentaire.
**Ne pas recopier les valeurs du NuPhy Air60** : même MCU et même marquage BYK916, mais PCB
différent.

| Symbole | Quantité | Comment l'établir | État |
| --- | --- | --- | --- |
| `KB_R0..R5` | 6 | désassembler le scan de matrice, ou sonder les pads | ❌ |
| `KB_C0..C14` | 15 (ou 16) | idem | ❌ |
| Masques de port colonnes | 1 par port | déduits des broches ci-dessus | ❌ |
| `LED_PWM_C0..C14` | 15 | canaux PWM utilisés par le rétroéclairage | ❌ |
| `RGB_R*R/G/B` | 18 | broches R/G/B par ligne | ❌ |
| Switch mode connexion | 0 ou 1 | existence à confirmer d'abord | ❌ |
| Switch mode OS | 0 ou 1 | idem | ❌ |
| Broches EUART0 ↔ BK3632 | 2 + direction | TX/RX + la broche de direction (P0.2 ?) | ❌ |

### Question ouverte : 15 ou 16 colonnes ?

La grille LED donne **15** colonnes de façon certaine. Mais une 16ᵉ colonne de matrice **sans LED**
resterait invisible pour OpenRGB. Les bornes de boucle cherchées dans le firmware d'usine
(`fcn.00003108` et `fcn.000005EA`, atteintes depuis `_INT_TIMER2`) n'ont rien donné de concluant :
seules des constantes `0x15` (21) et `0x17` (23) ressortent, et `0x17` est la taille du buffer RX
de l'UART, sans rapport.

À trancher avant d'écrire `MATRIX_COLS`.

### Pistes pour le relevé

1. **Désassemblage** — repartir de `_INT_TIMER2` (vecteur 0, `0xA4D7`) vers `fcn.00003108` et
   `fcn.000005EA`, et chercher des écritures de port avec des masques de 6 bits, sur le modèle des
   `KB_C_P3_MASK` de l'Air60. Le harnais est dans `tools/f75_r2.py` du projet parent.
2. **Sondage PCB** — un multimètre en continuité entre les pads du MCU et les diodes de matrice.
   Plus lent, mais c'est la seule méthode qui produit une certitude.

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
