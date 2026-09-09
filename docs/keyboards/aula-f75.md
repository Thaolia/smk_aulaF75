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

## `fcn.0000AF99` — publication de l'effet RGB, protégée contre l'USB

Appelée depuis `main()` (`0x9197`) et depuis `fcn.000005EA`, la grosse fonction périodique.

```asm
0xaf99   jnb  0x2c.7, 0xafc3     ; rien a faire si le drapeau n'est pas pose
0xaf9c   clr  0x2c.7             ; acquitte
0xaf9e   anl  IEN1, #0xfe        ; masque EUSB -- l'interruption USB
0xafa1   mov  dptr,#0x0e24 ; movx a,@dptr
0xafa5   cjne a, #0x20, 0xafb6   ; cas particulier si l'effet vaut 32
0xafa8     mov r4,#0xca ; rrc a  ; 202 et 101
0xafb3     lcall 0xaac1
0xafb6   clr  0x24.5
0xafb8   mov  dptr,#0x0e24 ; movx a,@dptr
0xafbc   mov  dptr,#0x009d ; movx @dptr,a    ; publie l'effet
0xafc0   orl  IEN1, #0x01        ; demasque EUSB
0xafc3   ret
```

`IEN1` bit 0 = **`EUSB`**, confirmé deux fois : header SMK (`#define _EUSB (1u << 0)`) et datasheet
(`IEN1 A9H : - ES0 EPWM4 EPWM3 EPWM2 EPWM1 EPWM0 EUSB`).

**C'est une section critique.** `0x0E24` porte l'effet *en attente*, `0x009D` l'effet *courant*, et
la copie se fait USB masqué parce que le chemin USB touche aussi `0x009D`. Le réglage par les
touches et le réglage par l'hôte se disputent la même variable ; cette fonction arbitre.

### `0xA4BD` — le second poseur de `0x2C.7`, résolu

`0x2C.7` (« publier l'effet ») a deux producteurs. Le premier, `0x0A71`, est le chemin
utilisateur déjà documenté. Voici le second.

#### Comment on y arrive : un dispatcher à table de sauts sur `XRAM 0x0D17`

`0xA674` :

```asm
mov dptr,#0x0d15 ; movx a,@dptr ; jnz 0xa67d
jb  0x2a.3, sortie
mov dptr,#0x0d17 ; movx a,@dptr
cjne a,#0x0a, .+3 ; jnc sortie      ; rejette 0x0D17 >= 10
mov dptr,#0xa68d
mov r0,a ; add a,r0 ; add a,r0      ; index x 3
jmp @a+dptr
```

Table de 10 entrées de 3 octets en `0xA68D` :

| `0x0D17` | cible |
| --- | --- |
| 0, 9 | `ljmp 0x83D2` |
| 1 | — |
| 2 | `ljmp 0x9363` |
| 3 | `ljmp 0x8C0F` |
| 4 | `0xA6C6` |
| **5, 6, 7** | **`ljmp 0xA468`** |
| 8 | `0xA6D2` |

`XRAM 0x0D17` est donc un index 0 à 9, et les valeurs 5, 6 et 7 partagent le même handler —
d'où le fait que `0xA468` les re-teste une par une en interne. Chaque autre branche est gardée
par `jb 0x2a.5` et `jb 0x23.3` ; la branche 5/6/7 ne l'est pas, parce que `0xA468` gère lui-même
le verrou `0x2A.5`. Voir plus bas ce que `0x0D17` est réellement.

### `XRAM 0x0D17` — la classe d'action d'une touche, lue dans une table en flash

`0x0D17` n'est pas une variable que le firmware calcule : c'est un **champ extrait d'une table**.

#### L'idiome d'extraction

Onze sites écrivent `0x0D17`, tous avec le même prologue. Les deux accesseurs le décodent :

```asm
fcn.00004DEC:                  ; charge 4 octets de CODE @ DPTR
    clr a  ; movc a,@a+dptr ; mov r4,a
    mov a,#1 ; movc a,@a+dptr ; mov r5,a
    mov a,#2 ; movc a,@a+dptr ; mov r6,a
    mov a,#3 ; movc a,@a+dptr ; mov r7,a

fcn.00004DC5:                  ; r4:r5:r6:r7 >>= r0   (decalage de r0 BITS, sur 32 bits)
    mov a,r0 ; jz fin
boucle: rrc sur r4, r5, r6, r7 ; djnz r0, boucle
```

`fcn.00004DC5` prend donc un nombre de **bits**, pas d'octets : `r0 = 8` et `r0 = 16` extraient
respectivement l'octet 2 et l'octet 1 d'un enregistrement de 4 octets. Dans `fcn.00007ECA` :

```asm
mov B,#4 ; mov a,r3 ; mul ab
mov DPL,a ; mov a,B ; addc a,#0xce ; mov DPH,a   ; DPTR = 0xCE00 + index x 4
lcall fcn.00004DEC
mov r0,#0x08 ; lcall fcn.00004DC5 ; movx [0x0d16], r7
...
mov r0,#0x10 ; lcall fcn.00004DC5 ; movx [0x0d17], r7
```

| Variable | Décalage | Octet de l'enregistrement |
| --- | --- | --- |
| `0x0D15` | 0 | b3 |
| `0x0D16` | 8 | b2 |
| `0x0D17` | **16** | **b1** |
| — | 24 | b0 |

L'index vient de `XRAM 0x02E0`, écrit par `fcn.00003108` (`0x3190`) depuis le compteur
d'événements `IDATA 0x09`.

#### La table `0xCE00` — vérifiée sur le dump

Extrait des 40 premières entrées de `assets/f75_firmware.bin` :

| idx | offset | b0 b1 b2 b3 | `0x0D17` | dispatch |
| --- | --- | --- | --- | --- |
| 0 | `0xCE00` | `00 00 00 29` | 0 | `0x83D2` |
| 1 | `0xCE04` | `00 00 00 35` | 0 | `0x83D2` |
| 2 | `0xCE08` | `00 00 00 2b` | 0 | `0x83D2` |
| 3 | `0xCE0C` | `00 00 00 39` | 0 | `0x83D2` |
| 4 | `0xCE10` | `00 02 00 00` | 2 | `0x9363` |
| 5 | `0xCE14` | `00 01 00 00` | 1 | — |
| 11 | `0xCE2C` | `00 04 00 00` | 4 | `0x8EC4` |
| 12 | `0xCE30` | `02 00 00 70` | 0 | `0x83D2` |
| 17 | `0xCE44` | `00 08 00 00` | 8 | `lcall 0x91D3` |
| 36 | `0xCE90` | `08 03 02 00` | 3 | `0x8C0F` |

**Sur toutes les entrées non nulles, `b1` vaut 0, 1, 2, 3, 4 ou 8 — jamais ≥ 10.** La borne
`cjne a,#0x0A ; jnc sortie` du dispatcher n'est jamais violée par la table d'usine. C'est une
validation empirique du décodage : si l'octet extrait avait été le mauvais, la distribution
n'aurait aucune raison de tomber dans les bornes.

Et quand `b1 == 0`, **`b3` contient un code d'usage HID clavier** : `0x29` Échap, `0x35`
backquote, `0x2B` Tab, `0x39` Verr.Maj, `0x04`–`0x1D` les lettres, `0x1E`–`0x23` les chiffres.
Deux entrées portent `b0 = 0x02` — le bit **LeftShift** du champ modificateurs HID.

#### Lecture

L'enregistrement est donc `[modificateurs, classe d'action, paramètre, usage HID]` :

| Octet | Variable | Rôle |
| --- | --- | --- |
| b0 | — | modificateurs HID (`0x02` = LeftShift observé) |
| b1 | `0x0D17` | **classe d'action**, index du dispatcher `0xA68D` |
| b2 | `0x0D16` | paramètre (routé vers `0x031F` quand la classe vaut 9) |
| b3 | `0x0D15` | code d'usage HID |

Le dispatcher `0xA674` teste d'ailleurs `0x0D15` en premier (`jnz`), et `0x83D2` — la classe 0 —
refait le même test avant de partir sur le chemin d'émission en `0x845F`. Classe 0 = « émettre
cet usage HID » ; les classes 1 à 8 sont des actions spéciales, dont 5/6/7 l'effet temporaire
décrit plus haut.

> *Inféré :* que cette table soit la **table de remap** du clavier. Ce qui est établi, c'est sa
> structure, son adressage, l'extraction, et le fait que `b3` contienne des usages HID.

#### Conséquence : la table est réinscriptible

`0xCE00` tombe dans la **page 103**, en plein dans la zone IAP `0xC600`–`0xEBFF` établie plus
haut — et la page 103 est justement l'une des sept pages non vierges du dump. Le firmware peut
donc réécrire cette table par sa propre routine `fcn.0000AAC1`, sans passer par l'ISP.

C'est le point d'entrée le plus prometteur pour un remap : il ne demande ni de reflasher le
firmware, ni de porter quoi que ce soit — seulement d'atteindre la bonne commande du protocole
de configuration. **Non tenté : rien n'a été flashé.**

#### `fcn @ 0xA468` — deux transitions symétriques autour de l'effet `0x2D`

Le tout est verrouillé par `0x2A.5`, et sort immédiatement si `0x28.2` est posé.

**Entrée, `0xA483`** — si `0x0D17 ∈ {5, 7}`, effet courant `0x009D == 0x20`, verrou libre :

```asm
setb 0x24.5      ; drapeau teste par euart0.parse (0x0632, 0x07A5)
setb 0x2a.5      ; verrou « effet temporaire actif »
setb 0x24.1
setb 0x27.7
movx [0x0e24], a ; a == 0x20 : l'effet a restaurer, dans « effet en attente »
movx [0x009d], #0x2d   ; effet courant force a 0x2D
```

**Sortie, `0xA4BD`** — si (`0x0D17 == 7` ou `== 6`), effet courant `== 0x2D`, verrou posé :

```asm
setb 0x2c.7      ; publier l'effet      <-- le second producteur
clr  0x2a.5      ; libere le verrou
clr  0x24.5
movx [0x08c0], [0x0d18]
movx [0x08c1], #0x00
movx [0x08c2], #0x02
setb 0x27.4      ; sonnette
clr  0x2c.0      ; reautorise le parsing EUART0
```

L'effet `0x2D` est donc un **effet temporaire** : le firmware range l'effet courant `0x20` dans
`0x0E24`, bascule sur `0x2D` tant que le mode le justifie, puis republie en sortant. C'est le
même mécanisme « effet en attente → effet courant » que le chemin utilisateur, déclenché par un
changement de mode au lieu d'une touche.

#### `clr 0x2C.0` — ce que la sortie débloque

`isr.timer2` teste ce bit en `0xA504` :

```asm
jb 0x2c.0, 0xa51c      ; saute par-dessus le `lcall euart0.parse` de 0xA519
```

Tant que `0x2C.0` est posé, **les trames EUART0 reçues ne sont pas analysées**. La sortie du
mode temporaire les réautorise. Le lien entre le RGB et le sans-fil passe donc aussi par là.

#### `0x08C0` n'est pas un tampon RGB : c'est un rapport HID

Huit sites écrivent la même structure de 3 octets en `0x08C0` puis posent `0x27.4` (`0x84D6`,
`0x8900`, `0x8CE5`, `0x8F94`, `0x9412`, `0xA4C3`, `0xB041`, plus `0x05AB`). C'est un idiome
partagé, pas une particularité de ce chemin.

`fcn.00006ACF` est le **seul** consommateur de `0x27.4` — un dispatcher qui parcourt les
drapeaux en attente par ordre de priorité :

```asm
jnb  0x27.4, suivant
setb 0x26.7
clr  0x27.4
movx [0x0f0c], #0x02
movx [0x08bf], #0x03          ; a = 2 puis inc a
movx [0x0f0f], #0x01
movx [0x0f10], #0x08
mov  a, #0xbf
ljmp 0x6c07
```

`0x0F0F..0x0F11 = 01 08 BF` est un **pointeur générique** vers `0x08BF` — l'adresse même du
tampon écrit juste au-dessus. Le drapeau suivant (`0x2A.6`) suit le même schéma avec
`01 09 80` → `0x0980`. Et l'octet 0 du tampon vaut `0x0F0C + 1` : **3** ici, **4** pour le
suivant. Ce sont des **Report ID HID**.

Le « message » de `0xA4BD` est donc le rapport HID `03 <0x0D18> 00 02` poussé vers l'hôte —
la notification d'un changement d'état, pas une couleur.

### Comment le firmware choisit un effet

Chaîne complète, du réglage à l'application :

```
octet de config, quartet HAUT (4 bits)
        |  swap a ; anl a,#0x0f          @ 0x07F8
        v
   XRAM 0x038D      sous-index d'effet, 0..15
        |  add a,#0x20                   @ 0x0A77
        v
   XRAM 0x0E24      effet EN ATTENTE
        |  setb 0x2C.7 ; lcall 0xAF99    @ 0x0A71, 0x0A7D
        v
   XRAM 0x009D      effet COURANT        (copie USB masqué)
        |
        v
   tables 0xA8BC / 0xA8D5   ->  paramètres du mode
```

**Le sous-index tient sur un quartet**, extrait par `swap a ; anl a,#0x0f` : d'où **16 effets
possibles**, décalés en **32..47** par le `+ 0x20`. Cela explique le `cjne a, #0x20` de
`fcn.0000AF99` : ce n'est pas une valeur magique, c'est la **borne basse de la plage**.

Trois chemins écrivent `0x0E24` :

| Site | Rôle |
| --- | --- |
| `0x0A79` | **chemin utilisateur** — `0x038D + 32`, puis publication immédiate |
| `0x9184` | **restauration au démarrage** — relit `0x009D` juste après `fn.fx_init` (`0xA283`), dans `main()` |
| `0xA48B` | **chemin du tick** — teste `0x009D == 0x20` puis pose quatre drapeaux (`0x24.5`, `0x2A.5`, `0x24.1`, `0x27.7`) |

`0x038D` est lu à 18 endroits et écrit à 3 (`0x02B3`, `0x034E`, `0x07FC`). Les lectures le comparent
à `8` (`0x0B1F`), lui ajoutent `-2` (`0x0DB5`), ou le décrémentent — ce sont les navigations
« effet suivant / précédent ».

À côté, `0x038E` et `0x038F` forment un petit bloc de configuration écrit dans la foulée
(`0x038E = 0`, `0x038F = r5`), non identifié.

### `XRAM 0x009D` — index d'effet RGB

Vingt sites y accèdent. Trois convergences l'identifient :

- `0x4494` y écrit `0x20` (32) — la valeur même que teste `fcn.0000AF99`
- `0x52CC` et `0x5377` le comparent à `9`
- `0x15E7`, `0x15FA`, `0x8C30`, `0x8C47` s'en servent pour **indexer deux tables en flash**

| Base | Contenu | Étendue |
| --- | --- | --- |
| `0xA8BC` | `00`, puis `04` × 23, puis `00` | index 0-24 |
| `0xA8D5` | `00`, puis `09` × 32 | index 0-32 |

Le code reprend en `0xA8F6`. Index 0 donne zéro, tous les autres une constante : ce sont des
**paramètres par mode**, le mode 0 étant traité à part — vraisemblablement « éteint ». L'index monte
au moins à 32, ce qui couvre le `0x20` de `0x4494`.

*Inférence restante : que ces constantes soient des paramètres d'effet RGB. La convergence est
forte — index borné, mode 0 nul, écriture depuis le chemin USB, arbitrage contre l'ISR USB — mais
aucune n'a été suivie jusqu'à un registre PWM.*

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

Liaison **EUART0** vers le BK3632, en **full-duplex** :

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
| Format | mode 1, **8N1**, `Baud = Fsys / 92` = **~260 870 bauds** (`0xB1CE`) |
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

### Toutes les fonctions qui touchent EUART0 — balayage exhaustif

Balayage de l'image entière sur les sept SFR de l'EUART0 (`SCON` D8H, `SBUF` AAH, `SADDR` ABH,
`SADEN` ACH, `SBRTH` ADH, `SBRTL` AEH, `SFINE` AFH), plus le bit d'autorisation et la priorité,
recoupé par un scan octet à octet de tous les opcodes 8051 à opérande direct.

| Fonction | Rôle | SFR touchés |
| --- | --- | --- |
| `0xB1C2` | **init** — débit, mode, autorisation | SBRTH, SBRTL, SFINE, SCON, PCON, IEN1 |
| `0xA544` | **ISR EUART0** (vecteur `0x6B`) | SCON.RI/TI, SBUF, SCON |
| `0xAB0F` | émetteur générique, longueur variable | SBUF |
| `0xACE6` | émetteur de trame courte (6 o) | SBUF |
| `0xEF40` | `orl IEN1,#0x40` — réarme l'IRQ (2 appels depuis `0x84E9`) | IEN1 |
| `0xECC5` @ `0xEDE0` | priorité : `IPH1 = 0x42`, `IPL1 = 0x41` → bit 6 posé des deux côtés, **niveau 3** | IPH1, IPL1 |
| `0x7D74`, `0x9E39` | veille : coupent EUART0, rappellent `0xB1C2` au réveil | SCON, IEN1 |
| `0x84E9` @ `0x8548` | coupe EUART0 | IEN1 |
| `0x41F2`, `0x4227`, `0x425D` | activent EUART0 après avoir posé XDATA `0x0319` et `0x031B` | IEN1 |
| `0x9156` | coupe EUART0, sous condition XDATA `0x031B == 0` | IEN1 |

Appelants de l'init `0xB1C2` : `main` (`0x9143`) et les **deux** chemins de réveil (`0x7E85`,
`0x9EBF`). La liaison est donc entièrement reconfigurée à chaque sortie de veille.

`XDATA 0x0319` (valeurs 1/2/3) et `XDATA 0x031B` gouvernent l'activation : ce sont selon toute
vraisemblance le **sélecteur de mode sans-fil** (BT1/BT2/BT3 ou BT/2.4G/USB), mais leur
sémantique **n'est pas résolue**.

> **Piège de méthode.** Trois « accès à `SBUF` » — `0xB1ED`, `0xECD0`, `0x7E62` — sont des
> **faux positifs** : ce sont les octets d'opérande de `lcall 0xABAA` (`12 ab aa`), que le scan
> octet lit comme `mov r3, 0xAA`. Tout balayage SFR sur cette image retombe dans ce piège.

#### Le vecteur — et la correction qu'il impose

`_INT_USB = 7`, et le firmware place bien `ljmp isr.usb` en `0x03 + 8×7 = 0x3B`. Le mapping
index → vecteur est donc confirmé, et il **n'est pas** celui du 8051 classique :

| Vec | n | Source | Cible |
| --- | --- | --- | --- |
| `0x03` | 0 | **TIMER2** | `0xA4D7` |
| `0x0B` | 1 | INT4 | `0xEF94` |
| `0x13` | 2 | INT3 | `0xEF9A` |
| `0x1B` | 3 | INT2 | `0xEFA0` |
| `0x3B` | 7 | USB | `0xAC60` |
| `0x43` | 8 | PWM0 | `0x72BA` |
| `0x4B` | 9 | PWM1 | `0xEE12` |
| `0x53` | 10 | PWM2 | `0xEE26` |
| `0x63` | 12 | PWM4 | `0x816B` |
| `0x6B` | 13 | **EUART0** | **`0xA544`** |

`0x23` (SCM), `0x2B` (LPD), `0x33` (SPI), `0x5B` (PWM3) et `0x7B` ne sont pas utilisés : le
firmware y a laissé du code ou des données.

⚠️ Les étiquettes `vec.int0` / `vec.timer0` / `vec.int1` / `vec.timer1` et `isr.int0 @ 0xA4D7`
du harnais `tools/f75_r2.py` reprenaient les noms 8051 classiques — **faux sur ce MCU**.
Corrigées dans le même commit que cette section.

#### Débit — la formule, pas une supposition

`0xB1C2` écrit `SBRTH = 0xFF`, `SBRTL = 0xFB`, `SFINE = 0x0C`. `SBRTH.7` est `SBRTEN`, donc
`SBRT = 0x7FFB = 32763`. La datasheet donne, pour les modes 1 et 3 :

```
BaudRate = Fsys / (16 × (32768 − SBRT) + SFINE)
         = Fsys / (16 × 5 + 12)
         = Fsys / 92
```

`Fsys` est établi par deux sources concordantes :

- le firmware écrit `PLLCON = 0x03` (PLLON + PLLFS) et `CLKCON = 0x0C` (HFON + FS, avec
  `CLKS[1:0] = 00`, donc aucune division). Datasheet : PLLFS = 1 → « PLL 的二分频作为
  OSCSCLK », et le schéma d'horloge donne **PLL = 48 MHz** → OSCSCLK = 24 MHz ;
- `meson.build` de SMK : `'sh68f90' : { 'freq_sys' : 24000000 }`.

**Débit = 24 000 000 / 92 ≈ 260 870 bauds.** Ce n'est aucun débit normalisé, et il n'y a pas
de valeur nominale à en déduire : la liaison est propriétaire des deux côtés.

#### `SCON = 0x50` + `SSTAT` — les trois bits hauts sont des drapeaux d'erreur

`0xB1CE` écrit `SCON = 0x50` → `SM1 = 1`, `REN = 1` : **mode 1, 8 bits, 1 stop**. Puis `0xB1DE`
fait `orl PCON, #0x40`, c'est-à-dire **`SSTAT = 1`**. La datasheet est explicite :

> SSTAT — 0：SCON[7:5] 工作方式作为 SM0，SM1，SM2 ／ 1：访问状态位（FE，RXOV，TXCOL）

Avec `SSTAT = 1`, `SCON[7:5]` ne sont donc plus les bits de mode mais **FE** (erreur de trame),
**RXOV** (débordement en réception) et **TXCOL** (collision d'émission) — et ils « 只能通过软件
清零 », ne peuvent être effacés que par logiciel. C'est très exactement ce que fait la queue de
l'ISR, dont le sens était autrement incompréhensible :

```asm
mov a, SCON
anl a, #0xE0        ; FE | RXOV | TXCOL
jz  fin
anl SCON, #0x1F     ; efface les trois drapeaux d'erreur
```

#### L'ISR `0xA544` — plan mémoire IDATA

```asm
; --- reception ---
jnb  SCON.RI, tx
clr  SCON.RI
mov  r0,#0x70 ; mov a,@r0     ; index d'ecriture RX
subb a,#0x17  ; jnc fin       ; buffer plein (23 o) -> octet jete
mov  a,#0x54  ; add a,r7      ; buf[idx]
mov  @r0, SBUF
inc  @r0                      ; idx++
setb 0x24.4                   ; « octet recu »

; --- emission ---
tx: jnb SCON.TI, fin
clr  SCON.TI
mov  r1,#0x71 ; mov a,@r1     ; index TX
mov  r0,#0x75 ; subb a,@r0    ; compare a la longueur-1
jc   suite
clr  0x2C.1                   ; fin d'emission
anl  P0CR, #0xFB              ; P0.2 repasse en entree
setb P0.2                     ; ... et remonte
sjmp fin
suite: inc @r0 ; mov a,@r0 ; add a,#0x33 ; mov SBUF, [a]
```

| IDATA | Rôle |
| --- | --- |
| `0x33`–`0x52` | tampon **TX** (32 o max, s'arrête juste avant le tampon RX) |
| `0x54`–`0x6A` | tampon **RX**, 23 octets — garde `idx ≥ 0x17` |
| `0x70` | index d'écriture RX |
| `0x71` | index d'émission TX |
| `0x74` | `r7` du dernier envoi (étiquette de trame) |
| `0x75` | longueur TX **− 1** |

Les 23 octets du tampon RX correspondent exactement aux 23 octets que `fcn.000005EA` recopie
vers XDATA `0x0120` — recoupement indépendant du plan mémoire.

#### `P0.2` — le handshake, mécanisme désormais prouvé

Le rôle de `P0.2` était affirmé sans preuve (et d'abord décrit à tort comme une ligne de
direction half-duplex). Les deux extrémités de la séquence le tranchent :

| Moment | Code | Effet |
| --- | --- | --- |
| début d'émission (`0xAB13`, `0xACEA`) | `clr P0.2` ; `orl P0CR,#0x04` ; `clr P0.2` | `P0.2` passe **en sortie** et est tiré **bas** |
| fin d'émission (ISR, `0xA580`) | `anl P0CR,#0xFB` ; `setb P0.2` | `P0.2` repasse **en entrée**, relâché **haut** |

Datasheet, registre `P0CR` (E1H) : « 0：输入模式 » — donc 1 = sortie. `P0.2` est bien une ligne
de **requête d'émission / réveil** vers le BK3632, maintenue basse pendant toute la rafale.

#### Somme de contrôle — même constante en émission et en réception

`0xAB0F` accumule les octets `0x33 .. 0x33+len−2` puis écrit `0x55 − Σ` comme **dernier** octet
de la trame (`0xAB2E`). C'est la **même constante `0x55`** que celle déjà relevée sur les
réponses reçues — recoupement des deux sens de la liaison.

#### Les deux émetteurs

- **`fcn.0000AB0F`** — générique. Entrée : `r5` = longueur totale, trame déjà composée en IDATA
  `0x33`. Il calcule la somme, la place en dernier, remet les index à zéro et amorce en écrivant
  `SBUF = IDATA[0x33]` ; l'ISR envoie le reste. Cinq appelants (`0x46C2`, `0x4730`, `0xA376`,
  `0xB0B8`, `0xED53`).
- **`fcn.0000ACE6`** — trame fixe de 6 octets `01 <r7> <r5> 00 00 <somme>`. Ses deux seuls
  appelants sont **dans le parseur `fcn.000005EA`** (`0x078A`, `0x0811`) : c'est donc la voie de
  réponse. *Lecture inférée* : `0x01` en tête sert ici d'accusé de réception court, par
  opposition aux réponses de 10 octets commençant par `0x02`.

#### Chaîne complète du drapeau de réception

```
ISR EUART0  (0xA544, vec 0x6B)   setb 0x24.4          « octet recu »
ISR PWM0    (0x72BA, vec 0x43)   0x24.4 -> 0x2D.7     relais, en 0x7444
ISR TIMER2  (0xA4D7, vec 0x03)   lcall 0x05EA @ 0xA519
fcn.000005EA                     IDATA 0x54 (23 o) -> XDATA 0x0120
```

Le parseur ne tourne donc **pas** dans la boucle principale mais dans l'**ISR TIMER2**.

### Le raccord EUART0 ↔ pile HID

Les rapports partent vers **deux destinations exclusives**, et l'aiguillage tient en un bit.

```
evenement touche
  |
  +-- fcn.00003108  /  fcn.000068E8       les deux ordonnanceurs
        |
        +-- fcn.000044F6 --> euart0.send  --------------------> RADIO
        |     gardes : 0x2A.4, 0x27.0, 0x2A.6, puis P4.7
        |
        +-- fcn.0000EF7B
              jb 0x2D.4, ret              <== l'interrupteur
              lcall fcn.0000AA79
              lcall fcn.00006ACF --> 0x6C07 --> EP2_IN_BUF ----> USB
```

Dans `fcn.000068E8`, les deux appels se suivent : `lcall fcn.000044F6` en `0x69C2`, puis
`lcall fcn.0000EF7B` en `0x69C7`. Chaque voie porte sa propre garde ; ce n'est pas un `if/else`
mais deux chemins gardés indépendamment.

#### `0x2D.4` — le bit qui coupe l'USB

> **Correction.** J'ai d'abord présenté `0x2D.4` comme « l'interrupteur radio/USB ». C'est exact
> quant à son **effet**, faux quant à sa **cause** : il n'est pas posé par un choix de mode mais
> par une hystérésis sur une grandeur analogique reçue de la radio (voir `fcn.00001DC3` plus
> bas). Le sélecteur de mode, lui, est plutôt `XRAM 0x031B`.

`fcn.0000EF7B` tient en dix octets :

```asm
jb    0x2d.4, ret        ; radio active -> aucun rapport USB
lcall fcn.0000aa79
lcall fcn.00006acf       ; dispatcher HID -> USB
```

Neuf autres fonctions le lisent (`0x0F77`, `0x1632`, `0x1F14`, `0x1F5B`, `0x1FF7`, `0x5995`,
`0x68EC`, `0x9A82`). Il n'est écrit **que dans `fcn.00001DC3`** : `setb` en `0x1F25`, `clr` en
`0x1EF5` et `0x1F50`. Un seul propriétaire pour l'état, ce qui en fait le point d'observation
naturel du mode de sortie.

#### La voie USB : `fcn.00006ACF` → `0x6C07`

`fcn.00006ACF` balaie les drapeaux de rapport en attente par priorité décroissante (`0x27.4`,
`0x2A.6`, `0x29.0`, …). Pour chacun il pose trois choses puis saute en `0x6C07` :

| XRAM | Rôle |
| --- | --- |
| `0x0F0C` | index du rapport |
| `0x0F0F`–`0x0F11` | pointeur générique vers le tampon source |
| tampon`[0]` | **Report ID** = `0x0F0C` + 1 |

`0x6C07` fait le reste :

```asm
movx [0x0f49] = 0x11 ; [0x0f4a] = 0x80    ; destination = 0x1180
movx [0x0f0b] = 0                          ; compteur
boucle:
  a = [0x0f0c] ; mov dptr,#0x6045 ; movc a,@a+dptr   ; longueur du rapport
  ...copie octet a octet du pointeur generique vers 0x1180 + i...
mov  IEP2CNT, a        ; SFR 0x9D
orl  EP2CON, #0x04     ; SFR 0x9A  -> arme l'endpoint
```

**`0x1180` est `EP2_IN_BUF`** (`_SBUF(0x1180) EP2_IN_BUF[64]` dans l'en-tête SMK). `0x6C07` est
donc purement USB : il ne contient aucune branche radio.

La table `0x6045` donne la longueur totale par rapport :

| index `0x0F0C` | 0 | 1 | 2 | 3 | 4 | 5 |
| --- | --- | --- | --- | --- | --- | --- |
| longueur | 3 | 2 | **4** | 16 | 8 | 8 |

L'index 2 vaut 4 octets — ce qui recoupe exactement le rapport de `0xA4BD`, qui écrit `0x08BF`
(Report ID 3) plus `0x08C0`–`0x08C2` : quatre octets, ni plus ni moins.

#### La voie radio : `fcn.000044F6`

Elle lit un index en `XRAM 0x0307`, adresse un enregistrement de **28 octets** en
`0x0C57 + index × 28`, et émet deux tailles de trame :

| Site | charge copiée | `r5` (trame) |
| --- | --- | --- |
| `0x46C2` | 28 o (`fcn.00004C4B`, `r7 = 0x1C`) | **30** |
| `0x4730` | 11 o (`r7 = 0x0B`) | **13** |

C'est la seule des cinq entrées de `euart0.send` qui soit appelée depuis la chaîne de rapport.
Les quatre autres portent de la configuration, pas des frappes :

| Appelant | Trame | Contenu |
| --- | --- | --- |
| `fcn.0000A307` @ `0xA376` | 32 o | commande `0x09`, nom Bluetooth |
| `fcn.0000B093` @ `0xB0B8` | 23 o | commande `0x08`, charge de 19 o |
| `fcn.0000ED3B` @ `0xED53` | 6 o | commande `0x06`, `01 06 00 00 00 <ck>` |

> Précision à une note antérieure : dans `euart0.send`, **`r7` n'est pas l'octet de commande** —
> il est rangé en IDATA `0x74` comme étiquette. L'octet de commande est l'octet 1 de la trame,
> composé par l'appelant. `fcn.0000ED3B` le montre : `r7 = 0x0A` mais la trame porte `0x06`.

#### `P4.7` — la seconde ligne de handshake, en entrée

Toute émission est précédée de `jnb 0xb0.7, sortie` (SFR `0xB0` = `P4`). Onze sites le testent :
`0x44F6`, `0xB093`, `0xB06A`, `0xB0BC`, `0xED3B`, `0xED89`, `0xEC85`, `0xEC68`, plus
`isr.pwm0` (`0x743D`), `fcn.0000801F` (`0x8152`) et `fcn.0000AE50` (`0xAE5B`).

**Aucun `setb` ni `clr` sur `0xB0.7` dans toute l'image** — le firmware ne l'écrit jamais. C'est
donc une **entrée**, pilotée par le BK3632. Avec `P0.2` en sortie (requête d'émission), la
liaison compte **deux lignes de handshake**, et `P4.7` n'apparaît dans aucune colonne de la
matrice — cohérent.

`fcn.0000B093` cumule les deux gardes, ce qui donne la forme canonique d'un émetteur :

```asm
jb  0x2c.1, ret     ; une emission est deja en cours
jnb 0xb0.7, ret     ; le module radio n'est pas pret
```

### Les trois fonctions du sous-système : `1DC3`, `44F6`, `6ACF`

| Fonction | Taille | Blocs | Rôle |
| --- | --- | --- | --- |
| `fcn.00001DC3` | 2700 o | 219 | gestion d'énergie — **seul écrivain de `0x2D.4`** |
| `fcn.000044F6` | 844 o | 53 | émetteur de rapports vers la radio |
| `fcn.00006ACF` | 460 o | 33 | dispatcher de rapports vers l'USB |

#### `fcn.00001DC3` — une hystérésis, pas un sélecteur de mode

Appelée depuis un seul site (`fcn.00008FA7` @ `0x8FE6`). Ses gardes d'entrée mènent à quatre
sous-chemins ; celui qui pilote `0x2D.4` est atteint quand `0x26.0` est **clair** :

```asm
0x1f05  ; compare 16 bits [0x02E6]:[0x02E7] a 0x02E1 = 737
        jc 0x1f17                       ; valeur < 737
0x1f14  jnb 0x2d.4, 0x1f2e              ; sinon, remet le compteur a zero
0x1f17  [0x097b]++ ; si >= 0xC8 (200) :
0x1f25     setb 0x2d.4                  ; <== bascule
0x1f27     setb 0x29.1 ; lcall fcn.0000EE6A

0x1f33  ; compare la meme valeur a 0x0390 = 912
0x1f43  [0x097f]++ ; si >= 0x64 (100) :
0x1f50     clr 0x2d.4                   ; <== retour
0x1f52     clr 0x2b.3
```

Deux seuils, deux compteurs anti-rebond, une bande morte de 737 à 912 : c'est une **hystérésis**,
pas une commutation.

**D'où vient la grandeur mesurée.** `XRAM 0x02E6:0x02E7` est écrit par **`euart0.parse`**
(`0x0677`, `0x0683`, `0x0691`) depuis l'octet en `XRAM 0x0127` — l'**offset 7 de la trame reçue**
recopiée en `0x0120` — et initialisé à `0x03xx` par `vec.reset` (`0x9136`). La valeur vient donc
du **BK3632**, pas du 8051.

Ce qui est cohérent avec le silicium : le SH68F90 **n'a pas d'ADC**. La datasheet ne lui donne
qu'un détecteur de sous-tension (`LPDCON` 0xB3, `LPDSEL` 0x89). Une mesure analogique ne peut
venir que de la radio.

`fcn.00001DC3` compare cette valeur à **quatre seuils** échelonnés :

| Site | Seuil | Décimal |
| --- | --- | --- |
| `0x1F06` | `0x02E1` | 737 |
| `0x23D8` | `0x030D` | 781 |
| `0x2329` | `0x034E` | 846 |
| `0x227A` | `0x0390` | 912 |

Une échelle monotone dans une plage compatible avec une conversion **sur 10 bits** (0–1023),
lue par le BK3632 et transmise sur EUART0.

> *Inféré :* qu'il s'agisse de la **tension de batterie** et que les quatre seuils soient les
> paliers d'une jauge. Ce qui est établi : la provenance (trame EUART0, octet 7), l'échelle de
> seuils, l'hystérésis, et le fait que `0x2D.4` n'a qu'un seul écrivain.

#### `fcn.00006ACF` — deux gardes, puis six emplacements

```asm
mov a, EP2CON            ; SFR 0x9A
jnb ACC.2, suite         ; si l'endpoint est encore arme -> sortir
ljmp sortie
suite:
a = [0x031b] ; jz chaine ; ljmp sortie   ; ne rien emettre si 0x031B != 0
```

`0x031B` non nul suffit à couper toute émission USB — c'est un gate plus direct que `0x2D.4`,
et c'est la même variable qui gouverne les activations d'EUART0 en `0x41F2`/`0x4227`/`0x425D`.

Vient ensuite une **chaîne de priorité décroissante**. Chaque maillon acquitte son drapeau,
pose `0x26.7`, écrit l'index, le Report ID en tête de tampon et un pointeur générique, puis
saute au copieur :

| Ordre | Drapeau | `0x0F0C` | Tampon | Report ID | Longueur |
| --- | --- | --- | --- | --- | --- |
| 1 | `0x2A.0` | 0 | `0x09BC` | 2 | 3 |
| 2 | `0x27.3` | 1 | `0x097D` | 1 | 2 |
| 3 | `0x27.4` | 2 | `0x08BF` | 3 | 4 |
| 4 | `0x2A.6` | 3 | `0x0980` | 4 | 16 |
| 5 | `0x29.0` | 4 | `0x09B0` | 7 | 8 |
| 6 | `0x2B.2` | 5 | `0x0095` | 6 | 8 |

**Six maillons — et la table de longueurs `0x6045` a exactement six entrées valides**
(`03 02 04 10 08 08`) avant son `0xFF` de fin. Les deux se confirment mutuellement, ce qui
valide au passage la lecture de la table.

Noter que le Report ID ne suit pas l'index : l'index 0 porte l'ID 2, l'index 1 porte l'ID 1.
Les maillons 3 et 4 le calculent par `inc a`, les autres l'écrivent en dur.

#### `fcn.000044F6` — deux moitiés symétriques, une sortie commune

```
0x44F6  jb 0x2A.4 / jb 0x27.0 / jb 0x2A.6  -> moitie A (0x4502)
                                   sinon   -> moitie B (0x45A2)

moitie A (0x4502-0x45A2)   index [0x0307], enregistrement 0x0C57 + n x 28
moitie B (0x45A2-0x4652)   trois gardes puis corps parallele

0x4652  convergence
0x4675    jb 0xB0.7          <-- P4.7 : le module radio doit etre pret
0x467B    ... -> euart0.send  r5 = 30   (charge de 28 o)
0x46EB    ... -> euart0.send  r5 = 13   (charge de 11 o)
```

Les deux moitiés sont structurellement identiques (blocs de 136 et 143 octets) et convergent sur
un unique bloc d'émission. C'est la seule des cinq entrées de `euart0.send` qui soit atteinte
depuis la chaîne de rapport.

### `XRAM 0x031B` — le transport actif

**57 références** dans l'image : c'est la variable centrale du sous-système sans-fil. Trois
valeurs seulement, `0`, `1` et `2`.

#### Ce que la valeur commande

| Site | Test | Effet quand `0x031B == 0` |
| --- | --- | --- |
| `usb.init` @ `0xEC00` | `jnz` | l'USB n'est initialisé (`USBADDR`) **que** dans ce cas |
| `fcn.00006ACF` @ `0x6AD7` | `jz` | aucun rapport HID USB si la valeur est non nulle |
| `isr.timer2` @ `0xA513` | `jz` | `euart0.parse` n'est appelé **que** si elle est non nulle |
| `vec.reset` @ `0x9150` | `jnz` | sinon `anl IEN1,#0xBF` — l'IRQ EUART0 reste coupée |
| `fcn.00001DC3` @ `0x1EFC` | `jnz` | l'hystérésis d'énergie ne tourne qu'en sans-fil |
| `fcn.00003108` @ `0x310E` | `cjne #0x02` | en mode 2, un élément de plus à balayer (`inc 0x0E`) |

> Précision à la section précédente : l'appel à `euart0.parse` dans `isr.timer2` a **deux**
> gardes, pas une — `jb 0x2c.0` en `0xA504` et `jz` sur `0x031B` en `0xA513`, toutes deux vers
> la même sortie `0xA51C`.

#### Le retour au filaire — `fcn.000084E9` @ `0x8543`

```asm
clr   a
movx  [0x031b], a       ; transport = 0
anl   IEN1, #0xBF       ; coupe l'IRQ EUART0
lcall usb.init          ; reinitialise l'USB
```

#### Le passage en Bluetooth — trois blocs identiques

`0x41D4`, `0x420A`, `0x423F`, au numéro de slot près :

```asm
jnb 0x29.5, abandon ; jnb 0x2a.5, abandon
a = [0x031B] ; xrl #0x02 ; jz suite     ; exige d'etre DEJA en mode 2
a = [0x0319] ; xrl #0x0N ; jz fin       ; deja sur ce slot -> ne rien faire
[0x031B] = 2
[0x0319] = N            ; dec a / movx a / inc a, avec a = 2  ->  1, 2, 3
orl  IEN1, #0x40        ; active l'IRQ EUART0
anl  USBCON, #0x7F      ; coupe l'USB
setb 0x27.7 ; setb 0x26.5
```

Le numéro de slot est produit par la seule différence entre les trois blocs : `dec a`, `movx a`,
`inc a` appliqués à `a = 2`. Donc **`XRAM 0x0319` est le slot Bluetooth, 1 à 3**, et ces trois
blocs sont les raccourcis de *changement de slot* — ils exigent le mode 2 et n'y font pas entrer.

La symétrie avec le retour filaire est exacte : `IEN1.6` activé contre coupé, `USBCON.7` coupé
contre `usb.init`.

#### Mode 1

`0x4276` exige `[0x031B] == 1`, remet à zéro un compteur 16 bits en `0x0961` et pose `0x2C.5` —
un déclencheur, pas une entrée de mode. `euart0.parse` (`0x070E`) et `fcn.000084E9` (`0x8576`)
traitent le mode 1 distinctement du mode 2, chacun avec son propre `xrl`.

> *Inféré :* 0 = USB filaire, 1 = 2,4 GHz, 2 = Bluetooth — c'est la structure tri-mode habituelle
> de ces claviers. *Établi :* trois valeurs ; le mode 2 porte un slot 1–3 ; le mode 0 est celui où
> l'USB est initialisé et où le sans-fil est intégralement coupé.

#### Résolu : `0x031B` est un champ d'un bloc de réglages persisté

Le point resté ouvert — « aucune écriture directe de la valeur 1 » — a une explication nette :
`0x031B` n'est pas une variable isolée, c'est **l'octet 14 d'un bloc de 128 octets** qui vit à
trois endroits.

```
flash page 99 (0xC600, 128 o)
      |   fcn.0000A611     lecteur flash generique
      v                    base 0x0ED6:7, dest 0x0ED8:9, compte 0x0EDA:B, movc @a+dptr
XRAM 0x09BF   bloc de travail, 128 o
      |   fn.fx_init  @ 0xA2BB-0xA2D1     [0x09BF + i] -> [0x030D + i]
      v
XRAM 0x030D   bloc VIF, 128 o
```

et le retour :

```
XRAM 0x030D --fcn.0000A069 @ 0xA083/0xA0C2--> XRAM 0x09BF --iap.save--> flash page 99
```

`fn.fx_init` est appelée deux fois par `vec.reset` (`0x9110`, `0x9174`) : la restauration a bien
lieu au démarrage. Et juste avant de sauvegarder, `fcn.0000A069` rafraîchit explicitement les
deux champs :

```asm
[0x09CB] = [0x0319]      ; slot Bluetooth     (offset 12)
[0x09CD] = [0x031B]      ; transport actif    (offset 14)
mov r5,#0x63 ; r6,#0xC6  ; page 99 = 0xC600
lcall iap.save
```

L'arithmétique ferme la boucle : `0x030D + 12 = 0x0319`, `0x030D + 14 = 0x031B`, et
`0x09BF + 12 = 0x09CB`, `0x09BF + 14 = 0x09CD`.

**Pourquoi la recherche ne voyait rien.** Le bloc vif est adressé **par pointeur calculé**
(`mov a,#0x0d ; add a,r7 ; mov DPL,a ; mov a,#0x03 ; addc a,r6 ; mov DPH,a`) depuis quatre sites,
et le bloc de travail depuis une trentaine. Une routine générique « écrire l'octet de config N »
pose donc `0x031B` sans jamais émettre `mov dptr, #0x031b`. La valeur 1 arrive par là, ou par la
restauration flash — pas par une constante en dur.

#### Vérification sur le dump

Bloc lu à `0xC600` dans `assets/f75_firmware.bin` :

```
0xC600  00 03 03 02 00 00 04 04 07 00 04 20 01 00 00 00
0xC610  00 00 00 00 02 01 00 ff 02 00 00 00 01 00 03 01
...
0xC670  09 37 09 37 04 09 04 04 04 04 04 04 04 04 5a a5
```

| Offset | XRAM vif | Valeur | Rôle |
| --- | --- | --- | --- |
| 12 | `0x0319` | **1** | slot Bluetooth |
| 14 | `0x031B` | **0** | transport actif — **filaire** |
| 126–127 | `0x038B`–`0x038C` | `5A A5` | marqueur de validité |

`0x031B = 0` : le clavier était en **mode filaire** au moment du dump — ce qui est exactement le
cas, `sinowisp` ayant lu par l'USB. La chaîne complète flash → `0x09BF` → `0x030D` est confirmée
de bout en bout par une valeur observable.

Le `5A A5` en queue de bloc est la signature classique d'un marqueur de validité : il permet au
firmware de distinguer un bloc écrit d'une page effacée.

> Cette page 99 est la même que celle sauvegardée par `fcn.0000A069`, et elle figurait déjà
> parmi les sept pages non vierges relevées à l'analyse d'entropie de la zone IAP. Les deux
> observations, faites indépendamment, se rejoignent.

#### Cartographie du bloc

La longueur est confirmée des deux côtés : `fn.fx_init` (`0xA292`) et `fcn.0000A5AD` (`0xA5B5`)
appellent `fcn.0000A611` avec les mêmes arguments — `r4:r5 = 0x09BF`, `r3:r2 = 0x0080`,
`r6:r7 = 0xC600`. **128 octets**, dans les deux sens.

**22 offsets sur 128 sont adressés directement** par `mov dptr, #0x03NN`, tous entre 0 et 32.
Le reste n'est atteint que par pointeur calculé.

| Off | XRAM | Dump | Refs | Ce que le firmware en fait |
| --- | --- | --- | --- | --- |
| 0 | `0x030D` | `00` | 20 | lu très largement |
| 1 | `0x030E` | `03` | 4 | compteur décrémenté puis testé (`0x88D9`, `0x88E7`) |
| 3 | `0x0310` | `02` | 1 | `fcn.00003108` : borne de balayage → `IDATA 0x0E` |
| 5 | `0x0312` | `00` | 6 | drapeau (`jz`) |
| 6 | `0x0313` | `04` | 4 | recopié vers `0x0D19` |
| 7 | `0x0314` | `04` | 2 | recopié vers `0x0D9D` |
| 8 | `0x0315` | `07` | 4 | champ compacté (`anl #0x0F`, `anl #0x80`) |
| 9 | `0x0316` | `00` | 13 | **source de `XRAM 0x009D`** — l'effet RGB |
| 10 | `0x0317` | `04` | 11 | plage 0–14 (`cjne a, #0x0E`) |
| 11 | `0x0318` | `20` | 3 | apparié à l'offset 10 dans `fn.fx_init` et `fcn.0000A069` |
| **12** | `0x0319` | `01` | 16 | **slot Bluetooth** |
| **14** | `0x031B` | `00` | 57 | **transport actif** |
| 15 | `0x031C` | `00` | 2 | écrit en `0x4197`, lu par `fcn.00008FA7` |
| 16 | `0x031D` | `00` | 2 | idem, `0x41AC` / `0x8FDA` |
| 17 | `0x031E` | `00` | 1 | lu par `fcn.00008FA7` |
| 18 | `0x031F` | `00` | 3 | reçoit `0x0D16` quand la classe d'action vaut 9 |
| 22 | `0x0323` | `00` | 3 | drapeau |
| 24 | `0x0325` | `02` | 1 | lu par `fcn.00008FA7` |
| 26 | `0x0327` | `00` | 8 | drapeau |
| 27 | `0x0328` | `00` | 23 | testé `cjne a, #0x01` |
| 28 | `0x0329` | `01` | 1 | compteur cyclique modulo 4 (`0x84C6`) |
| 32 | `0x032D` | `00` | 2 | |

Les offsets 2, 4, 13, 19–21, 23, 25, 29–31 n'ont aucune référence directe et valent 0 dans le
dump : réservés, ou atteints uniquement par le protocole de configuration.

#### Deux tableaux, adressés par pointeur calculé

Une recherche du motif `mov DPL,a ; mov a,#0x03 ; addc a,rX ; mov DPH,a` donne les bases :

| Base | Offset | Pas | Index vérifié |
| --- | --- | --- | --- |
| `0x0345` / `0x0346` | 56 / 57 | **2** | `[0x009D] × 2` en `0x1166`, `0x8F5F`, `0x93F0` |
| `0x0362` | 85 | 1 | `[0x009D]` en `0x114E` |

Le tableau à pas 2 est un **enregistrement de deux octets par effet**, dont les champs sont
compactés :

```asm
0x1170  movx a,@dptr ; rlc a ; mov 0x24.3, c   ; bit 7 de l'octet 0 -> drapeau 0x24.3
0x117E  movx a,@dptr ; anl a,#0x0f             ; quartet bas de l'octet 1 -> 0x011C
0x8F6A  movx a,@dptr ; anl a,#0x8f             ; bits 7 et 3-0 de l'octet 1
```

Et la queue du dump montre exactement cette régularité par paires :

```
off 56   ff ff
off 58   09 34 | 09 37 | 09 37 | 09 37 | 09 37 | 09 37
off 70   00 34 | 09 37 x13
off 98   07 47 | 07 47 | 07 44 x8
off 116  04 09 | 04 04 x5
off 126  5a a5
```

#### Résolution : deux familles d'effets, et un sélecteur

La tension venait d'une hypothèse fausse de ma part — que `XRAM 0x009D` soit uniformément
`0x20`-basé. Il ne l'est pas. `fn.fx_init` (`0xA2EC`) et `fcn.0000A069` (`0xA0D9`) portent le
même sélecteur, à l'identique :

```asm
mov dptr,#0x0316 ; movx a,@dptr    ; offset 9
setb c ; subb a,#0x00              ; carry ssi [0x0316] == 0
jc  prendre_0317
mov dptr,#0x0318                   ; [0x0316] != 0  ->  offset 11
sjmp ecrire
prendre_0317:
mov dptr,#0x0317                   ; [0x0316] == 0  ->  offset 10
ecrire:
movx a,@dptr ; movx [0x009d], a
```

Il y a donc **deux familles d'effets**, et l'offset 9 dit laquelle est active :

| Offset | Rôle | Plage | Dump |
| --- | --- | --- | --- |
| 9 | sélecteur de famille | 0 / ≠0 | `0x00` |
| 10 | effet **famille A** | 0–14 (`cjne a, #0x0E`) | `0x04` |
| 11 | effet **famille B** | `0x20`+ | `0x20` |

Sur le clavier dumpé, le sélecteur vaut 0 → **`XRAM 0x009D = 4`, famille A**. C'est ce qui
manquait : les valeurs `0x20` / `0x26` / `0x2D` que j'avais relevées appartiennent toutes à la
famille B, et m'avaient fait généraliser à tort.

#### Le tableau se recale exactement

Avec l'index `[0x009D] × 2` sur la base `0x0345` :

| Famille | Index | Offsets couverts | |
| --- | --- | --- | --- |
| A | 0 – 14 | **56 – 85** | entièrement **dans** le bloc |
| B | `0x20` – `0x2F` | 120 – 151 | **déborde** les 128 octets |

La famille A couvre précisément la zone structurée du dump (offsets 58–85), et c'est elle qui est
persistée. Les enregistrements de la famille B sortent du bloc et ne vivent qu'en XRAM.

Décodage de l'enregistrement de l'effet courant (index 4 → offset **64**, octets `09 37`) :

| Champ | Valeur | Destination |
| --- | --- | --- |
| `b0 & 0x1F` | 9 | `XRAM 0x0D19` (`0x1197`) |
| `b0` bit 7 | 0 | drapeau `0x24.3` (`0x1170`) |
| `b1 & 0x0F` | 7 | `XRAM 0x011C` (`0x117E`) |
| `b1 & 0x8F` | `0x07` | `0x8F6A` |

Deux paramètres par effet, sur 5 et 4 bits, plus deux bits de drapeau — la forme attendue d'un
couple *vitesse / luminosité* avec des indicateurs, même si les noms restent à confirmer.

#### `0x0D19` et `0x011C` — vitesse et couleur

Les deux destinations de l'enregistrement par effet sont identifiées.

##### `XRAM 0x0D19` — la vitesse (27 références)

`fcn.0000A0F0` — appelée depuis douze sites de rendu — s'en sert comme index :

```asm
mov dptr,#0x0d19 ; movx a,@dptr
mov dptr,#0x2937 ; movc a,@a+dptr    ; gain = table[vitesse]
movx [0x0ef1], a ; mov r5, a
... lcall fcn.0000EF66   (r6:r7 = r7 x r5, multiplication 8x8 -> 16)
... lcall fcn.00000056   avec r5 = 0x50 = 80  (division)
```

Table `0x2937` :

```
00 08 10 18 20 28 32 3c 46 50 | ff
 0  8 16 24 32 40 50 60 70 80   fin
```

**Dix entrées, indices 0 à 9** — ce qui recoupe exactement la borne `subb a, #0x09` relevée en
`0x7A18` et `0x7A86`. Le gain va de 0 à 80, et la division par 80 en fait un facteur de 0 à 1
appliqué à trois grandeurs (`0x0000`, `0x011D`, `0x0E25` → `0x0E46`, `0x0E42`, `0x0E3F`).

Le maximum est **par effet**, lu dans une table CODE :

```asm
0x8C30  mov dptr,#0x009d ; movx a,@dptr
0x8C34  mov dptr,#0xa8d5 ; movc a,@a+dptr   ; borne = CODE[0xA8D5 + effet]
0x8C39  ... compare et sature 0x0D19
```

Valeur par défaut **4** (`0x1D10`, `0x1D31`). Valeur dans le dump : **9**, soit le maximum.

##### `XRAM 0x011C` — le mode de couleur (30 références)

Six fonctions de rendu — `fcn.00004EA9`, `0x5114`, `0x60C9`, `0x6C9B`, `0x7108`, `0x746A`, plus
`0xA791` — commencent toutes par le même test :

```asm
mov dptr,#0x011c ; movx a,@dptr ; mov r7,a
xrl a,#0x07 ; jz  saute            ; 0x011C == 7 -> on court-circuite
mov dptr,#0x0896 ; movx a,@dptr
mov 0xf0,#0x15 ; mul ab            ; x 21
add a,#0x00 ; mov DPL,a
mov a,0xf0 ; addc a,#0xc8 ; mov DPH,a   ; 0xC800 + n x 21
mov 0xf0,#0x03 ; mov a,r7 ; lcall fcn.00004E39   ; x 3
```

**21 octets = sept triplets RGB**, et l'index dans l'enregistrement est `0x011C × 3`. La table
est en **flash page 100 (`0xC800`)**, dans la zone IAP. Le dump donne, à l'identique pour chaque
profil :

| Index | Couleur |
| --- | --- |
| 0 | `#FF0000` rouge |
| 1 | `#00FF00` vert |
| 2 | `#0000FF` bleu |
| 3 | `#FFFF00` jaune |
| 4 | `#FF00FF` magenta |
| 5 | `#00FFFF` cyan |
| 6 | `#FFFFFF` blanc |

Donc **`0x011C` de 0 à 6 sélectionne une couleur fixe de la palette, et 7 est le cas
« pas de couleur fixe »** — le mode arc-en-ciel, celui qui saute la lecture de palette. Défaut
**7** (`0x1D2B`) ; valeur dans le dump : **7**.

Son maximum est lui aussi par effet, dans `CODE[0xA8A3 + effet]` (`0x939F`, `0x93B4`).

##### Trois tables de bornes par effet

`0xA8A3`, `0xA8BC`, `0xA8D5` — espacées de `0x19` = 25 :

| Base | Borne de | Valeur famille A | Valeur famille B |
| --- | --- | --- | --- |
| `0xA8A3` | `0x011C` (couleur) | 7 | 4 |
| `0xA8BC` | *(troisième paramètre)* | 4 | — |
| `0xA8D5` | `0x0D19` (vitesse) | 9 | 9 |

Les deux affectations extrêmes sont vérifiées par désassemblage direct (`0x8C34` pour `0xA8D5`,
`0x939F` pour `0xA8A3`). Celle du milieu ne l'est pas.

##### L'enregistrement se relit entièrement

Pour l'effet courant du clavier dumpé (index 4, offset 64, octets `09 37`) :

| Bits | Valeur | Sens |
| --- | --- | --- |
| `b0[4:0]` | 9 | **vitesse**, au maximum |
| `b0[7]` | 0 | drapeau `0x24.3` |
| `b1[3:0]` | 7 | **couleur** = arc-en-ciel |
| `b1[6:4]` | 3 | non lu par les masques identifiés — candidat pour le paramètre borné à 4 |
| `b1[7]` | 0 | drapeau (masque `0x8F`) |

#### Et `5A A5`

L'argument « ce pourrait être l'enregistrement d'index 35 » s'affaiblit nettement : les
enregistrements de la famille B débordent le bloc de toute façon (l'effet `0x26`, valeur par
défaut au reset, tombe à l'offset 132), donc les deux derniers octets du bloc ne portent aucune
donnée d'effet utile. Avec en plus un motif complémentaire (`0101 1010` / `1010 0101`) en toute
fin de structure, la lecture **marqueur de validité** est la mieux étayée. Elle reste non prouvée.

### Réception : parser et format des réponses

La réception se fait en **deux étages**.

**1. L'ISR ne fait que signaler.** À la fin de l'**ISR PWM0**
(vecteur `0x43` → `0x72BA`), en `0x7444` :

```asm
jnb  0x24.4, ...   ; drapeau materiel « donnees recues », pose par l'ISR EUART0
clr  0x24.4        ; acquitte
setb 0x2d.7        ; drapeau de traitement differe
```

**2. `fcn.000005EA` traite hors interruption** (`0x05F3`) :

```asm
clr   0x2d.7                 ; acquitte
mov   r1, #0x54              ; source : buffer RX en IDATA 0x54
mov   r7, #0x17              ; 23 octets
lcall fcn.00004c4b           ; copie vers XDATA 0x0120
mov   r0,#0x70 ; mov a,@r0   ; index d'ecriture RX
```

**Les trames reçues sont donc analysées en XDATA `0x0120`.**

#### Format d'une réponse

```
[0] = 0x02        en-tete de REPONSE   (les commandes emises utilisent 0x01)
[1] = commande    ex. 0x06
[2] = statut / parametre
[3..8] = donnees
[9] = checksum
```

**Checksum vérifié** (`0x0650`) :

```asm
clr  c
mov  a, #0x55
subb a, 0x08        ; 0x55 - somme(octets 0..8)
mov  dptr,#0x0129 ; movx a,@dptr   ; octet [9]
xrl  a, 0x08 ; jz                  ; sinon la trame est rejetee
```

→ **`checksum = 0x55 − Σ(octets 0..8)`**, trames de **10 octets**.

Après validation, les champs sont dispatchés : `[4]` → XRAM `0x09BA`, `[5]` → XRAM `0x09AC`,
`[7]` → chaîne passant par `0x02E6`.

#### Plusieurs types de trames

L'octet `[0]` est dispatché sur **`0x02`, `0x03`, `0x08` et `0x15`** (`0x0612`, `0x077D`, `0x0797`,
`0x07BD`). L'octet `[2]` est ensuite testé contre `0x08`, `0x05`, `0x40`.

Pour le type `0x03` : pose `0x2C.3`, met XRAM `0x0150` à 0, appelle `fcn.0000ACE6` avec `r7 = 0xF0`,
puis charge `IDATA 0x18 = 0x06` et `IDATA 0x17 = 0xFF`.

#### 🔑 Le sans-fil pilote le RGB

Dans la branche autour de `0x07D5`-`0x07FC`, le parser lit les octets **`[3]`, `[4]`, `[5]`** de la
trame reçue, puis écrit :

```asm
0x07f0   mov dptr,#0x038e ; clr a ; movx @dptr,a    ; 0x038E = 0
0x07f5   inc dptr ; mov a,r5 ; movx @dptr,a         ; 0x038F = r5
0x07f8   mov a,r7 ; swap a ; anl a,#0x0f            ; quartet haut
0x07fc   mov dptr,#0x038d ; movx @dptr,a            ; 0x038D = sous-index d'effet
```

**`0x038D` est le sous-index d'effet RGB** (voir la chaîne de sélection plus haut). Une trame venue
du BK3632 peut donc **changer l'effet lumineux**. Autrement dit, le chemin sans-fil relaie de la
configuration vers le 8051 — vraisemblablement depuis une application hôte via Bluetooth ou le
dongle 2,4 GHz.

C'est la troisième source d'effet, à côté des touches Fn et du chemin USB.

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

#### La zone de données `0xAF6D` – `0xAF98`

Bloc entièrement délimité : le code reprend en `0xAF99`. **Quatre lecteurs**, tous identifiés par
leurs chargements DPTR.

| Adresse | Taille | Contenu | Lu par |
| --- | --- | --- | --- |
| `0xAF6D` | 16 o | `"AULA-F75 3.0 KB "` | `0xA321` — commande `0x09` |
| `0xAF7D` | 16 o | `"AULA-F75 5.0 KB "` | `0xA348` — commande `0x09` |
| `0xAF8D` | 12 o | `20, 10, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2` | `0x808C` et `0x8114` |

La table de 12 octets est une **courbe d'accélération à seuil dégressif** :

```asm
mov  B, #0x0A ; div ab      ; index = (valeur - X) / 10
mov  dptr, #0xaf8d ; movc   ; seuil = table[index]
movx a, @dptr               ; compteur (XRAM 0x08C5, ou 0x08DC pour l'autre lecteur)
clr c ; subb a, r7 ; jc     ; compteur < seuil -> sortie
clr a ; movx @dptr, a       ; sinon remise a zero
inc  0x17                   ; et avance d'un cran
```

Il faut **20 passages pour le premier cran, puis 10, puis 5, puis 2** indéfiniment. Deux compteurs
distincts (`0x08C5` et `0x08DC`) partagent la même courbe. Ce qu'elle pilote au final n'est pas
établi — il faudrait suivre les consommateurs de `IDATA 0x17`.

*Note : les octets `30 67 27 c2 67 53 a9 fe` qui suivent la table ne sont pas des données mais du
**code** — `30 67 27` désassemble en `jnb 0x2c.7, 0xafc3`, début de `fcn.0000AF99`. Ils
ressemblaient à une adresse MAC Bluetooth ; ils n'en sont pas.*

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

#### La voie ICP du BYK916 de référence — **ne s'applique PAS au F75**

Les notes de reverse [`swiftgeek/hykker-re`, issue #4](https://github.com/swiftgeek/hykker-re/issues/4)
documentent les conventions du BYK916 et signalent une piste séduisante :

> `CSNA` / `CLKA` / `MOSIA` / `MISOA` — SPI/JTAG interface for BK3632, **could be used for ICP/ISP of
> flash in BK3632**.

Sur la conception de référence, le 8051 est donc câblé au BK3632 par SPI, ce qui offrirait un accès
ICP tout fait. **Vérifié sur ce clavier : ce n'est pas le cas.**

Broches SPI par défaut du SH68F90, d'après le datasheet :

| Signal | Broche |
| --- | --- |
| `SCK` | `P7.4` |
| `SS` | `P7.3` (aussi `INT45`) |
| `MOSI` | `P7.2` (aussi `INT44`) |
| `MISO` | `P7.1` (aussi `INT41`) |

Or sur le F75, **`P7.1`, `P7.2` et `P7.3` sont les lignes de matrice R1, R2 et R3** — établi par
quatre sources concordantes (table de scan `0x72F4`, parking de veille `0x006E`, init GPIO `0xA7EC`,
routine de lecture `0x73A6`). Ces broches portent le clavier, pas un bus SPI.

Et le firmware le confirme lui-même : `fcn.0000EFA6`, appelée depuis l'init `fcn.0000ECC5`, fait

```asm
clr a ; mov SPCON, a ; mov SPSTA, a ; ret
```

soit **la désactivation explicite du périphérique SPI au démarrage**. Recherche exhaustive :
aucun autre accès à `SPCON`, `SPSTA` ou `SPDAT` dans tout le firmware.

**Conclusion : le F75 relie son BK3632 par EUART0 uniquement.** L'ICP via le SPI du 8051 est fermé —
les broches sont prises par la matrice.

*(Les broches SPI/JTAG propres au BK3632 — `GPIOA[3..7]` selon le datasheet BK3633 — existent
évidemment toujours sur la puce. Mais rien ne dit qu'elles soient routées vers des pastilles
accessibles sur ce PCB.)*

### Au passage : le câblage RGB de référence confirme l'inversion

La même issue liste le câblage RGB « de référence » du BYK916 :

```
Row0: VR0->P4_1  VG0->P6_0  VB0->P4_0
Row1: VR1->P0_4  VG1->P6_1  VB1->P0_3
Row2: VR2->P6_7  VG2->P6_2  VB2->P6_6
Row3: VR3->P0_2  VG3->P6_3  VB3->P5_7
Row4: VR4->P4_5  VG4->P6_4  VB4->P4_6
Row5: VR5->P4_3  VG5->P6_5  VB5->P4_4
```

C'est **exactement** le brochage du NuPhy Air60 (`RGB_R0R P0_4`, `RGB_R0G P6_1`, `RGB_R0B P0_3`, …),
à un décalage de ligne près. L'Air60 suit donc la conception de référence.

Le F75, lui, met ses **colonnes de matrice sur P6/P5/P4** et ses **18 canaux RGB sur les broches PWM
P1/P2/P3**. C'est la topologie inverse déjà documentée plus haut — et cette issue en fournit la
confirmation externe : le F75 s'écarte délibérément du design de référence.

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

## EEPROM émulée en flash — l'IAP

Le F75 n'a pas d'EEPROM (le `settings_store` de SMK vaut `flash` pour le `sh68f90`). Les
réglages persistants sont écrits **dans la flash de programme elle-même**, par une routine IAP
que voici de bout en bout.

### `fcn.0000AAC1` — enregistrer une page de réglages

Douze appelants, dont la grappe du parseur EUART0 (`0x77CE`, `0x77FF`, `0x7830`, `0x7861`,
`0x7892`, `0x78C3`, `0x78F4`) et `fcn.0000AF99` (publication d'un effet RGB, en `0xAFB3`) :
c'est la routine qui **rend un réglage permanent**, qu'il vienne du sans-fil ou des touches.

```asm
mov  dptr,#0x0F18 ; sauve r6, r7, r5 en 0x0F18-0x0F1A
mov  c, EA ; mov 0x2E.5, c   ; memorise l'etat de EA
clr  EA                      ; toutes interruptions coupees
lcall fcn.0000EEE3           ; relache la matrice (voir plus bas)
movx a, [0x0F1A] ; -> 0x0302 ; numero de page
lcall fcn.0000AEE4           ; efface la page
...
mov  r3,#0x01 ; r2,#0x09 ; r1,#0xBF    ; source = 0x09BF
movx [0x0F20] = 0x02, [0x0F21] = 0x00  ; longueur = 0x0200 = 512
lcall fcn.00000181           ; ecrit la page
movx [0x0302] = 0
lcall fcn.0000EDBB           ; reverrouille
mov  c, 0x2E.5 ; ...         ; restaure EA
```

### `fcn.0000AEE4` — la borne, et donc la carte de la zone

```asm
mov a,r7 ; setb c ; subb a,#0x62 ; jc  ret   ; rejette page < 99
mov a,r7 ;         subb a,#0x76 ; jc  corps  ; rejette page > 117
...
add  a, 0xE0                                 ; direct E0H = ACC -> a = r7 × 2
mov  0xF7, a                                 ; XPAGE = page × 2
mov  r7,#0xE6 ; lcall fcn.0000989F           ; declenche l'effacement
```

`XPAGE` (F7H) reçoit `page × 2`, soit l'octet de poids fort de l'adresse : **une page vaut 512
octets** — ce qui est exactement le `sector_size` déclaré par SMK pour le `sh68f90`.

| | |
| --- | --- |
| Pages autorisées | **99 à 117** (bornes vérifiées dans le code, pas déduites) |
| Adresses | **`0xC600` – `0xEBFF`** |
| Taille | 19 pages × 512 o = **9 728 octets** |

La zone tombe entièrement dans les 61 440 octets du firmware (`0xEBFF < 0xF000`) : cohérent.

`fcn.0000989F` écrit ensuite `IB_CON1..IB_CON5` (F2H–F6H) après avoir revérifié **une seconde
fois** que `XPAGE / 2` est bien dans `[99, 117]` et que `EA` est nul — double garde contre un
effacement hors zone.

### Vérification sur le dump : la zone est vivante, et à moitié utilisée

Entropie par page de 512 octets sur `assets/f75_firmware.bin` :

| Page | Adresse | Entropie | Octets nuls |
| --- | --- | --- | --- |
| 98 | `0xC400` | 6.53 | 46 | *(code — hors zone)* |
| 99 | `0xC600` | 1.20 | 425 |
| 100 | `0xC800` | 1.10 | 230 |
| 101 | `0xCA00` | 0.22 | 494 |
| 102–105 | `0xCC00`–`0xD3FF` | 1.0 – 1.9 | 412 – 455 |
| 106–109 | `0xD400`–`0xDBFF` | 0.04 | 510 |
| 110–117 | `0xDC00`–`0xEBFF` | **0.00** | **512** |
| 118 | `0xEC00` | 6.40 | 11 | *(code — hors zone)* |

Les pages 98 et 118, juste au-delà des bornes, sont du code dense (entropie > 6) ; les pages
99 à 117 sont à faible entropie et majoritairement nulles. **Les bornes déduites du code
coïncident exactement avec la frontière code/données observée dans le dump** — confirmation
indépendante. Les huit dernières pages sont intégralement vierges : la zone est
surdimensionnée par rapport à ce que le firmware d'usine y range.

Cette page 101 (`0xCA00`) est celle que `fcn.0000AF99` sauvegarde : les constantes `r6 = 0xCA`
(202 = 101 × 2) et `r5 = 101` du site `0xAFB3` s'expliquent enfin — c'est **la page où l'effet
RGB courant est rendu persistant**.

### `fcn.0000EEE3` — cinquième confirmation du brochage des colonnes

Appelée juste avant de couper les interruptions, elle relâche la matrice — le balayage étant
suspendu pendant l'écriture flash :

```asm
mov 0xC0, #0xFF   ; P6 = 0xFF    -> colonnes P6.0 .. P6.7
orl 0x88, #0x87   ; P5 |= 0x87   -> colonnes P5.0, P5.1, P5.2, P5.7
orl 0xB0, #0x0D   ; P4 |= 0x0D   -> colonnes P4.0, P4.2, P4.3
```

Les trois masques reproduisent **exactement** la carte des 15 colonnes établie par ailleurs.
C'est une cinquième source indépendante, et elle n'a pas été cherchée pour ça.

> ⚠️ Cette routine n'a **jamais été exécutée** : rien n'a été flashé sur l'appareil. Ce qui
> précède est du désassemblage, pas de l'observation.

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
