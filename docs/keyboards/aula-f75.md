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
| Câblage du rendu dans la boucle SMK | ✅ **résolu** — sous-trames LED de `tick.c`, voir ci-dessous |
| Rotation d'encodeur | ❌ non implémentée (phases identifiées : `P0.5` / `P0.6`) |
| Broches au rôle inconnu | ❓ `P0.0` `P0.1` `P4.1` `P4.4` `P5.5` `P5.6` `P7.7` — `P7.4`/`P4.5` sont le sélecteur de connexion, `P4.7` la ligne « module prêt » |
| Veille | ✅ **implémentée**, USB **et radio** — transcrite du firmware d'usine, non testée sur matériel |
| Sans-fil 2,4 GHz / Bluetooth | ⚠️ **écrit et compilé** (`aula_rf.c`, EUART0), **jamais exécuté** — voir ci-dessous |

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

**Ce n'est plus une inférence.** Le convertisseur d'usine `0x765B` lit le framebuffer 8 bits en
`0x0152 + colonne × 18 + ligne × 3 + couleur` et écrit le tampon 16 bits en
`0x05A2 + colonne × 36 + ligne × 6 + couleur × 2` : le **même** indexage `(ligne, couleur)` des deux
côtés, sans permutation. Et la table de phases `CODE 0x2922`, indexée elle aussi `ligne × 3 +
couleur`, se retrouve octet pour octet dans les `DUTY1` posés à l'init, permutés par exactement
l'ordre de chargement ci-dessus. La correspondance est lue, pas supposée ; l'essai sur matériel que
demandait la rédaction précédente n'est plus nécessaire.

### L'inadéquation d'architecture avec SMK — levée

Le rendu par colonne exige de recharger les 18 duties **à chaque avance de colonne**. Dans le
firmware d'usine, c'est l'ISR `_INT_PWM0` qui possède l'avance de colonne — et qui en profite pour
lire les lignes de la matrice au passage (`0x73A6`).

Cette page a longtemps conclu qu'il fallait choisir entre « l'ISR reprend la main sur les colonnes
comme en usine » et « le rechargement se greffe dans la boucle de scan », et que ce choix ne se
validait pas sans matériel. **Les deux branches étaient fausses, et l'ordonnanceur qu'il fallait
existait déjà dans SMK.**

`src/smk/tick.c` alterne, depuis l'ISR Timer2, **un balayage de matrice** puis
`LED_SUBFRAMES_PER_SCAN` **sous-trames LED** :

```c
void tick_dispatch(void) {
    if (scan_due) { scan_due = false; subframes_since_scan = 0; run_matrix_scan(); return; }
    run_led_subframe();
    if (++subframes_since_scan >= LED_SUBFRAMES_PER_SCAN) scan_due = true;
}
```

Le portage pose `LED_SUBFRAMES_PER_SCAN = MATRIX_COLS` : quinze sous-trames, une par colonne LED,
entre deux balayages de touches. Le rendu ne se greffe donc pas *dans* le scan — il vit à côté,
dans le créneau que `tick.c` lui réserve (`RELOAD_LED_SUBFRAME` = 400 µs contre ~320 µs de
balayage). `matrix.c` n'est pas touché d'une ligne, et `pwm_interrupt_handler` reste vide.

La cohabitation électrique, elle, était déjà résolue : `matrix_scan_full()` encadre son balayage
par `indicators_pwm_disable()` et `indicators_pwm_enable()` **parce que le courant des LED se
couple dans la détection de ligne**. Il suffisait d'implémenter les deux.

Un précédent existait dans l'arbre et n'avait pas été vu : le **genesis-thor-300** partage lui aussi
ses colonnes de matrice avec son multiplexage LED, pose `LED_SUBFRAMES_PER_SCAN MATRIX_COLS`, et
mesure son budget — *« one effect evaluation per subframe: six, one per row, does not fit and
starves the USB interrupt »*. Le portage reprend sa structure, une seule évaluation d'effet par
sous-trame, en remplaçant son tramage de trames entières par du vrai PWM.

#### La polarité, tranchée sur le dump

Cette section posait la polarité comme indécidable et le portage la concentrait dans une constante
`AULA_RGB_DUTY_INVERTED`. **La question était décidable, et la réponse est l'inverse de celle qui
avait été retenue.** Trois relevés indépendants la verrouillent.

**1. `DUTY1` est écrit, une seule fois, et vaut une constante par canal.** Sur les 36 références aux
registres `DUTY1` de l'image entière (`PWMnnDUTY1H` en `0xFFB8`–`0xFFC9`, `DUTY1L` en
`0xFFA0`–`0xFFB1`), **les 36 sont dans la routine d'init `0x6713`–`0x68CD`, et zéro ailleurs**. Le
chargeur de colonne ne module que `DUTY2`. Les 18 valeurs :

| Banc | Canaux | `DUTY1H` | `DUTY1L` |
| --- | --- | --- | --- |
| PWM0 | `PWM00`…`PWM05` | `0x00` | `C3 C4 C5 C0 C1 C2` |
| PWM1 | `PWM10`…`PWM15` | `0x00` | `BA BB BC BD BE BF` |
| PWM2 | `PWM20`…`PWM25` | `0x00` | `B4 B5 B6 B7 B8 B9` |

Ni `0`, ni `0x04B0` : dix-huit valeurs consécutives `0xB4`–`0xC5`, une par canal.

**2. La même table existe en flash, et sa permutation est celle du chargeur.** `CODE 0x2922` tient
18 octets, `0xB4 + indice`, indexés `ligne × 3 + couleur`. Trié, cet ensemble est **exactement**
celui des `DUTY1` ci-dessus ; la permutation entre les deux est **exactement** l'ordre de chargement
de `fcn @ 0x6E61` (`buf[0..5]→PWM20..25`, `[6..11]→PWM10..15`, `[12..14]→PWM03..05`,
`[15..17]→PWM00..02`). Ce recoupement confirme au passage la correspondance canal ↔ (ligne, couleur)
et la rotation du groupe P3, jusque-là déduites de l'ordre du driver OpenRGB.

**3. La conversion 8 → 16 bits — l'ancien « seul vrai trou » — est en `0x765B`–`0x770C`.** Une seule
fonction touche les deux bases : elle écrit le framebuffer 8 bits (`0x0152 + colonne × 18 +
ligne × 3 + couleur`) puis, en écriture directe, le tampon 16 bits (`0x05A2 + colonne × 36 +
ligne × 6`, **gros-boutiste**). Son arithmétique, en `0x76C3` :

```
76C3  mov  a,r3           ; la valeur 8 bits
76C4  mov  b,#0x04
76C7  mul  ab             ; r6:r7 = v x 4
76CB  mov  a,r5           ; la ligne
76CC  mov  b,#0x03
76CF  mul  ab
76D0  add  a,#0x22        ; dptr = 0x2922 + ligne x 3
76D5  addc a,#0x29
76DA  movc a,@a+dptr      ; la phase du canal
76DD  add  a,r7           ; r6:r7 += phase   (addition 16 bits)
```

Soit :

> **`DUTY2 = (valeur << 2) + phase[ligne × 3 + couleur]`**, avec `DUTY1` figé sur cette même phase.

**Conséquence, par monotonicité.** Ni le datasheet ni `src/platform/sh68f90/pwm.h` ne disent
laquelle des deux arêtes ouvre l'impulsion — on n'en a pas besoin. `DUTY1` est figé et `DUTY2` croît
avec la valeur ; sous la lecture opposée, la valeur 1 donnerait une impulsion de 1196 crans et la
valeur 255 une de 180, soit une luminosité **décroissante** avec la valeur, discontinue en zéro.
Absurde. L'impulsion va donc de `DUTY1` à `DUTY2`, sa largeur vaut `valeur << 2`, et le rapport
cyclique est **direct** : 0 = éteint (impulsion nulle, `DUTY2 = DUTY1`), 255 = 1020/1200 soit 85 %
de la période. Le décalage de phase, un cran par canal, étale les fronts montants des 18 voies
au lieu de les faire coïncider ; il disparaît du résultat lumineux.

L'indice qui allait vers le sens direct — `user_matrix_sinks_off()` tire les 18 broches au niveau
bas — était donc le bon, et la conclusion « anode commune, le PWM fait office de sink » de cette
page était fausse. `AULA_RGB_DUTY_INVERTED` a disparu du portage : il n'y a plus de choix à faire.

**Un seul écart assumé avec l'usine : l'écrêtage.** À valeur 255 la somme vaut jusqu'à
`1020 + 197 = 1217`, soit 17 crans au-delà de la période. L'usine ne l'écrête pas ; le portage si,
parce que `led_effect_rgb()` produit bel et bien 255 et qu'un `DUTY2` supérieur à la période n'a pas
de comportement défini par le datasheet. L'écrêtage ne mord qu'à partir de la valeur 251, et
seulement sur les trois canaux de la ligne 5.

### Ce qui est établi côté PWM

- Le MCU a **25 canaux** : PWM0 (00-05), PWM1 (10-15), PWM2 (20-25), PWM3 (30-33), PWM4 (40-42).
- Les **18** utilisés pour le RGB sont PWM0/1/2, confirmés par `P1CR=P2CR=P3CR=0x3F` dans l'init
  d'usine.
- La correspondance broche↔canal est déduite du NuPhy Air60 : `PWM0x↔P3_x`, `PWM1x↔P2_x`,
  `PWM2x↔P1_x`, `PWM4x↔P5_x`. **Déduite, pas confirmée par datasheet.**
- **Période PWM d'usine : `0x04B0` = 1200**, lue en `0x6707` (`PWM0PERDH=0x04`) et `0x670D`
  (`PWM0PERDL=0xB0`). Le portage `tiagoluizo` reprogramme la sienne à `0x0400` = 1024 et calcule
  ses rapports cycliques en conséquence (`0x0400 - (v << 2)`) — cohérent chez lui, mais **ce n'est
  pas la valeur d'usine**, et son inversion est contredite par le relevé de `DUTY1` ci-dessus. Son
  facteur `v << 2`, en revanche, est exactement celui de l'usine.
- Rapport cyclique **direct** : `DUTY2 = (v << 2) + phase`, `DUTY1` = phase, donc largeur
  d'impulsion `v << 2` — 0 = éteint, 255 = 85 % de la période. Établi sur le dump (voir ci-dessus) ;
  une rédaction antérieure de cette page concluait « inversé », c'était faux.
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

### L'ordonnanceur d'animation — `0x1AF0` et la table `0x1B13`

Choisir un effet ne l'anime pas. L'animation est cadencée par un ordonnanceur situé à la fin de la
grande fonction RGB (Ghidra `0x1108`–`0x1DC2`, r2 `fcn.00001111`), et **aucun des deux outils ne
l'avait sorti** : le corps est atteint par saut calculé, donc r2 le laisse hors fonction et Ghidra
laisse les octets indéfinis. Il se lit au désassemblage, depuis une ancre sûre.

#### Les trois variables

| Adresse | Rôle | Écrit par |
| --- | --- | --- |
| `0x0300:0x0301` | **tic d'animation**, 16 bits gros-boutiste | incrémenté par `isr.pwm4` (`0x8188`), remis à zéro par `0x1D69` |
| `0x08BB:0x08BC` | **période** de l'effet courant | `0x1AE2`, juste avant l'aiguillage : poids fort = 0, poids faible = `r7` |
| `0x097C` | compteur secondaire | incrémenté par `isr.pwm4` (`0x8196`) |

`isr.pwm4` incrémente `0x0301` et ne touche `0x0300` que sur débordement — c'est bien un compteur
16 bits, et la période tenant sur `r7` reste **inférieure à 256**.

> `0x08BC` n'est atteint par aucun `mov dptr, #0x08bc` : il n'est lu que par `inc dptr`. Chercher
> l'adresse d'un octet haut d'un mot 16 bits ne donne rien — c'est le piège du pointeur calculé,
> déjà rencontré sur le bloc de réglages.

#### Ce qui précède l'aiguillage — la table `0x1744`, dix-neuf semeurs

L'aiguillage décrit ci-dessous n'est pas le point d'entrée. Il est précédé d'un **second
répartiteur**, en `0x1741`, qui branche sur `[0x0ED0]` — l'effet **demandé** — et non sur `0x0896`,
l'effet **actif**. C'est le même répartiteur inline `0x4E45` que le conteneur `0x08` reçu, et les
entrées se lisent de la même façon : `(adresse_hi, adresse_lo, clé)`, terminateur `00 00`, puis
l'adresse par défaut — ici **`0x1AEA`**, c'est-à-dire *l'aiguillage lui-même*. Un effet sans semeur
tombe donc directement dans le rendu.

##### Le squelette, identique aux dix-neuf

```asm
20 21 03        jb  0x21, +3          ; le bit de ressemis est-il pose ?
02 1a ea        ljmp 0x1AEA           ; non -> aiguillage de rendu, sans rien semer
c2 21           clr 0x21              ; oui : consommer la demande
d2 22           setb 0x22             ; autoriser le rendu
c2 32 / c2 55   clr ...               ; remettre a zero les bits d'etat de l'effet
12 xx xx        lcall <SEMEUR>        ; propre a l'effet
90 0d 9d e0     a = [0x0D9D]          ; la VITESSE
90 xx xx        dptr = <table de periode propre a l'effet>
02 1a e0        ljmp 0x1AE0           ; movc, puis [0x08BB:0x08BC] = periode, puis aiguillage
```

Le bit `0x21` est celui que `0x16EC` lève quand l'effet change, et `0x0D9D` est la vitesse. Trois
faits en découlent, qu'aucune autre partie de cette page ne donnait :

- **le semeur ne tourne qu'une fois par changement d'effet**, pas à chaque trame ;
- **la période dépend de l'effet ET de la vitesse**, par une table à deux entrées ;
- **quatre effets ont en plus un travail par trame** avant l'aiguillage (voir plus bas).

##### La table

| Effet | Arm | Bits remis à zéro | Semeur | Table de période |
| --- | --- | --- | --- | --- |
| `0x00` | `0x1781` | `0x32`, `0x55` | `0xEDE7(0)` puis `0xEE6A` | — |
| `0x01` | `0x179A` | `0x55` | `0xEF85` | — |
| `0x02` | `0x17AC` | `0x32`, `0x55` | `0xEFB8` | index 2 |
| `0x03` | `0x17C4` | `0x32`, `0x55` | `0xEFB8` | index 3 |
| `0x04` | `0x17DC` | *(dans le semeur)* | `0xA94A` | index 4 |
| `0x05` | `0x183D` | `0x55` | `0xEF32` | index 0 |
| `0x06` | `0x1853` | *(dans le semeur)* | `0xEE62` + `0xA791` | index 5 |
| `0x07` | `0x1866` | *(dans le semeur)* | `0xA94A` | index 6 |
| `0x08` | `0x18F3` | *(dans le semeur)* | `0xEE62` + `0xEF4D` | — |
| `0x0A` | `0x1902` | `0x55`, `0x32` | `0x50E2` | index 8 |
| `0x0B` | `0x191A` | `0x55`, `0x32` | `0xEFB8` | index 9 |
| `0x0C` | `0x1932` | `0x55`, `0x32` | `0xEE6A` | index 10 |
| `0x0D` | `0x19C6` | `0x55`, `0x32` | `0xA846` | index 11 |
| `0x0F` | `0x1A9D` | `0x32`, `0x55` | **`0xACA3`** — le transposeur | index 12 |
| `0x10` | `0x1AB4` | `0x32`, `0x55` | `0xEFBE` | index 13 |
| `0x11` | `0x1ACB` | `0x32`, `0x55` | `0xEF85` | index 14 |
| `0x20` | `0x19DE` | `0x55` | `0x95A4(0)` | — |
| `0x26` | `0x19F2` | `0x55` | `0x50E2` | — |
| `0x2D` | `0x1A04` | — | `0x94E6` | — |
| *défaut* | `0x1AEA` | — | — | — |

**Les clés `0x09` et `0x0E` sont absentes**, exactement comme les entrées 9 et 14 de la table de
rendu `0x1B13`. Deux tables indépendantes qui manquent les deux mêmes effets : ces deux emplacements
sont vides dans le firmware d'usine, ce n'est pas une lacune de lecture.

##### Les tables de période — `CODE 0x2FE9`, quinze tables de cinq octets

Indexées par la vitesse `[0x0D9D]`, de 0 (lente) à 4 (rapide). La période va dans `[0x08BB:0x08BC]`,
et le gestionnaire de rendu ne travaille que lorsque le tic l'atteint.

| Index | Adresse | Valeurs | Effet |
| --- | --- | --- | --- |
| 0 | `0x2FE9` | `120 100 80 50 20` | `0x05` |
| 1 | `0x2FEE` | `60 50 40 30 20` | **jamais référencée** |
| 2 | `0x2FF3` | `45 35 25 15 5` | `0x02` |
| 3 | `0x2FF8` | `45 35 25 15 6` | `0x03` |
| 4 | `0x2FFD` | `80 60 40 20 8` | `0x04` |
| 5 | `0x3002` | `20 15 10 5 1` | `0x06` |
| 6 | `0x3007` | `88 68 48 28 8` | `0x07` |
| 7 | `0x300C` | `115 100 85 60 30` | **jamais référencée** |
| 8 | `0x3011` | `120 90 70 45 1` | `0x0A` |
| 9 | `0x3016` | `30 24 18 12 6` | `0x0B` |
| 10 | `0x301B` | `32 24 18 16 6` | `0x0C` |
| 11 | `0x3020` | `115 95 75 55 30` | `0x0D` |
| 12 | `0x3025` | `32 24 16 8 1` | `0x0F` |
| 13 | `0x302A` | `46 36 26 16 6` | `0x10` |
| 14 | `0x302F` | `50 40 30 20 8` | `0x11` |

Une recherche de toutes les charges `mov dptr` visant `0x2FE9`–`0x3033` sur l'image entière donne
quatorze sites : les treize arms ci-dessus, plus `0xAC25` qui réutilise l'index 0. **Les index 1
et 7 ne sont chargés nulle part.** Six effets n'ont aucune table (`0x00`, `0x01`, `0x08`, `0x20`,
`0x26`, `0x2D`) : leur cadence est portée par leur propre moteur.

##### Les quatre arms qui travaillent à chaque trame

`0x04`, `0x07`, `0x0C` et `0x2D` ne se contentent pas de semer. Après la période, et **à chaque
passage**, ils lisent la dernière touche vue et la projettent sur la grille :

```c
if ([0x0EE1] == 0) {                       /* une touche est en attente */
    id = CODE[0xC500 + [0x0EE0]];          /* index de grille col*6+ligne -> id col*8+ligne */
    [0x0ED1] = id & 7;                     /* ligne */
    [0x0ED2] = id >> 3;                    /* colonne */
    ...
}
```

C'est la même table `0xC500` dont cette page tire la carte de présence, lue ici pour ce qu'elle est
vraiment : **une conversion d'index de grille vers l'identifiant d'usine**, `0xFF` pour les six
emplacements sans touche.

| Arm | Ce qu'il fait de la touche |
| --- | --- |
| `0x04` | pose le bit `1 << ligne` dans **deux** cartes : `0x013B + col` et `0x0001 + col` |
| `0x07` | pose ce bit dans les **quatre** : `0x08C6`, `0x02E8`, `0x013B`, `0x0001`, toutes `+ col` |
| `0x0C` | teste d'abord le `0xFF`, puis lit `CODE[0x2EED + ligne*21 + col]` et `CODE[0x2F6B + …]` dans `[0x0ED5]`/`[0x0ED6]`, et appelle `0x5114` |
| `0x2D` | même lecture des deux tables, puis, si les bits `0x44` et `0x43` sont posés, appelle `0x8A1A` avec `[0x0ED5]*6 + 0x17 + [0x0ED6]` |

Les quatre cartes de `0x04` et `0x07` sont exactement celles que cette page attribue à l'automate à
quatre directions des effets 4, 7 et 13 — **et l'arm `0x07` les alimente toutes les quatre alors que
l'arm `0x04` n'en alimente que deux**, ce qui distingue enfin deux effets que la table de rendu fait
pourtant pointer sur le même moteur `0x5BE9`.

Et `0x2EED` réapparaît ici par une voie indépendante, avec son pas de 21 octets par ligne : la base
corrigée de la table A est confirmée une seconde fois.

##### Ce que le portage pourrait en tirer, et n'en tire pas

`indicators.c` cadence ses animations avec un diviseur inventé, `16 >> led_speed`, et une décroissance
`4 << led_speed`. Les quinze tables ci-dessus sont la donnée d'usine correspondante — **mesurée, pas
inférée**. Les reprendre remplacerait une invention par un relevé.

Ce n'est pas fait ici, et pour une raison précise : les treize effets du portage ne sont pas les
dix-neuf d'usine, la correspondance n'est pas bijective, et brancher des périodes d'usine sur des
moteurs réécrits changerait le comportement de treize effets qui n'ont jamais tourné. C'est une
décision de conception, pas une transcription ; elle appartient à qui la demandera.

#### L'aiguillage

```asm
0x1AE2  mov dptr,#0x08BB ; clr a ; movx @dptr,a ; inc dptr ; mov a,r7 ; movx @dptr,a
0x1AEA  jb  0x24.2, 0x1AF0        ; sinon, rien
0x1AED  ljmp 0x1DC2
0x1AF0  mov dptr,#0x0896 ; movx a,@dptr    ; a = effet courant
0x1AF4  add a,#0xE0 ; jnz +3 ; ljmp 0x1DAB ; effet 0x20
0x1AFB  add a,#0xFA ; jnz +3 ; ljmp 0x1D05 ; effet 0x26
0x1B02  add a,#0x26                        ; a restaure
0x1B04  cjne a,#0x12,$+3 ; jc 0x1B0C ; ljmp 0x1DC2   ; borne : 18 effets
0x1B0C  mov dptr,#0x1B13 ; mov r0,a ; add a,r0 ; add a,r0   ; a = 3 x idx
0x1B12  jmp @a+dptr
```

La chaîne `add a,#0xE0 / add a,#0xFA / add a,#0x26` cumule à zéro : les deux premiers termes
testent `0x20` et `0x26` sans détruire l'accumulateur. **Ce n'est pas un calcul, c'est deux
`if` déguisés** — un idiome de compilateur qui trompe la lecture rapide.

#### La table `0x1B13` — dix-huit gestionnaires

Chaque gestionnaire compare le tic à la période, et n'agit que si `tic ≥ période` ; sinon il part
sur `0x1DC2` (`ret`). Quand il agit, il appelle le rendu puis saute à la queue commune `0x1D69`,
qui **remet le tic à zéro**.

| idx | Gestionnaire | Seuil | Compteur `0x097C` | Rendu |
| --- | --- | --- | --- | --- |
| 0 | `0x1B49` | immédiat **100** | — | `0xEE6A` |
| 1 | `0x1B61` | immédiat **10** | — | `0x746A`, pas `0x08C3` borné à 9 |
| 2 | `0x1B8D` | `0x08BB` | — | **`0x7C12`** |
| 3 | `0x1BAB` | `0x08BB` | — | `0x9DA4` |
| 4 | `0x1BC9` | `0x08BB` | ≥ 16 → `0x64F1` | `0x5BE9` |
| 5 | `0x1BF5` | `0x08BB` | ≥ 11 → `0x64F1` | `0x9B2B` |
| 6 | `0x1C21` | `0x08BB` | — | `0x60C9` |
| 7 | `0x1C3F` | `0x08BB` | ≥ 16 → `0x64F1` | `0x5BE9` |
| 8 | `0x1C6B` | — | — | `ljmp 0xAC1C` (tremplin) |
| 9 | `0x1DC2` | — | — | **`ret` — effet inactif** |
| 10 | `0x1C6E` | `0x08BB` | ≥ 16 → `0x64F1` | `0x8DDF` |
| 11 | `0x1C9A` | `0x08BB` | — | `0x6C9B` |
| 12 | `0x1CBC` | `0x08BB` | — | `0x64F1` |
| 13 | `0x1CDA` | `0x08BB` | ≥ 3 → `0x64F1` | `0x5BE9` |
| 14 | `0x1DC2` | — | — | **`ret` — effet inactif** |
| 15 | `0x1D51` | `0x08BB` | — | `0x82A5` |
| 16 | `0x1D71` | `0x08BB` | — | `ljmp 0x9659` |
| 17 | `0x1D8D` | `0x08BB` | — | corps en ligne |

#### Les huit moteurs manquants — décodés

Cette page listait huit gestionnaires « non décodés ». Ils le sont. Le tableau ci-dessus les
nomme ; voici ce qu'ils font, et ce que leur lecture a rapporté au passage.

| idx | Moteur | Effet visible |
| --- | --- | --- |
| 5 | `0x9B2B` | **pluie** : une gouttelette descend chaque colonne, une ligne par trame ; une colonne tirée au hasard est relancée quand elle dort ; la traînée vient du fondu `0x64F1` |
| 8 | `0xAC1C` → `0x4EA9`/`0x62E6` | **scintillement** : des touches tirées au hasard passées à pleine intensité |
| 10 | `0x8DDF` | **serpent** : un point parcourt la grille en boustrophédon, inverse son sens horizontal au bord et descend d'une ligne, inverse son sens vertical en haut et en bas |
| 15 | `0x82A5` | **vague** sur une seconde palette de 128 teintes, phase par touche avancée d'un cran par trame (`0xEDA2`) |
| 16 | `0x9659` | **arc-en-ciel vertical** : teinte `+25` par ligne modulo 192, base défilante — six lignes couvrent 150 des 192 entrées |
| 17 | `0x1D8D` | l'onde concentrique du moteur 1 (`0x746A`), **paramètre figé à 9** — pas un moteur distinct |
| `0x20` | `0x1DAB` → `0x95A4` | **mode gaming** : image statique de `CODE 0xCA00`, plans rouge et vert nuls |
| `0x26` | `0x1D05` → `0x8DDF` | le serpent en **mode couleur 7** (teinte aléatoire) avec luminosité forcée à 4 |

##### Le modèle de rendu à deux plans

C'est la trouvaille structurante, et elle vaut pour tous les moteurs, pas seulement les huit :

- **`XRAM 0x0428`** — la **couleur de base** de chaque touche, `colonne × 18 + ligne × 3 + composante`.
- **`XRAM 0x0017`** — l'**intensité** de chaque touche, `colonne × 6 + ligne`, 126 octets.
- **`fcn @ 0x62E6`** — le rendu : `base × intensité >> 5` vers `0x0152`, qui part ensuite au PWM.

C'est exactement le modèle que le portage tenait déjà pour le seul moteur réactif. D'où la
structure retenue : **six effets partagent la même mécanique et ne diffèrent que par leur semeur**
(touches frappées, gouttelettes, hasard, serpent, serpent multicolore, couronnes).

##### `0x0428` est un miroir de `0x0152`

La question restait posée de savoir si ces deux tampons de 378 octets avaient la même forme. Ils
l'ont : `fcn @ 0x8B29` écrit **la même couleur dans les deux**, avec le **même** indexage
`colonne × 18 + ligne × 3 + composante`. La correspondance canal ↔ (ligne, couleur) est donc
vérifiée sur deux chemins d'écriture indépendants, pas un.

##### L'image « gaming » confirme tout le plan de matrice

Le plan bleu de `CODE 0xCAFC` allume neuf positions : `(c0,l0)`, `(c2,l2)`, `(c1,l3)`, `(c2,l3)`,
`(c3,l3)`, `(c13,l4)`, `(c12,l5)`, `(c13,l5)`, `(c14,l5)`. Reportées sur
`layouts/default/layout.c`, ce sont **Échap, W, A, S, D, ↑, ←, ↓, →**. Neuf coïncidences sur une
voie de données qui n'a rien à voir avec le balayage : c'est la meilleure confirmation disponible
du plan de matrice complet.

##### Trois tables recopiées

- **Palette secondaire, `CODE 0x2D6D`** — 128 triplets `(R,G,B)`, 384 octets. Distincte de la roue
  de 192. Ses bornes sont fixées par contiguïté : la roue finit en `0x2D6A`, la carte des couronnes
  commence en `0x2EED = 0x2D6D + 384`.
- **Couronnes de l'onde, `CODE 0x2959`** — 9 groupes de 13 identifiants, `0xFF` pour vide.
  L'encodage est `colonne × 8 + ligne` : la couronne 0 vaut `0x3A` = (colonne 7, ligne 2), le centre
  que cette page annonçait déjà. **Ce ne sont pas des distances** — la couronne 8 contient `(2,0)`,
  qu'aucune métrique ne placerait là. C'est un ordre de propagation écrit à la main.
- **Carte de présence, `CODE 0xC500`** — six emplacements de la grille 6 × 15 n'ont pas de touche.
  Sa ligne 4 permute les colonnes, **exactement** comme la ligne 4 de la table `0x2EED` : deux
  relevés indépendants qui butent sur la même particularité de rangée.

##### Le générateur pseudo-aléatoire, `fcn @ 0xA997`

LFSR de Galois sur 32 bits, décalage à droite, masque `0xCC4C4ECE`, état en `XRAM 0x0F67`, réamorcé
à `0xA5A5` s'il tombe à zéro, seize tours par appel ; suivi de `0x4D52` (division signée) pour le
modulo. Six appelants, dont la pluie (`rand % 15`) et le mode couleur 7 (`rand % 192`).

Le portage transcrit le **comportement** et substitue le **générateur** : reproduire le polynôme ne
rendrait pas la même suite sans la même graine ni le même ordre d'appel, et coûterait de la pile
dans l'ISR pour une propriété observable qui est « une colonne au hasard ».

##### La vitesse pilote le mouvement, pas seulement la teinte

Piège du portage, pas du dump. `led_speeds[]` de SMK ne fait avancer que la phase d'animation, donc
sans précaution `SPD_UP`/`SPD_DN` changeraient la *couleur* de la pluie et du serpent mais pas leur
*allure*. Le firmware d'usine, lui, conditionne tout son rendu à `tic ≥ période`, période tirée de
`CODE 0x2FE9` selon la vitesse.

Le portage gate donc le semeur sur un diviseur de trame `16 >> vitesse` (16, 8, 4, 2, 1) et fait
suivre la décroissance du plan d'intensité, `4 << vitesse` (4, 8, 16, 32, 64). Le produit reste
constant : la **traînée garde la même longueur** — environ quatre touches — à toutes les vitesses,
et seul le mouvement accélère.

##### Le champ de phase de l'effet 15 — l'écrivain de `0x0E49` n'existe pas

Cette page a listé « l'écrivain de `XRAM 0x0E49` » comme point ouvert. **Il n'y en a pas, et c'est la
réponse** : le plan est une *constante*, pas une variable.

La recherche a d'abord donné un résultat qui ressemblait à une impasse : sur les 126 octets de
`0x0E49`–`0x0EC6`, **une seule référence dans les 64 Ko de l'image**, et c'est la lecture par le
transposeur `0xACAC`. Trois formes d'adressage pouvaient encore cacher une écriture ; les trois sont
éliminées :

| Forme | Verdict |
| --- | --- |
| `MOV DPTR,#0x0E49` | aucun site — la plage `0x0E4A`–`0x0EC7` n'est référencée nulle part |
| `ADD A,#0x49` / `ADDC A,#0x0E` | un seul site, `0xACAC`, et c'est une lecture |
| `MOVX @Ri` (page `P2`) | impossible : `P2` n'est écrit que deux fois, dont une dans le bootloader, et l'autre (`0xA824`) est une mise à zéro de port au démarrage. `P2` vaut 0, donc `MOVX @Ri` ne voit que `0x00xx` |

Le démarrage donne la réponse. `startup_c51` (`0x9C74`) efface **les 4 096 octets de XDATA**
(`0x9C7A`–`0x9C86`) puis saute au décompacteur d'initialiseurs Keil, qui lit la table de `CODE 0x9FDE`.
Cette table ne compte que **trois enregistrements** :

| Enregistrement | Contrôle | Taille | Destination |
| --- | --- | --- | --- |
| `0x9FDE` | `0x41` | 1 o | `XDATA 0x08C4` |
| `0x9FE2` | `0x41` | 1 o | `XDATA 0x0C36` |
| `0x9FE6` | `0x60` | **126 o** | **`XDATA 0x0E49`**, données en `CODE 0x9FEA` |

Le décodage est **auto-validant** : le terminateur `0x00` tombe exactement à la fin du troisième
enregistrement, en `0xA068`. Aucune place pour un décalage d'interprétation.

Les 126 octets, remis en forme `[ligne][colonne]` avec le pas de 21 qu'emploie `0xACA3` :

```
l0: 52 53 54 55 57 59 5c 62 69 73 78 7c 7f 01 03 | 05 06 07 08 51 51
l1: 50 50 51 52 53 55 56 5f 6c 78 7f 01 03 06 07 | 08 09 09 0a 4f 4f
l2: 4d 4e 4e 4f 4f 50 52 5a 7d 06 08 09 09 0a 0b | 0b 0b 0c 0c 4d 4d
l3: 48 47 46 45 44 43 42 34 17 11 10 0f 0f 0f 0e | 0e 0e 0d 0d 4a 49
l4: 45 44 43 42 40 3d 34 27 1b 18 16 13 13 12 12 | 12 11 10 0f 47 46
l5: 44 42 40 3e 36 30 29 1f 19 17 16 15 14 14 13 | 12 11 11 10 46 45
```

Valeurs **1 à 127** — très exactement la plage d'un index dans la palette de 128, ce qui confirme la
lecture du moteur. Et le champ n'est **pas uniforme** : c'est un **balayage angulaire**. La phase
croît le long de la première ligne jusqu'à repasser par zéro entre les colonnes 12 et 13, et décroît
vers le bas à gauche. Une vague qui **tourne**, pas qui translate.

Le portage transpose la table une fois pour toutes à la compilation (`aula_fx_keywave()`, 90 octets
pour les colonnes utiles) et calcule `phase_de_la_touche + compteur de trames` — strictement
équivalent à l'avance d'un cran par trame et par touche que fait `0xEDA2`, sans aucun état à tenir.

Deux détails qui comptent pour le rendu :

- **Le transposeur est appelé par effet, pas une fois pour toutes.** `0x1AA8` est précédé d'un
  `JNB 0x21` et suivi de branches sœurs en `0x1AB2`, `0x1AC4` et `0x1AD0` qui appellent `0xEFBE` et
  `0xEF85` avec le même prologue : c'est un aiguillage, une branche par effet, chacune ressemant son
  propre plan. Le champ repart donc du dump **à chaque sélection** de l'effet. Le portage remet la
  phase à zéro dans `fx_reset()` pour la même raison.
- **La couture entre la colonne 14 et la colonne 0 est normale.** Le balayage se referme sur lui-même
  dans l'espace de 21 colonnes grâce aux six colonnes 15–20 (`51 51`, `4f 4f`, `4d 4d`…), qui
  n'existent pas sur ce clavier. Sur quinze colonnes, la phase saute de `0x03`–`0x13` à `0x52`–`0x44`
  au bouclage — et le firmware d'usine fait exactement le même saut sur exactement ce matériel. Ce
  n'est pas un artefact de portage.

**L'effet 15 est donc entièrement transcrit.** L'approximation diagonale que le portage assumait est
retirée.

Hors table, les deux effets spéciaux :

| Effet | Gestionnaire | Ce qu'il fait |
| --- | --- | --- |
| `0x20` | `0x1DAB` | seuil immédiat **100** |
| `0x26` | `0x1D05` | `0x097C ≥ 5` → force **`0x0D19 = 4`** (luminosité) puis `0x64F1` ; ensuite seuil immédiat **1** |

`0x64F1` revient sept fois : c'est le rendu partagé, appelé soit comme sous-étape sur le compteur
secondaire, soit comme rendu principal (idx 12). Les indices **9 et 14 sont explicitement vides** —
deux emplacements d'effet réservés et non implémentés.

**Les dix-huit gestionnaires sont désormais tous décodés et portés** (hors les deux vides). Le
détail des huit derniers suit.

#### `0x1BA5`, en particulier

C'est la ligne utile du gestionnaire d'indice **2** :

```asm
0x1B8D  mov dptr,#0x08BB ; movx a,@dptr ; mov r6,a   ; periode, poids fort
0x1B92  inc dptr ; movx a,@dptr ; mov r7,a           ; periode, poids faible
0x1B95  clr c
0x1B96  mov dptr,#0x0301 ; movx a,@dptr ; subb a,r7
0x1B9B  mov dptr,#0x0300 ; movx a,@dptr ; subb a,r6
0x1BA0  jnc 0x1BA5
0x1BA2  ljmp 0x1DC2       ; tic < periode : rien a faire
0x1BA5  lcall 0x7C12      ; rendu de l'effet 2
0x1BA8  ljmp 0x1D69       ; remise a zero du tic
```

**Qui y saute :** le `jnc` de `0x1BA0`, quand `tic ≥ période`. Et qui atteint le bloc : le saut
calculé `jmp @a+dptr` de `0x1B12`, via l'entrée 2 de la table `0x1B13`. Aucun `lcall` ni `ljmp`
direct — c'est pour cela qu'un balayage de références ne trouvait rien.

Cela referme la question laissée ouverte sur `fcn.00007C12` : ce n'est pas une routine orpheline,
c'est **le moteur de rendu de l'effet d'indice 2**, et il relit `0x0896` en `0x7C33` et `0x7C6F`
pour retrouver sa palette.

> **Correction sur Ghidra.** `get_xrefs_to 0x7C12` annonce un `COMPUTED_JUMP` venant de `0x7B24`,
> le `jmp @a+dptr` de `fcn.00007AAD`. C'est faux : la table de ce saut est en `0x7B25`, elle compte
> **treize entrées** (`0x7B25`–`0x7B48`) et **aucune ne vaut `0x7C12`** — elles pointent toutes
> entre `0x7B4C` et `0x7B8A`. La reconstruction de table de Ghidra déborde, et c'est cette
> destination inventée qui lui fait fusionner `0x7AAD` et `0x7C12` en une seule fonction.

### Les deux moteurs de rendu — `0x64F1` et `0x7C12`

L'ordonnanceur appelle seize routines de rendu. Deux d'entre elles portent les deux familles
d'animation, et elles se lisent enfin en C.

#### D'abord l'outil : `fcn.00004E39`, l'assistant de pointeur Keil

```asm
0x4E39  mul ab ; add a,DPL ; mov DPL,a ; mov a,B ; addc a,DPH ; mov DPH,a ; ret
```

**`dptr += a × b`**, en sept octets, appelé **94 fois**. Toute indexation de tableau à pas non
trivial passe par lui. Le décompilateur le rend en `FUN_CODE_4e39(3, ligne)` au milieu d'une
expression d'adresse : ce n'est pas un appel métier, c'est l'opérateur `[]`. Le nommer
`dptr_add_mul` rend lisibles d'un coup toutes les décompilations RGB.

#### `0x64F1` — la décroissance par touche

Double boucle sur toute la matrice : `0x0EE2` = colonne **0..14**, `0x0EE3` = ligne **0..5**.

| Tableau | Adresse | Contenu |
| --- | --- | --- |
| masque d'exclusion | `0x0BFF + colonne` | un octet par colonne ; la touche est traitée si le bit `1 << ligne` est **à zéro** |
| couleur par touche | `0x0428 + colonne × 18 + ligne × 3` | trois octets ; **378 o alloués** (21 × 18), `0x0428`–`0x05A1` |
| intensité par touche | `0x0017 + colonne × 6 + ligne` | un octet ; **126 o alloués** (21 × 6) |

Pour chaque touche retenue :

```c
composante = (couleur[i] * intensité) >> 5;     /* i = 0,1,2 */
rgb_key_suppressed(ligne, colonne);
if (!supprimée) { rgb_apply_brightness(); rgb_pixel_write(); }

if (g_palette_effect_idx == 13)  intensité = (intensité < 2) ? 0 : intensité - 2;
else                             if (intensité) intensité -= 1;
```

**C'est le moteur réactif** : chaque touche porte sa propre couleur et sa propre intensité, et
l'intensité **décroît d'un cran par trame** — de deux crans pour l'effet 13, qui s'éteint donc deux
fois plus vite. Le `>> 5` fait de l'intensité une échelle sur **32 niveaux**.

Les trois tableaux ne sont atteints que par pointeur calculé : `dptr_refs` sur `0x0428`, `0x0017`
ou `0x0BFF` ne renvoie **rien**. Ils n'ont été trouvés que par la décompilation.

> **Correction.** J'avais écrit `0x0C17` pour le tableau d'intensité. C'est **`0x0017`** :
> `mul ab (b=6) ; add a,#0x17 ; clr a ; addc a,#0x00` — l'octet de poids fort est **nul**. Le `0x0C`
> venait du masque d'exclusion voisin (`add a,#0xFF ; addc a,#0x0C`), qui lui est bien en `0x0BFF`.
> Le `CONCAT11` du décompilateur écrit ces deux cas presque pareil ; seules les constantes du
> désassemblage les séparent.

> **Taille corrigée.** J'avais annoncé 270 et 90 octets, d'après la boucle de `0x64F1` qui ne
> parcourt que quinze colonnes. L'allocation réelle en fait **vingt et une**, comme la table LED :
> `rgb_clear_key_state` (`0x9078`) les efface toutes deux jusqu'à `0x15`. Le tampon couleur finit
> donc en `0x05A1`, **juste avant** le tampon de trame PWM en `0x05A2` — les deux sont contigus.

#### `0x7C12` — le dégradé défilant (effet d'indice 2)

Une seule boucle, sur les colonnes **0..14** — pas d'état par touche. La couleur est choisie une
fois par colonne, selon le mode de couleur `0x011C` :

| `0x011C` | Source |
| --- | --- |
| `< 7` | la palette de l'effet : `CODE 0xC800 + [0x0896] × 21 + mode × 3` |
| `== 7` | la **roue de teintes** : `CODE 0x2B2A + phase × 3` |
| `> 7` | aucune couleur écrite |

La roue compte **192 entrées de trois octets** (`0x2B2A`–`0x2D69`), et la phase avance de **11 par
colonne** avec repli modulo 192 (`add a,#0x0B` puis, si `≥ 0xC0`, `add a,#0x40`). C'est cela qui
produit l'arc-en-ciel réparti sur le clavier.

Ensuite `fcn.0000A17B([0x08C3], 0xC480)` applique la position de défilement, puis
`rgb_apply_brightness`. La boucle interne parcourt les six lignes et ne dessine que si la position
existe :

```c
if (CODE[0xC500 + colonne * 6 + ligne] != 0xFF) { ... }
```

##### `CODE 0xC500` — la carte de présence des touches

Quinze colonnes de six octets, `0xFF` = pas de touche à cette position.

```
col  0 : 00 01 02 03 04 05        col  8 : 40 41 42 43 4c 45
col  1 : ff 09 0a 0b 14 0d        col  9 : 48 49 4a 4b 54 4d
col  2 : 10 11 12 13 1c 15        col 10 : 50 51 52 53 5c 55
col  3 : 18 19 1a 1b 24 ff        col 11 : 58 59 5a 5b 64 ff
col  4 : 20 21 22 23 2c ff        col 12 : 60 61 62 63 0c 65
col  5 : 28 29 2a 2b 34 2d        col 13 : 68 69 6a 6b 6c 6d
col  6 : 30 31 32 33 3c ff        col 14 : 70 71 72 73 74 75
col  7 : 38 39 3a 3b 44 ff
```

**Six trous sur quatre-vingt-dix positions, donc 84 emplacements**. Les valeurs suivent
`colonne × 8 + ligne`, avec des exceptions visibles (`0x14` en col 1, `0x0C` en col 12) — ce sont
des positions déplacées.

#### Les deux tables de couleur n'ont pas le même ordre d'octets

Vérifié instruction par instruction, pas déduit du C :

| Table | `+0` | `+1` | `+2` |
| --- | --- | --- | --- |
| palette `0xC800` | `0x011D` | `0x0000` | `0x0E25` |
| couleur par touche `0x0428` | `0x011D` | `0x0000` | `0x0E25` |
| roue de teintes `0x2B2A` | `0x0E25` | `0x0000` | `0x011D` |

**La roue est rangée à l'envers de la palette.** L'octet du milieu est le même dans les trois cas.
La section suivante établit que `0x011D` est le rouge, `0x0000` le vert et `0x0E25` le bleu : les
deux premières tables sont donc en `(R,G,B)` et la roue en `(B,G,R)`.

#### Laquelle est le rouge — tranché

La question restait ouverte : trois composantes, aucune preuve de leur couleur. La chaîne complète
la referme, et chaque maillon est vérifié au désassemblage.

**Maillon 1 — `rgb_apply_brightness` (`0xA0F0`).** Trois multiplications, chacune suivie d'une
division par 80 (`mov r5,#0x50 ; lcall math_div16by8`) :

| Source | Résultat 16 bits |
| --- | --- |
| `0x011D` | `0x0E46:0x0E47` |
| `0x0000` | `0x0E42:0x0E43` |
| `0x0E25` | `0x0E3F:0x0E40` |

Le gain vient de `CODE 0x2937 + [0x0D19]`, et vaut au plus 80 : le quotient tient donc sur un
octet, celui de poids faible.

> **Correction.** Une note antérieure appariait `0x0000, 0x011D, 0x0E25 → 0x0E46, 0x0E42, 0x0E3F`,
> dans cet ordre. C'est faux : `0x011D` va vers `0x0E46`, `0x0000` vers `0x0E42`. Les deux premières
> sont croisées — et c'est précisément l'appariement dont dépend toute la suite.

**Maillon 2 — `rgb_pixel_write` (`0x761E`).** Il ne lit que l'octet de poids faible de chaque
quotient, et les range à trois offsets consécutifs :

```asm
0x765B  add a,#0x52   ; dptr = 0x0152 + colonne x 18, puis += ligne x 3
0x7678  mov a,r3      ; r3 = [0x0E47]   -> offset +0
0x7684  add a,#0x53   ; +1
0x7695  mov a,r5      ; r5 = [0x0E43]   -> offset +1
0x76A4  add a,#0x54   ; +2
0x76C1  mov a,r7      ; r7 = [0x0E40]   -> offset +2
```

**Maillon 3 — la table de destination est déjà connue.** `XRAM 0x0152 + colonne × 18 + ligne × 3`,
soit 21 × 18 = **378 octets** (`0x0152`–`0x02CB`) : exactement la table que la commande hôte `0x08`
sous-index 2 remplit, et dont l'opcode `0x42` relit 378 octets. Le pilote OpenRGB y écrit
`buf[0x08 + i*3] = R, G, B` avec `i = colonne × 6 + ligne`.

**Conclusion.** L'offset `+0` de cette table est le rouge, donc :

| Adresse | Composante | Mise à l'échelle |
| --- | --- | --- |
| **`0x011D`** | **rouge** | `0x0E46:0x0E47` |
| **`0x0000`** | **vert** | `0x0E42:0x0E43` |
| **`0x0E25`** | **bleu** | `0x0E3F:0x0E40` |

Les étiquettes `g_color_r` et `g_color_g` étaient donc **inversées** dans le montage Ghidra ;
corrigé dans `tools/gh_setup.py`, ainsi que `g_scaled_r` / `g_scaled_g`.

Il en découle l'ordre des trois tables :

| Table | Ordre réel |
| --- | --- |
| palette `0xC800` | **(R, G, B)** |
| couleur par touche `0x0428` | **(R, G, B)** |
| roue de teintes `0x2B2A` | **(B, G, R)** |

La table « rouge vert bleu jaune magenta cyan blanc » de la palette d'usine est donc **correcte**.

##### Contrôle : la roue lue en (B, G, R) est un cercle des teintes

```
phase   0  ->  R=  0 G=  1 B=255     bleu
phase  32  ->  R=  0 G=255 B=248     cyan
phase  64  ->  R=  1 G=255 B=  0     vert
phase  96  ->  R=255 G=248 B=  0     jaune
phase 128  ->  R=255 G=  0 B=  1     rouge
phase 160  ->  R=248 G=  0 B=255     magenta
```

Six segments de 32 pas, et **les 192 entrées ont toutes un maximum de 255** — saturation constante,
la signature d'une roue de teintes. Avec un pas de 11 par colonne, les quinze colonnes couvrent
165 des 192 phases : le dégradé fait presque un tour complet sur la largeur du clavier.

### Les deux autres moteurs — `0xEE6A` et `0x746A`

#### `0xEE6A` — l'effet d'indice 0 est l'extinction

Douze octets, deux appels :

```asm
0xEE6A  clr a ; mov r3,a ; mov r5,a ; mov r7,a
0xEE6E  lcall rgb_fill_solid        ; (0, 0, 0)
0xEE71  clr a ; mov r7,a
0xEE73  ljmp  rgb_clear_key_state   ; (0)
```

**`rgb_fill_solid` (`0xADA2`)** prend un triplet dans `r7`/`r5`/`r3`, le pose en `0x0EE3`–`0x0EE5`,
puis balaie **21 colonnes × 6 lignes** et écrit chaque position via `rgb_key_suppressed` puis la
chaîne d'écriture (`0x7631`). Elle remet `RSTSTAT` (SFR `0xB1`) à zéro **à chaque itération** — un
coup de chien de garde, la boucle de 126 positions étant assez longue pour en avoir besoin. Huit
autres sites l'appellent, tous groupés en `0x8736`–`0x8785` : ce sont les couleurs fixes des
touches-témoins.

**`rgb_clear_key_state` (`0x9078`)** écrit son argument dans les **deux tableaux du moteur
réactif** — les trois octets de couleur en `0x0428 + col × 18 + ligne × 3` et l'intensité en
`0x0017 + col × 6 + ligne` — sur 21 colonnes et 6 lignes.

> Donc l'effet d'indice 0 **n'anime rien** : il éteint la table LED et remet à zéro l'état du
> moteur réactif, toutes les 100 trames. C'est le mode « rétroéclairage coupé », réaffirmé
> périodiquement plutôt que posé une fois.

#### `0x746A` — l'onde concentrique

Appelée avec `r7 = [0x08C3]`, le rayon, que le gestionnaire d'indice 1 incrémente à chaque trame et
**borne à 9**. Ce rayon devient la borne de la boucle externe : `[0x0EE3]` va de 0 à `rayon − 1`.

Chaque tour lit un **groupe** de la table `CODE 0x2959`, treize octets par groupe, `0xFF` = case
vide. Les valeurs sont des positions **empaquetées `colonne × 8 + ligne`** — le même codage que la
carte `0xC500`.

| Groupe | Positions | Lecture |
| --- | --- | --- |
| 0 | 1 | `c7,l2` — **le centre** |
| 1 | 6 | la couronne immédiate |
| 2 | 11 | |
| 3–7 | 11 à 13 | |
| 8 | 12 | le bord |

Neuf groupes, `0x2959`–`0x29CD`. **C'est une onde qui part du centre du clavier** (colonne 7,
ligne 2) et gagne une couronne par trame jusqu'au bord. La phase de la roue de teintes avance de
**13 par couronne**, ce qui colore chaque anneau différemment.

Les deux tables `CODE 0x2DED` et `CODE 0x2F6B`, indexées `(v & 7) × 21 + (v >> 3)`, sont les mêmes
que celles de `0x7C12` — la conversion position → coordonnées passées à `rgb_key_suppressed`.

#### Les deux moteurs ne lisent pas la roue dans le même ordre

Vérifié au désassemblage des deux côtés :

| Fonction | `+0` | `+1` | `+2` | Ordre |
| --- | --- | --- | --- | --- |
| `0x746A` (`0x74AD`, `0x74C2`, …) | `0x011D` | `0x0000` | `0x0E25` | **(R, G, B)** |
| `0x7C12` (`0x7CA4`, `0x7CB9`, `0x7CD1`) | `0x0E25` | `0x0000` | `0x011D` | **(B, G, R)** |

Leurs branches *palette* sont pourtant **identiques** — les deux rangent `+0` dans `0x011D`
(`0x7508` et `0x7C4F`). Seule la branche roue diverge.

> Deux lectures possibles : soit c'est délibéré et l'un des deux effets affiche l'arc-en-ciel
> avec rouge et bleu échangés, soit c'est une étourderie du firmware d'origine. **Le dump ne
> permet pas de trancher** — il faudrait voir les deux effets tourner. Je le signale parce que
> quiconque réimplémente ces effets reproduira l'un ou l'autre sans le savoir.


#### `0x0EE4` n'est pas un drapeau partagé

`0x64F1` y écrit `1` et s'en sert comme **graine du masque** (`1 << ligne`) ; `0x7C12` y écrit `0`
puis s'en sert comme **phase de la roue de teintes**. Même octet, deux usages sans rapport — un
recyclage de variable par le compilateur. Conclure à un drapeau d'état partagé aurait été une
erreur naturelle et fausse.

#### `fcn.0000598F` — le filtre d'extinction

Appelée par les deux moteurs avec `(ligne, colonne)`, elle renvoie dans `R4` un « ne pas
allumer ». Son corps est une longue suite de `if (ligne == a && colonne == b && condition) return`,
les conditions portant sur `g_transport`, `0x0328` (slot Bluetooth courant), `0x030A`, `0x09BA` et
une dizaine de bits d'IDATA. **C'est la logique des touches-témoins** : celles qui affichent le
slot apparié ou le mode de liaison sont retirées de l'animation pour être pilotées à part.

#### Encore une divergence de bornes

r2 donne à `0x64F1` **onze octets** — il coupe à `0x64FB` parce que `0x64FC` est la cible d'un saut
venant de `0x66F5`. Ghidra donne `0x64F1`–`0x66F8`, soit **520 octets**, et c'est lui qui a raison :
le corps continue sans `ret`. C'est l'inverse exact du cas `0x7AAD`, où r2 avait raison. **Aucun des
deux n'est autorité ; il faut regarder le flot.**

### `0x9DA4` et `0x5BE9` — la couleur unique et l'automate

#### D'abord `rgb_scroll_step` (`0xA3F4`), que les deux emploient

```asm
0xA3F4  jnb 0x24.3, 0xA42D        ; le bit choisit le SENS
        ; sens avant, pas selon l'effet [0x0896] :
        ;   0x0A        -> +20
        ;   0x0B, 0x11  -> +2
        ;   sinon       -> +1
0xA421  mov dptr,#0x08C3 ; movx a ; clr c ; subb a,r7
0xA427  jc  0xA462 ; clr a ; movx @dptr,a     ; si position >= r7, repli a 0
```

Donc **`rgb_scroll_step(n)` avance `g_scroll_pos` (`0x08C3`) modulo `n`**, d'un pas propre à
l'effet, et `0x24.3` inverse le sens — c'est le réglage « direction » du bloc de configuration.
`0x9DA4` et `0x746A` l'appellent avec `0xC0` (192, la taille de la roue), `0x7C12` avec `0x80`.

#### `0x9DA4` — toute la matrice d'une seule couleur (effet d'indice 3)

Cent quarante-neuf octets, et la structure la plus simple des quatre moteurs vus jusqu'ici :

```c
rgb_scroll_step(0xC0);
teinte = roue[g_scroll_pos];        /* CODE 0x2B2A + pos * 3, ordre (R,G,B) */
rgb_apply_brightness();             /* une seule fois, hors boucle */
pour chaque colonne 0..14, chaque ligne 0..5 :
    si (CODE[0xC500 + col*6 + ligne] != 0xFF)
        rgb_key_suppressed(...) ; si non supprimee : rgb_pixel_write()
```

**La couleur est calculée une fois, avant la boucle** — tout le clavier prend la même teinte, qui
défile dans la roue. Cet effet **n'interroge jamais `0x011C`** : il ignore le mode de couleur et
utilise toujours la roue.

#### `0x5BE9` — un automate de propagation à quatre directions (effets 4, 7 et 13)

Deux cent soixante-cinq octets, partagés par trois gestionnaires (`0x1BEF`, `0x1C65`, `0x1D00`).
Il ne dessine pas une figure : il fait **évoluer un état**, sur **quatre cartes de bits** de quinze
octets, un octet par colonne, six bits utiles par octet (une ligne par bit).

| Base | Sens de propagation | Opération, par trame |
| --- | --- | --- |
| `0x0001` | vers la **droite** | `A[0x0F − c] = A[0x0E − c]`, bord remis à zéro |
| `0x013B` | vers la **gauche** | `B[c − 1] = B[c]`, bord remis à zéro |
| `0x08C6` | vers le **haut** | `C[c] >>= 1` |
| `0x02E8` | vers le **bas** | `D[c] <<= 1` |

Les bases sont confirmées au désassemblage (`add a,#0xE8 ; addc a,#0x02`, `mov a,#0xC6 ; addc a,#0x08`,
`mov a,#0x3B ; addc a,#0x01`) — le C seul ne suffisait pas, la correction d'emprunt des `CONCAT11`
est facile à mal lire.

La trame se déroule en trois phases :

1. **Tracer** — union des quatre cartes ; pour chaque bit posé, les tables de position
   `CODE 0x2DED` et `CODE 0x2F6B` donnent les coordonnées, puis un des chemins de dessin selon un
   bit d'IDATA : soit `fcn.00005114(1, …)` seul, soit les trois `0x5114(0, …)`, `0xAEB3(0, …)` et
   `0x7108(…)`.
2. **Propager** — les quatre opérations du tableau ci-dessus, plus l'effacement des deux bords
   (`0x0001` et `0x0149`).
3. **Recombiner** — `A[c] |= C[c] | D[c]` et `B[c] |= C[c] | D[c]`, ce qui fait repartir les fronts
   verticaux vers la gauche et la droite : la propagation devient **diagonale**.

Si aucun bit n'était posé (`0x0EE7` reste à 1), un drapeau d'IDATA est levé — l'animation signale
qu'elle est éteinte.

##### L'effet 13 s'auto-alimente

```c
if (g_palette_effect_idx == 13 && ++[0x08BE] > 5) {
    [0x08BE] = 0;
    bit = alterne(1, 0x20);                     /* ligne 0 ou ligne 5 */
    [0x0007] |= bit; [0x0141] |= bit;           /* colonne 6 des quatre cartes */
    [0x02EE] |= bit; [0x08CC] |= bit;
}
```

Toutes les six trames, une graine est injectée **en colonne 6**, alternativement sur la ligne 0 et
la ligne 5. Les trois autres effets qui partagent ce moteur n'ont pas cette injection : leur graine
vient d'ailleurs — vraisemblablement d'une frappe. **C'est le moteur des ondes qui se propagent
depuis les touches**, et l'effet 13 est sa version qui tourne toute seule.

> Les quatre `+6` (`0x0007`, `0x0141`, `0x02EE`, `0x08CC`) tombent exactement à `base + 6` pour les
> quatre bases : c'est le contrôle qui confirme la lecture des bases.

### `0x60C9` et `0x6C9B` — la phase par touche et le dégradé par colonne

#### L'outil commun : `fcn.0000EDA2`

```asm
0xEDA2  jnb 0x24.3, 0xEDAF
        inc r5 ; si r5 >= r7 -> r5 = 0          ; sens avant
0xEDAF  dec r5 ; si debordement -> r5 = r7 - 1  ; sens arriere
```

**Avance une valeur d'un pas, modulo `r7`, dans le sens que donne `0x24.3`.** C'est
`rgb_scroll_step` appliqué à un octet quelconque au lieu de la position globale — et c'est le même
bit de direction, donc le réglage utilisateur agit sur les deux.

#### `0x60C9` — chaque touche a sa propre phase (effet d'indice 6)

Cinq cent quarante et un octets. Double boucle 15 × 6, et pour chaque touche :

```c
phase = etat[0x0017 + col*6 + ligne];
EDA2(phase, mode == 7 ? 0xC0 : 0x60);      /* avance d'un pas, module */
etat[0x0017 + col*6 + ligne] = phase;
g_scroll_pos = phase;
couleur = (mode == 7) ? roue[phase]                     /* modulo 192 */
                      : palette[fx][mode] puis gain(0x08C3);   /* modulo 96 */
si position < 15 : rgb_key_suppressed(...) ; sinon rien
    -> rgb_apply_brightness() ; ecriture par 0x7619
```

**Le tableau `0x0017` n'est donc pas « l'intensité » en général** : c'est un octet d'état par
touche, dont le sens dépend de l'effet. `0x64F1` y range une intensité qui décroît ; `0x60C9` y
range une phase qui tourne. Le même tableau, deux significations — comme `0x0EE4` plus haut.

Résultat visuel : chaque touche parcourt la roue à son propre décalage, et toutes avancent d'un pas
par trame. Un scintillement, pas un défilement.

#### `0x6C9B` — dégradé par colonne (effet d'indice 11)

Quatre cent cinquante-quatre octets, deux moitiés quasi identiques selon `0x011C` :

```c
rgb_scroll_step(mode == 7 ? 0xC0 : 0x60);   /* une seule fois */
phase = g_scroll_pos;
pour chaque colonne 0..14 :
    couleur = (mode == 7) ? roue[phase] : palette[fx][mode] puis gain(phase);
    phase += (mode == 7) ? 7 : 10;   avec repli sur 0xC0 / 0x60
    rgb_apply_brightness();
    pour chaque ligne 0..5 : si position < 15 -> 0x5968 puis, si non supprimee, 0x7616
```

C'est **la même famille que `0x7C12`** — une couleur par colonne, la phase avançant d'un pas fixe
d'une colonne à l'autre — avec un pas différent :

| Effet | Fonction | Pas par colonne | Modulo |
| --- | --- | --- | --- |
| 2 | `0x7C12` | 11 | 192 |
| 11 | `0x6C9B` | 7 (roue) / 10 (palette) | 192 / 96 |

Sur quinze colonnes : `0x7C12` couvre 165 phases sur 192, `0x6C9B` en couvre 105 — un dégradé plus
serré, moins d'un tour complet.

#### Au passage : l'échelle d'entrées de l'écriture de pixel

`0x6C9B` n'appelle ni `rgb_key_suppressed` ni `rgb_pixel_write` directement, mais `0x5968` et
`0x7616`. Ce ne sont pas d'autres routines : ce sont des **points d'entrée plus hauts dans les
mêmes**.

| Entrée | Ce qu'elle fait avant de tomber dans la suivante |
| --- | --- |
| `0x7616` | `mov dptr,#0x0EE4` — prend les coordonnées dans `0x0EE4`/`0x0EE5` |
| `0x7619` | `movx a,@dptr ; inc dptr` — l'appelant a déjà posé `dptr` |
| `0x761B` | `mov r7,a ; movx a,@dptr ; mov r5,a` |
| `0x761E` | charge les trois composantes mises à l'échelle |
| `0x7631` | ne charge que la troisième |
| `0x7636` | le corps : bornes, adresse, écriture |

Chaque appelant entre au niveau qui correspond à ce qu'il a déjà en main. C'est une économie de
place classique en C51 — et c'est **pourquoi r2 découpe cette zone en une demi-douzaine de
« fonctions » de deux ou trois octets** : ce sont des étiquettes d'entrée, pas des routines.
Même explication pour `0x5968`, qui précède `rgb_key_suppressed` et lui prépare la coordonnée lue
dans `CODE 0x2F6B`.

#### Et la roue, encore

`0x60C9` et `0x6C9B` lisent tous deux `+0` → `0x011D`, donc **(R, G, B)** — vérifié en `0x6CC3`.
Cela fait **cinq** moteurs en `(R,G,B)` contre **un seul**, `0x7C12`, en `(B,G,R)`. L'hypothèse de
l'étourderie du firmware prend nettement le dessus sur celle de l'arc-en-ciel volontairement
inversé.

> **Ce point ne se tranchera jamais sur le dump, et on peut maintenant dire pourquoi.** Les 192
> triplets de `0x2B2A` ont été extraits et lus : le premier canal part à 255 pendant que le
> deuxième monte, puis le premier retombe, puis le troisième monte, etc. — un cercle de teintes
> parfaitement régulier, avec des rampes non linéaires (1, 3, 5, 7, 10, 14, 18, 22, 26, 32, 38,
> 44…), donc perceptuelles.
>
> Or **une roue de teintes est symétrique par échange des canaux extrêmes**. Lue en `(R,G,B)` la
> table parcourt rouge → jaune → vert → cyan → bleu → magenta ; lue en `(B,G,R)` elle parcourt
> exactement le même cercle dans l'autre sens. Les deux sont des roues valides et **aucune
> structure des données ne les distingue** : ce n'est pas un manque d'information sur cette table,
> c'est une propriété de l'objet.
>
> La question n'est donc pas « quelles couleurs » mais « dans quel sens tourne l'arc-en-ciel », et
> le coût de se tromper se limite au sens de rotation. Le portage retient `(R,G,B)`, ce que font
> cinq des six moteurs.

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

### La veille radio, ajoutée après coup

Le portage rendait `USER_SLEEP_USB` **en dur**, avec le commentaire « Pas de sans-fil sur ce
portage ». La conséquence n'était pas cosmétique : `sleep.c` décide de l'endormissement par la
**suspension du bus USB** en mode `USB`, et par son **compteur d'inactivité** en mode `RF` — et hors
mode `RF` il remet ce compteur à zéro à chaque passage. Autrement dit, **le clavier n'avait aucune
veille sur batterie.**

Le crochet suit désormais le sélecteur en direct, comme le NuPhy Air60 le fait du sien.
`sleep_task()` l'interroge à chaque passage, donc glisser le sélecteur change le critère
immédiatement.

Autour de `power_enter_powerdown()`, deux ajouts transcrits :

| Moment | Firmware d'usine | Portage |
| --- | --- | --- |
| avant | `fcn.00007D74` / `fcn.00009E39` coupent l'EUART0 (SCON, IEN1) | `rf_sleep_prepare()` : vidange l'émission, puis `IEN1 &= ~ES0` et `SCON = 0` |
| après | les deux chemins de réveil (`0x7E85`, `0x9EBF`) rappellent l'init `0xB1C2` | `rf_sleep_wake()` : `rf_uart_init()` puis réarmement de l'IRQ |

La vidange préalable est un ajout : une rafale interrompue par la mise en veille laisserait
`tx_busy` armé et **`P0.2` bloqué bas en sortie**, donc le module tenu en requête d'émission
pendant tout le sommeil.

Et le réveil réaffirme le transport sans attendre la sonde suivante — c'est la règle du superviseur
d'usine (« pas connecté, donc on redemande »), appliquée à un moment où l'on sait que l'état
mémorisé ne vaut plus rien.

> **Un point signalé à tort.** Une relecture avait conclu que `park_panel()` avait un défaut en
> mettant `P7.4` et `P4.5` — les deux broches du sélecteur — en sortie basse. Vérification faite,
> ce parking est **transcrit du firmware d'usine** (`0x7DCB`-`0x7DF1`), et `user_gpio_init()` les
> rend au sélecteur au réveil : `P4CR=0x4D` et `P7CR=0x60` les laissent toutes deux en entrée, avec
> pull-up (`P4PCR=0x6F`, `P7PCR=0xDF`). Il n'y avait rien à corriger — seulement un ordre à
> respecter, `rf_sleep_wake()` après `user_gpio_init()`.

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
| `0xEF40` | **entrée en sans-fil** : `orl IEN1,#0x40` **puis `anl USBCON,#0x7F`**, `setb 0x27.7`, `delay(20)` (2 appels depuis `0x84E9`) | IEN1, **USBCON** |
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

#### Résolu, complètement : ce qui fait entrer en 2,4 GHz

Le point ouvert n°1 — « aucune écriture directe de la valeur 1 dans `g_transport` » — se referme,
et pas seulement par le chemin du bloc persisté décrit plus bas. Il y a un **sélecteur matériel**,
et il a sa propre routine.

##### `fcn.000084E9` — le sélecteur à glissière, trois positions

Appelée depuis `tick_slow` (`0x9074`). Elle échantillonne **deux broches** :

```asm
0x84E9  jb  0xF8.4, 0x84FE     ; 0xF8 = P7  ->  P7.4
0x84FE  jb  0xB0.5, 0x8512     ; 0xB0 = P4  ->  P4.5
```

> Ghidra les nomme `F8_4` et `T1` : `T1` est le nom classique du bit `0xB0.5` quand `0xB0` est pris
> pour `P3`. C'est encore le piège de la carte SFR — sur ce MCU `0xB0` est `P4`.

| `P7.4` | `P4.5` | Position | Compteur |
| --- | --- | --- | --- |
| 1 | 1 | **filaire** | `0x08BD` |
| 1 | 0 | **Bluetooth** | `0x0960` |
| 0 | — | **2,4 GHz** | `0x02DF` |

Chaque position a son compteur ; les deux autres sont remis à zéro à chaque passage. **Dix
passages consécutifs** déclenchent la bascule. C'est un anti-rebond, pas un temporisateur.

Ces deux broches figuraient parmi les entrées « rôle inconnu » de l'init GPIO d'usine. Il en reste
sept : `P0.0`, `P0.1`, `P0.5`, `P0.6`, `P4.1`, `P4.4`, `P7.7`.

##### Les trois branches

```c
/* 2,4 GHz */
if (g_transport == 0) { setb 0x27.7; delay(20); }   /* 0xEF8D */
g_transport = 1;
IEN1 |= 0x40; USBCON &= 0x7F; setb 0x27.7; delay(20);   /* 0xEF40 */
rf_link_select(0, 0);                               /* <<< slot 0 */

/* Bluetooth */
g_transport = 2;
if (g_bt_slot < 1 || g_bt_slot > 3) g_bt_slot = 1;
IEN1 |= 0x40; USBCON &= 0x7F; setb 0x27.7; delay(20);
rf_link_select(g_bt_slot, 0);

/* filaire */
setb 0x27.7; delay(20);
rf_notify_wired();                                  /* 0xEED1 */
g_transport = 0;
IEN1 &= 0xBF;
usb_init();
delay(200);
```

`fcn.0000EED1` est la politesse de sortie : **commande `0x0E`**, dix millisecondes, **commande
`0x0B` avec le paramètre 0**, dix millisecondes.

##### Les deux temporisateurs de bascule — et l'USB qu'on éteint

Une première rédaction de cette page donnait `0xEF40` pour un simple réarmement d'IRQ. C'est faux,
et la différence compte pour un portage. Les deux thunks, désassemblés en entier :

```asm
0xEF8D  setb 0x3f           ; = 0x27.7
        mov  r7,#0x14
        ljmp 0xED03         ; ~20 ms

0xEF40  orl  IEN1,#0x40     ; l'IRQ EUART0 s'arme
        anl  USBCON,#0x7F   ; <<< le module USB s'ETEINT
        setb 0x3f
        mov  r7,#0x14
        ljmp 0xED03         ; ~20 ms
```

`0xED03` est la boucle de temporisation (`r7` en entrée, deux niveaux imbriqués). `0x3f` est
l'adresse *bit* de `0x27.7`, d'où la notation employée plus haut.

Donc **en sans-fil, le firmware d'usine coupe le périphérique USB** ; il ne se contente pas
d'ignorer l'hôte. Les trois raccourcis de changement de slot (`0x41D4`, `0x420A`, `0x423F`) le
faisaient déjà en clair — la nouveauté est que le chemin du *sélecteur à glissière* le fait aussi,
par ce thunk, et c'est celui-là que le portage suit.

##### `fcn.00003901` : la commande `0x06` est une sonde de présence

Relu en C, la branche de repos du séquenceur (documentée plus bas) n'est pas une requête d'état
ponctuelle mais un **entretien de liaison à trois coups** :

```c
if (g_state == 0 && !flag_7_2) {
    if (++[0x0150] > 99) {
        [0x0150] = 0;
        euart0_cmd06();                 /* commande 0x06 */
        if (++IDATA[0x30] > 2) {        /* trois sondes sans reponse */
            IDATA[0x30] = 0;
            0x2C.1 = 0;                 /* « liaison vivante » retombe */
            P0CR &= 0xFB; P0_2 = 1;     /* la ligne d'emission est relachee */
        }
    }
}
```

Le relâchement de `P0.2` est exactement celui que l'ISR fait en fin de rafale : la sonde sert aussi
de rattrapage si une rafale s'est perdue. C'est ce mécanisme que le portage transcrit pour donner un
sens à son indicateur « connecté » — et c'est pourquoi cet indicateur **retombe** de lui-même.

##### `fcn.0000EF5A` → `fcn.0000ED89` — la commande qui choisit la radio

```asm
0xED89  jb  0x2c.1, ret        ; pas d'emission en cours
0xED8C  jnb P4.7,   ret        ; module pret
        IDATA[0x33] = 0x01     ; en-tete
        IDATA[0x34] = 0x01     ; commande
        IDATA[0x35] = R7       ; slot
        IDATA[0x36] = R5       ; drapeau
0xED9E  lcall euart0_send      ; six octets
```

Soit sur le fil :

```
01 01 <drapeau> <slot> 00 <0x55 - somme>
```

> **Correction.** Cette page a d'abord écrit `01 01 <slot> <drapeau>`. L'ordre est l'inverse, et
> l'assembleur ci-dessus le dit déjà : `IDATA[0x35] = R7` **puis** `IDATA[0x36] = R5`. Ce qui reçoit
> `g_bt_slot` est **R5**, sur les quatre sites qui l'émettent — `0x85D7` (entrée Bluetooth), `0x44C0`,
> `0x87AD`, `0x06E6` (superviseur de lien). R7 vaut `0` partout sauf en `0x87BD`, où il vaut `1`.
>
> Ce qui tranche est `0x06E6` : en Bluetooth, quand l'état reçu ne correspond pas au slot attendu, le
> superviseur redemande avec `R5 = g_bt_slot` et `R7 = 0` (`0x072C` : `ff` = `MOV R7,A` avec `A = 0`).
> Si R7 portait le slot, le clavier réclamerait le **dongle 2,4 GHz** à chaque échec de lien
> Bluetooth : il changerait de radio pour cause de mauvaise radio. Le portage émettait dans le mauvais
> ordre ; c'est corrigé dans `rf_send_link()`.

**Le slot est l'encodage radio, et il ne vaut pas celui de `g_transport`** :

| Slot | Radio |
| --- | --- |
| `0` | **dongle 2,4 GHz** |
| `1` `2` `3` | Bluetooth, emplacements 1 à 3 |

La preuve tient en trois lignes de `tick_slow_wireless` (`0x870C`) :

```c
if (g_transport == 2)      slot = g_bt_slot;   /* 0x87AD */
else if (g_transport == 1) slot = 0;           /* 0x87BB */
else                       /* rien */;
rf_link_select(slot, 1);                       /* 0x87BF, R7 = 1 */
```

En mode 1, le seul slot possible est **0**. Et l'entrée en mode 1 de `fcn.000084E9` appelle
`rf_link_select(0, 0)` en dur. Les deux sites se recoupent.

> **Attention à la collision d'encodages.** `g_transport` vaut 0 pour *filaire* ; le slot vaut 0
> pour *2,4 GHz*. Les deux champs se croisent sur la valeur la plus dangereuse. Le portage SMK
> définit deux types distincts pour cette raison.

Le second argument vaut **0** à l'entrée dans un mode et **1** depuis le tic lent, sur le bit
`0x2C.5` que pose le raccourci d'appairage (`0x4287`). D'où la lecture « 1 = relancer
l'appairage » — cohérente avec les deux sites, mais c'est leur seule différence : *inféré*.

##### `fcn.0000870C` — le tic lent sans-fil

Trois actions temporisées, dont deux nouvelles :

- **`0x2B.7`** → réinitialisation d'usine. Le détail complet est plus bas, « L'effacement
  d'appairages, mais une relance » : ce n'est pas seulement `settings_save` et le retour visuel.
- **`0x2C.5`** → réaffirmation du lien, ci-dessus.
- un compteur d'environ trente-deux passages → **commande `0x04` avec le paramètre 3**.

#### Résolu : pas d'effacement d'appairages, mais une relance

Cette page a longtemps porté « l'effacement d'appairages » comme point ouvert, avec pour seul
candidat `fcn.0000929E`, « dont la zone remplie de `0xFF` n'est pas spécifiée ». **Le candidat est
éliminé, et la question a deux moitiés de réponse** :

- **il n'existe aucun effacement** — ce firmware n'envoie jamais au BK3632 quoi que ce soit qui
  puisse détruire un lien enregistré ;
- **il existe une relance d'appairage**, sur un raccourci dédié, et son chemin est maintenant
  remonté de bout en bout : c'est la commande `0x01` avec son drapeau à `1`.

##### `fcn.0000929E` est l'initialisation RAM du démarrage

Son unique appelant est `0x910D`, **dans `main`, sans condition**, entre `0xECC5` et
`settings_restore` (`0xA283`). Les zones qu'il remplit sont maintenant lues : `0x0964` et `0x0994`
(21 octets chacune) à `0xFF`, `0x089D` (21 octets), `0x009E` et `0x0DA1` (126 octets chacune) à zéro,
plus 9 octets en `0x08B2` et 2 en `0x097D`. Ce sont les tableaux de couleur par touche en XRAM, mis
à leur état neutre avant que les réglages ne soient relus depuis la flash. Rien à voir avec le
Bluetooth.

##### Ce que fait réellement le raccourci de réinitialisation

Deux déclencheurs posent le bit `0x2B.7`, et le consommateur est `0x8725` dans le tic lent sans fil :

| Déclencheur | Site | Comportement |
| --- | --- | --- |
| Raccourci clavier | clé `0x04` -> arm `0x41B3`, `0x41BF` pose le bit puis remet le compteur `[0x0961:0x0962]` à zéro | le consommateur exige que ce compteur 16 bits atteigne **3000** : c'est un **appui long** |
| Commande reçue | `0x0C53`, atteint par le sous-code **`0x06`** du conteneur `0x08` | pré-charge le compteur à **3001**, force le prédiviseur `[0x08DB]` à **10** et pose le bit : réinitialisation **immédiate, pilotée par l'hôte** |
| Abandon | `0x44AE` efface le bit | touche relâchée trop tôt |

> **Correction.** Cette page a d'abord écrit « sous-code `0x05` », en parsant la table inline de
> `0x0859` comme des triplets *(clé, adresse)*. C'est l'inverse — voir « Les treize arms du conteneur
> `0x08` reçu » plus bas : le répartiteur `0x4E45` lit *(adresse_hi, adresse_lo, clé)*, la clé en
> **dernier**. Toutes les clés de cette table étaient donc décalées d'un cran. Le déclencheur de
> `0x0C53` est **`0x06`**, et `0x05` arme un tout autre arm (`0x0C4A`, une lecture).
>
> La justification donnée à l'époque — « le jeu de sous-codes correspond exactement à celui que cette
> page relève côté émission » — était fausse et est retirée : les treize sous-codes **reçus**
> (`01`–`06`, `09`, `41`–`44`, `49`, `4A`) ne sont pas le jeu **émis** (`05`, `0A`, `41`–`44`, `49`,
> `4A`). Ils se recoupent sans coïncider.

Le prédiviseur `[0x08DB]` est ce qui rend la chose *immédiate* : `tick_slow` (`0x8FA7`) ne fait son
travail que lorsque `[0x08DB]` atteint **10**, et `0x819C` ne l'incrémente que d'un par tic. Le poser
à 10 fait passer la garde au tic suivant, au lieu d'attendre dix tics. C'est le pendant du compteur
pré-chargé à 3001 : l'un supprime l'appui long, l'autre supprime l'attente.

Ce bit vient du **répartiteur de raccourcis** `0x4136`, un appel au répartiteur inline `0x4E45`
suivi de dix-sept triplets `(adresse_hi, adresse_lo, clé)` — l'ordre est bien celui-là, et il se
vérifie sur l'arm de la réinitialisation, dont l'adresse tabulée `0x41B3` tombe exactement sur les
deux gardes `JNB 0x4D` / `JNB 0x55` qui précèdent le `SETB 0x2B.7` de `0x41BF` :

| Clé | Arm | Rôle |
| --- | --- | --- |
| `0x04` | `0x41B3` | **réinitialisation d'usine** — pose `0x2B.7` |
| `0x05` `0x06` `0x07` | `0x41C8` `0x41FE` `0x4233` | emplacements Bluetooth **1**, **2**, **3** (`g_transport = 2`, `g_bt_slot = 1/2/3`) |
| `0x08` | `0x426A` | **relance d'appairage** — pose `0x2C.5` |
| `0x00` `0x01` `0x02` `0x0A` `0x0B` `0x11` `0x18`–`0x1D` | `0x4170` … `0x43D5` | autres raccourcis |

Les trois arms Bluetooth se lisent d'un coup d'œil : chacun écrit `g_transport = 2` avec `A = 2`,
puis réutilise ce même `A` pour le slot — `14` (`DEC A`) pour l'emplacement 1, rien pour le 2, `04`
(`INC A`) pour le 3.

Et voici la séquence complète, `0x8728`–`0x8799`, **ses dix-huit appels énumérés** :

| Appel | Rôle |
| --- | --- |
| `0xEF8D`, `0xEF46` | marquer les réglages modifiés + 20 ms d'attente |
| `0xADA2` | `rgb_fill_solid(0,0,0)` — noir |
| `0xA069` | `settings_save` |
| **`0x91D3`** | restaure les couleurs par touche par défaut depuis `CODE 0xCB7A` vers `0x09BF`, `0x0A3D`, `0x0ABB` |
| **`0x77A0`** | restaure la **disposition d'usine** : blocs de 512 octets depuis `CODE 0xB400`, `0xB600`, `0xB800`… vers `0x09BF`, puis écriture dans les pages IAP `0x66`, `0x67`, `0x68` via `0xAAC1` |
| `0xADA2` ×6 + `0xED03` ×7 | bleu 200 ms, noir, vert, noir, rouge, noir |

Puis `CLR 0x1B` (rétroéclairage éteint), `SETB 0x51` et `SETB 0x21` (rechargement et ressemis).

**Aucun de ces dix-huit appels ne touche l'EUART0.** Pas une trame, pas une entrée dans la file
d'émission. C'est une réinitialisation de **disposition et d'éclairage**, pas d'appairages.

##### La seconde moitié du même tic : la relance d'appairage

`0x8725` teste `0x2B.7` et, **s'il est absent, saute directement en `0x8799`** — c'est-à-dire dans
la suite du tic, pas hors de lui. Or `0x8799` teste `0x2C.5`. Les deux raccourcis partagent donc le
compteur d'appui long **et** le tic qui les consomme, dans cet ordre :

```asm
0x870C  si [0x0961:0x0962] < 3000 -> 0x87F6      ; l'appui long n'est pas atteint
0x8721  compteur = 0
0x8725  jnb 0x2b.7, 0x8799                       ; pas de reinitialisation -> on tente l'appairage
0x8728  ...  reinitialisation d'usine ...
0x8793  clr 0x2c.5                               ; <-- et elle ANNULE l'appairage en attente
0x8799  jnb 0x2c.5, 0x87C2
0x879C  [0x0E3B] = 0 ; clr 0x2c.5 ; delai 20 ms
0x87A6  si g_transport == 2 -> R5 = g_bt_slot
0x87B4  sinon si g_transport == 1 -> R5 = 0
0x87BD  R7 = 1
0x87BF  lcall 0xEF5A                             ; 01 01 01 <slot> 00 <somme>
```

L'arm `0x426A` qui pose `0x2C.5` **exige `g_transport == 1`**, c'est-à-dire le 2,4 GHz. En pratique
la seule trame émise par ce chemin est donc `01 01 01 00 00 <somme>` : *relance l'appairage avec le
dongle*. Le `CLR 0x2C.5` de `0x8793` dit le reste — quand les deux bits sont posés, la
réinitialisation gagne et l'appairage est abandonné, pas mis en file.

Un second consommateur de `0x2C.5` existe, en `0x44BD`, et il émet `R7 = 0` : une simple
reconnexion. Il exige `g_transport == 2` alors que le seul poseur du bit exige `g_transport == 1` —
il est donc inatteignable sans changement de transport entre la pose et la lecture. Noté, pas
expliqué.

`0xEF5A` n'est d'ailleurs pas qu'un émetteur : après `0xED89` il efface le bit `0x69` et recharge
`IDATA[0x18] = 6`, `IDATA[0x17] = 0xFF` — le superviseur de lien repart à neuf.

##### Pourquoi le négatif est solide

Les appairages vivent dans le BK3632, pas dans le SH68F90 : les effacer demanderait une commande sur
l'EUART0. Or les **onze** commandes émises ont toutes une sémantique établie, et la dernière dont
cette page laissait le déclencheur à « — » est désormais placée : **`0x0C` est émise en `0x7D9B`,
dans la préparation de veille** (`0x7D74`), avec les paramètres `7` et `8`, quand `[0x0E3B]` est non
nul. Les codes `0x05`, `0x07` et `0x0A` ne sont **jamais** émis. Et les sous-opcodes du conteneur
`0x08` (`0x05`, `0x0A`, `0x41`–`0x44`, `0x49`, `0x4A`) sont tous des transferts de données depuis
`CODE` ou la zone IAP.

Il ne reste donc aucune commande candidate **pour effacer**. Le portage avait choisi de **ne pas
inventer** de commande d'effacement ; ce n'était pas seulement prudent, c'était juste. La seule
action d'appairage que ce firmware sache faire est la relance ci-dessus, et elle passe par une
commande que le portage émet déjà.

##### Ce que le portage pourrait reprendre, et ne reprend pas

La réinitialisation d'usine elle-même est portable — SMK a déjà `settings`, `indicators_apply_defaults()`
et une disposition par défaut compilée. Ce serait une **fonctionnalité nouvelle**, pas la réponse à
la question posée, et elle n'est pas ajoutée ici.

Côté code, il reste deux choses, et une seule a demandé une modification :

- **rien à porter pour l'effacement** — il n'existe pas ;
- **la relance existe déjà** dans `aula_rf.c` sous le nom `RF_LINK_PAIRING`, mais elle partait avec
  les deux octets inversés. C'est corrigé : `rf_send_link()` place désormais le drapeau en octet 2 et
  le slot en octet 3. Le défaut était sans effet en 2,4 GHz, où les deux valent zéro ; en Bluetooth il
  faisait partir `01 01 <slot> 00`, soit une demande de dongle assortie d'un drapeau parasite.

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

##### `XRAM 0x0D19` — la **luminosité** (27 références)

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
appliqué aux trois composantes (`0x011D` → `0x0E46`, `0x0000` → `0x0E42`, `0x0E25` → `0x0E3F`).

Le maximum est **par effet**, lu dans une table CODE :

```asm
0x8C30  mov dptr,#0x009d ; movx a,@dptr
0x8C34  mov dptr,#0xa8d5 ; movc a,@a+dptr   ; borne = CODE[0xA8D5 + effet]
0x8C39  ... compare et sature 0x0D19
```

Valeur par défaut **4** (`0x1D10`, `0x1D31`). Valeur dans le dump : **9**, soit le maximum.

> **Correction.** J'avais d'abord nommé `0x0D19` « vitesse ». C'est la **luminosité**.
> `fcn.00000056` est bien une division (`div ab`, vérifiée), donc `fcn.0000A0F0` calcule
> `x × table[0x0D19] / 80` — un facteur de 0 à 1 en dix pas. Et les trois grandeurs qu'il met à
> l'échelle sont `0x0000`, `0x011D` et `0x0E25`, toutes trois écrites **juste après la lecture de
> palette** dans `fcn.00004EA9` (`0x4ED9`, `0x4F1F`) : ce sont les trois composantes de couleur —
> respectivement **vert**, **rouge** et **bleu**, voir *Laquelle est le rouge — tranché*.
> Trois composantes, un seul facteur — c'est une commande de luminosité, pas de vitesse.

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

#### `0x0D9D` — la vitesse, et `0x0896` — la palette par effet

##### `XRAM 0x0D9D` — la vitesse (26 références)

Restaurée de l'**offset 7** du bloc (`0x1130`) et resauvegardée vers lui (`0x8F4B`), bornée par
`CODE[0xA8BC + effet]` = 4, avec la même mécanique incrémenter/saturer que les autres
(`0x8EED`–`0x8F30`).

Ce qui la caractérise : elle indexe **quinze tables CODE de cinq valeurs décroissantes**, chacune
câblée en dur dans une fonction de rendu différente.

| Table | Valeurs |
| --- | --- |
| `0x2FE9` | 120 100 80 50 20 |
| `0x2FEE` | 60 50 40 30 20 |
| `0x2FF3` | 45 35 25 15 5 |
| `0x2FF8` | 45 35 25 15 6 |
| `0x2FFD` | 80 60 40 20 8 |
| `0x3002` | 20 15 10 5 1 |
| `0x3007` | 88 68 48 28 8 |
| `0x300C` | 115 100 85 60 30 |
| `0x3011` | 120 90 70 45 1 |
| `0x3016` | 30 24 18 12 6 |
| `0x301B` | 32 24 18 16 6 |
| `0x3020` | 115 95 75 55 30 |
| `0x3025` | 32 24 16 8 1 |
| `0x302A` | 46 36 26 16 6 |
| `0x302F` | 50 40 30 20 8 |

**Quinze tables — et la famille A compte exactement quinze effets (0 à 14).** Cinq valeurs, et le
maximum vaut 4. Décroissantes : plus l'indice monte, plus la période est courte. C'est la vitesse,
avec une plage propre à chaque effet.

Valeur dans le dump : **4**, soit le maximum.

##### `XRAM 0x0896` — l'effet dont on lit la palette (23 références)

> **Son unique écrivain est identifié** : `0x16EC`, le verrou de changement d'effet. Voir plus bas,
> « L'écrivain de `0x0896` — c'est un verrou, pas une variable libre ».

Les fonctions de rendu font `0xC800 + [0x0896] × 21`. Les valeurs auxquelles `0x0896` est comparé
sont parlantes : `xrl a,#0x0d` (`0x5E0C`, `0x6630`), `cjne a,#0x0A` (`0xA3F7`, `0xA42D`),
`xrl a,#0x01` (`0x7476`), et `add a,#0xE0` — c'est-à-dire une comparaison à `0x20`. **Ce sont les
plages des deux familles d'effets.**

Vérification sur le dump, en supposant `0x0896` = index d'effet :

| Effet | Adresse | Page | Palette |
| --- | --- | --- | --- |
| `0x00` | `0xC800` | 100 | rouge vert bleu jaune magenta cyan blanc |
| `0x04` | `0xC854` | 100 | *idem* |
| `0x0E` | `0xC926` | 100 | *idem* |
| `0x20` | `0xCAA0` | 101 | tout noir |
| `0x2D` | `0xCBB1` | 101 | tout noir |

Les quinze effets de la famille A portent chacun leur palette de sept couleurs, toutes réglées
sur la palette d'usine ; ceux de la famille B sont à zéro — ils n'utilisent pas de palette, ce qui
est cohérent avec leur maximum de couleur plus bas (4 au lieu de 7). Les adresses tombent
entièrement dans les pages 100 et 101 de la zone IAP.

##### L'écrivain de `0x0896` — c'est un verrou, pas une variable libre

Cette page a longtemps porté « l'écrivain de `XRAM 0x0896` » comme point ouvert, en relevant que
l'aiguillage d'animation branche sur `0x0896` **et non** sur `0x009D`. Les deux questions n'en font
qu'une, et la réponse les règle ensemble.

Sur les **23 références** à `0x0896`, **une seule est une écriture** : `0x16EC`.

```
16A6  jb   0x51, 0x16D7            ; deja marque « a recharger »
16A9  mov  dptr,#0x0ED0 ; movx a,@dptr ; mov r6,a
16AE  mov  dptr,#0x0896 ; movx a,@dptr
16B2  cjne a,r6,   0x16D7          ; l'effet a-t-il change ?
16B5  mov  dptr,#0x0897 ; movx a,@dptr
16B9  cjne a,r7,   0x16D7          ; et le second parametre ?
...                                 ; idem 0x0ED3/0x09AF (luminosite) et 0x0ED4/0x0306 (vitesse)
16D4  ljmp 0x1AEA                  ; rien n'a bouge -> rendu normal
16D7  clr  0x51
16DE  mov  dptr,#0x0896 ; movx a,@dptr ; cjne a,r6, 0x16EC
16E5  mov  dptr,#0x0897 ; movx a,@dptr ; xrl a,r7 ; jz 0x1702
16EC  mov  dptr,#0x0896 ; mov a,r6 ; movx @dptr,a    <== L'UNIQUE ECRITURE
16F1  mov  dptr,#0x0897 ; mov a,r7 ; movx @dptr,a
16F6  setb 0x21                    ; ressemis du plan par effet
16F8  clr  0x22
16FC  lcall 0xEDE7
16FF  lcall 0xEE6A                 ; et on efface le panneau
```

C'est un **comparateur-verrou** : il ne recopie que si la valeur a changé, et le changement
déclenche deux choses — le bit `0x21`, qui commande le ressemis du plan par effet (c'est lui que
`0x1A9E` teste avant d'appeler le transposeur `0xACA3`), et l'effacement complet du panneau.

**Et la source, c'est bien `0x009D`.** Trente octets plus haut :

```
160D  mov dptr,#0x009D ; movx a,@dptr
1611  mov dptr,#0x0ED0 ; movx @dptr,a     ; [0x0ED0] = [0x009D]
1615  ... [0x0ED3] = [0x0D19] (luminosite), [0x0ED4] = [0x0D9D] (vitesse)
162A  jnb 0x1B, +5 : [0x0ED0] = 0
1632  jnb 0x6C, +5 : [0x0ED0] = 0
163A  jnb 0x5B, +5 : [0x0ED0] = 0
```

D'où la chaîne complète, en quatre temps :

> **`0x009D`** (effet *choisi*, réglage persistant, douze écrivains dont les raccourcis qui posent
> `0x20`, `0x26` et `0x2D`) **→ `0x0ED0`** (effet *demandé*, recopié à chaque tour) **→ trois portes**
> qui le forcent à 0 **→ `0x0896`** (effet *actif*, verrouillé au changement).

Les trois portes sont des bits d'état, et ils expliquent pourquoi la valeur effective peut différer
du réglage :

| Bit | Rôle | Écrivains |
| --- | --- | --- |
| `0x1B` (`0x23.3`) | **rétroéclairage allumé** | basculé par un raccourci (`b2 1b` en `0x42AC`, suivi du marquage « réglages modifiés »), effacé à l'endormissement sans fil (`0x8797`), reposé au réveil (`0x8816`) |
| `0x6C` (`0x2D.4`) | le bit qui coupe l'USB, déjà documenté plus bas — **seul écrivain : `fcn.00001DC3`**, le gestionnaire d'énergie | `0x1F25` / `0x1EF5`, `0x1F50` |
| `0x5B` (`0x2B.3`) | second bit d'énergie, écrit dans la même fonction | `0x1F8E` / `0x1EF7`, `0x1F52`, `0x7EBF` |

**Voilà pourquoi l'aiguillage lit `0x0896` et pas `0x009D`** : `0x009D` est ce que l'utilisateur a
choisi, `0x0896` est ce qui doit réellement s'afficher une fois passées les portes d'énergie et
l'interrupteur de rétroéclairage. Ce n'était pas une variable orpheline, c'était le verrou de sortie
d'un comparateur.

##### `0x0897`, l'autre moitié du verrou

Le comparateur teste **une paire**, pas une valeur : `cjne a,r7` en `0x16B9`, et `R7` vient de
`0x1619` — `mov dptr,#0x011C ; movx a,@dptr ; mov r7,a`. `0x011C` est le **mode couleur** (celui que
`0x1D2B` force à 7 pour le tirage aléatoire, et que `0x5108` consulte). `R7` survit intact jusqu'au
comparateur.

Donc `0x0897` est la **copie verrouillée du mode couleur**, et le verrou porte sur le couple
*(effet, mode couleur)*. C'est cohérent : changer de mode couleur demande le même ressemis et le même
effacement que changer d'effet. Les moteurs, eux, lisent le mode courant en `0x011C` directement —
`0x0897` ne sert qu'à détecter le changement.

##### `0x26` et `0x2D` ne sont pas des effets, ce sont des états transitoires

Trois raccourcis posent une valeur littérale dans `0x009D` : `0x20`, `0x26` et `0x2D`. Les deux
derniers sont **symétriques**, et ils sauvegardent d'abord :

| Site | Ce qu'il fait |
| --- | --- |
| `0x9184` | `[0x0E24] = effet courant` puis `[0x009D] = 0x26` |
| `0xA48B` | `[0x0E24] = effet courant` puis `[0x009D] = 0x2D` — et seulement si l'effet courant vaut `0x20` (`cjne a,#0x20` en `0xA47E`) |
| `0x1D40` | `[0x009D] = [0x0E24]` — la restauration, dans le gestionnaire de `0x26` |
| `0xAFB8` | `[0x009D] = [0x0E24]` — la seconde restauration, précédée d'un test `cjne a,#0x20` sur la valeur sauvegardée |

**`0x26` est une animation de transition.** Son gestionnaire `0x1D37` lit `XRAM 0x0ECD` — le sens
vertical du serpent — et **dès qu'il repasse à zéro**, c'est-à-dire quand le balayage a fini de
parcourir la grille, il efface le panneau, restaure `[0x0E24]` et repose le drapeau de rechargement.
L'usine s'en sert comme d'un habillage de changement de mode, pas comme d'un effet qu'on choisit.

**`0x2D` ne rend rien du tout.** L'aiguillage ne teste que `0x20` et `0x26` avant sa borne
`cjne a,#0x12` ; `0x2D` la dépasse et tombe sur `0x1DC2`, un `ret`. Sa ligne de palette est
d'ailleurs entièrement noire. C'est un état où le moteur d'animation se tait délibérément, entré
depuis le mode gaming et restauré par `0xAFB8`.

**Conséquence pour le portage** : `AULA_FX_SNAKE_RGB` reste un effet permanent chez nous. Son rendu
est transcrit, mais sa **durée** est notre choix — SMK n'a pas de notion d'effet transitoire et notre
liste se parcourt à la touche. C'est écrit dans `indicators.c`. Quant à `0x2D`, il n'a pas de
contrepartie et n'en a pas besoin : ne rien afficher, c'est `AULA_FX_OFF`.

##### Ce que ça change pour le portage : rien, et c'est en soi un résultat

Le portage recalcule à chaque passage depuis `user_settings.led_effect`, et `fx_reset()` — appelé à
chaque changement d'effet — joue exactement le rôle du couple `SETB 0x21` + `LCALL 0xEE6A`
(ressemis de l'état des semeurs, puis `aula_rgb_clear()`). Les trois portes ont leurs équivalents
ailleurs dans SMK : l'extinction est l'effet `AULA_FX_OFF`, et la veille passe déjà par
`indicators_pwm_disable()` depuis `sleep.c` et `user_sleep_prepare()`. Aucune ligne à ajouter — la
convergence est vérifiée, pas supposée.

##### L'offset 5 est l'aiguillage global / par effet

```asm
0x111B  mov dptr,#0x0312 ; movx a,@dptr
0x111F  jz 0x1144                        ; == 0 -> parametres PAR EFFET
        ... lit les offsets 8, 6, 7  (0x0315, 0x0313, 0x0314)  -> globaux
0x1144  ... lit le tableau par effet
```

Le dump donne l'offset 5 à **0**, donc le clavier était en mode **par effet** — ce qui explique
que la luminosité effective soit 9 (issue de l'enregistrement) et non 4 (la copie globale à
l'offset 6).

##### Les bits 6-4 de `b1` — c'est bien la vitesse

> **Correction d'une correction.** Au tour précédent j'ai retiré cette piste en la déclarant
> fausse. Elle était juste : je n'avais cherché que les masques `0x0F` et `0x8F`, et raté
> `anl a, #0xF0` en `0x11A8`.

Un **quatrième** accès à `b1`, dans la branche « par effet » de `fcn.00001111` :

```asm
0x119D  mov a,#0x46 ; add a,r5 ; ...      ; DPTR = 0x0346 + 2e   (b1)
0x11A7  movx a,@dptr
0x11A8  anl a,#0xf0                        ; bits 7-4
0x11AF  mov r0,#0x04 ; lcall fcn.00004DD8  ; decalage de 4 bits a droite
0x11B4  mov dptr,#0x0d9d ; ...             ; -> vitesse
```

`fcn.00004DD8` décale `r4:r5:r6:r7` de `r0` bits vers la droite — le pendant de `fcn.00004DC5`
déjà rencontré pour la table de descripteurs. Et la réécriture, en `0x8F6A`, est l'exacte
inverse :

```asm
anl  a, #0x8f                   ; garde le bit 7 et les bits 3-0 de b1
mov  dptr,#0x0d9d ; movx a,@dptr
mov  B,#0x10 ; mul ab           ; vitesse x 16
orl  a, r7                      ; b1 = (b1 & 0x8F) | (vitesse << 4)
```

La vitesse étant bornée à 4, `vitesse << 4` ne dépasse jamais `0x40` et ne touche donc jamais le
bit 7. **En pratique `b1[6:4]` est la vitesse**, et `b1[7]` est un bit libre que l'écriture
préserve.

#### La symétrie complète

| Paramètre | Global | Par effet | Borne par effet |
| --- | --- | --- | --- |
| luminosité | offset 6 — `0x0313` | `b0[4:0]` | `CODE[0xA8D5 + e]` = 9 |
| vitesse | offset 7 — `0x0314` | `b1[6:4]` | `CODE[0xA8BC + e]` = 4 |
| couleur | offset 8 — `0x0315[3:0]` | `b1[3:0]` | `CODE[0xA8A3 + e]` = 7 |
| sens | offset 8 — `0x0315[7]` | `b0[7]` | drapeau `0x24.3` |

Chaque paramètre existe donc en **deux exemplaires**, et l'offset 5 dit lequel fait foi.

##### `XRAM 0x0312` (offset 5) — l'aiguillage

Six références, **toutes des lectures `jz`**, et — comme `0x031B` — aucune écriture directe : il
est posé par le protocole de configuration via le pointeur de bloc.

| Site | Rôle |
| --- | --- |
| `0x111B` | restauration : branche globale ou branche par effet |
| `0x7990`, `0x7A26` | idem, dans `fcn.00007928` |
| `0x8C85` | incrément **luminosité** — persister vers `0x0313` ou vers `b0` |
| `0x8F45` | incrément **vitesse** — vers `0x0314` ou vers `b1[6:4]` |
| `0x93CE` | incrément **couleur** — vers `0x0315` ou vers `b1[3:0]` |

Les trois gestionnaires de touches consultent donc le même bit pour savoir **où écrire** la
valeur modifiée. Dump : **0** → mode par effet.

##### `XRAM 0x0315` (offset 8) — l'homologue global de `b1`

```asm
0x1121  movx a,@dptr ; rlc a ; mov 0x24.3, c    ; bit 7  -> sens
0x1138  movx a,@dptr ; anl a,#0x0f ; -> 0x011C  ; bits 3-0 -> couleur
0x93D4  a = [0x0315] & 0x80 ; a |= [0x011C] ; [0x0315] = a     ; reecriture
```

Dump : `0x07` → couleur 7 (arc-en-ciel), sens 0. Cohérent avec l'enregistrement par effet
(`b1 = 0x37` → couleur 7) — les deux exemplaires sont en accord.

##### Le drapeau `0x24.3` — le sens de l'animation

Sept sites. Écrit depuis le bit 7 du réglage (`0x1126` en global, `0x1172` en par-effet), effacé
par deux fonctions de rendu (`0x7474`, `0xEF8A`), lu par trois. Le plus parlant est
`fcn.0000A3F4` :

```asm
0xA3F4  jnb 0x24.3, 0xA42D
0xA3FF  mov dptr,#0x08c3 ; movx a,@dptr ; add a,#0x14    ; +20
...
0xA435  mov dptr,#0x08c3 ; movx a,@dptr ; add a,#0xec    ; -20
```

**`+20` d'un côté, `−20` de l'autre**, sur le même compteur de position `0x08C3` (qui reboucle
sur `cjne a,#0x11`, soit 17). C'est un **sens de défilement**.

##### Relecture finale du clavier dumpé

| | Global | Par effet (effet 4, `09 37`) |
| --- | --- | --- |
| luminosité | 4 | **9** |
| vitesse | 4 | **3** |
| couleur | 7 (arc-en-ciel) | **7** (arc-en-ciel) |
| sens | 0 | **0** |

Offset 5 = 0, donc ce sont les valeurs **par effet** qui s'appliquent.

##### Trois tables de bornes par effet

`0xA8A3`, `0xA8BC`, `0xA8D5` — espacées de `0x19` = 25 :

| Base | Borne de | Valeur famille A | Valeur famille B |
| --- | --- | --- | --- |
| `0xA8A3` | `0x011C` (couleur) | 7 | 4 |
| `0xA8BC` | `0x0D9D` (vitesse) | 4 | — |
| `0xA8D5` | `0x0D19` (luminosité) | 9 | 9 |

Les trois affectations sont vérifiées par désassemblage direct : `0x8C34` pour `0xA8D5`,
`0x939F` pour `0xA8A3`, `0x15EB` / `0x8EF4` / `0x8F0B` pour `0xA8BC`.

##### L'enregistrement se relit entièrement

Pour l'effet courant du clavier dumpé (index 4, offset 64, octets `09 37`) :

| Bits | Valeur | Sens |
| --- | --- | --- |
| `b0[4:0]` | 9 | **luminosité**, au maximum |
| `b0[7]` | 0 | drapeau `0x24.3` |
| `b1[3:0]` | 7 | **couleur** = arc-en-ciel |
| `b1[6:4]` | 3 | non lu par les masques identifiés — **non attribué** |
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

### La table de remap décodée — la disposition d'usine

La table de `0xCE00` (page 103) contient **84 enregistrements non nuls sur 128**, dont les
indices 0 à 89. Lus **colonne par colonne**, six lignes par colonne, ils donnent ceci :

```
      c0    c1    c2    c3    c4    c5    c6     c7     c8    c9    c10   c11   c12   c13   c14
 L0   Esc   .     Lum+  Lum-  Tab   H     cl3/2  cl3/1  Prec  Play  Suiv  Muet  Vol-  Vol+  (07)
 L1   `     1     2     3     4     5     6      7      8     9     0     -     =     BSpc  Del
 L2   Tab   Q     W     E     R     T     Y      U      I     O     P     [     ]     \     PgUp
 L3   Caps  A     S     D     F     G     H      J      K     L     ;     '     #     Ent   PgDn
 L4   MOD2  Z     X     C     V     B     N      M      ,     .     /     MOD32 \|    Up    End
 L5   MOD1  MOD4  MOD8  .     .     Spc   .      .      Fn?   MOD16 .     .     Left  Down  Rght
```

**Les lignes 1 à 4 sont le bloc QWERTY exact**, colonne par colonne, sans une seule anomalie. La
ligne 5 est la rangée du bas, la ligne 0 la rangée de fonction. Cela valide d'un coup la géométrie
6 × 15 et l'ordre de balayage établis par ailleurs — matrice, pinout et keymap se recoupent.

#### Correction : `b0` n'est pas un masque de modificateurs

J'avais lu l'enregistrement comme `[modificateurs, classe, paramètre, usage]`, avec `0x02` =
LeftShift. C'est faux, et le décalage de 24 bits le montre :

```asm
mov r0, #0x18 ; lcall fcn.00004DC5      ; decalage de 24 bits
mov dptr, #0x0d18 ; mov a, r7 ; movx @dptr, a
```

`b0` va en **`XRAM 0x0D18`** — l'octet que j'avais déjà identifié comme celui recopié dans le
tampon de rapport HID (`0x0D18 → 0x08C0`). C'est donc un **sélecteur de rapport**, pas un masque.

Ce qui explique les entrées qui n'avaient aucun sens en modificateurs : `b0 = 0x02` avec
`b3 = 0xB5` ne peut pas être « LeftShift + usage clavier 0xB5 », parce que `0xB5` n'est pas un
usage clavier. C'est un usage **Consumer** :

| `b3` | Usage Consumer |
| --- | --- |
| `0x6F` / `0x70` | luminosité écran − / + |
| `0xB5` / `0xB6` | piste suivante / précédente |
| `0xCD` | lecture / pause |
| `0xE2` | muet |
| `0xE9` / `0xEA` | volume + / − |

**La rangée de fonction porte donc les commandes multimédia, pas F1–F12** — le réglage d'usine a
la couche Fn inversée, ce qui est l'usage courant sur ces claviers.

L'enregistrement se lit finalement :

```
b0  selecteur de rapport   -> 0x0D18   (0x00 clavier, 0x02 consumer, autres a determiner)
b1  classe d'action        -> 0x0D17
b2  parametre              -> 0x0D16
b3  usage                  -> 0x0D15
```

#### Les six modificateurs, et la tension sur `0x0D17` levée

Six positions portent une **puissance de deux** en classe d'action, sans usage :

| Position | Classe | Touche |
| --- | --- | --- |
| L4 c0 | 2 | Maj gauche |
| L4 c11 | 32 | Maj droite |
| L5 c0 | 1 | Ctrl gauche |
| L5 c1 | 4 | Win gauche |
| L5 c2 | 8 | Alt gauche |
| L5 c9 | 16 | Ctrl droite |

Ce sont **exactement les six touches modificatrices**, et rien d'autre dans la table ne porte une
puissance de deux. Cela lève la tension notée depuis longtemps sur `0x0D17` : le champ est bien
utilisé **à la fois** comme index de classe (0 à 9, via la table de sauts `0xA68D`) et comme
**masque** (le `0x08B2 &= ~0x0D17` de `0x7058`) — selon qu'il désigne une action ou un
modificateur. La borne `< 10` du dispatcher laisse passer 1, 2, 4 et 8 mais rejette 16 et 32, qui
n'existent que sur la voie masque.

> *À noter :* l'ordre des bits ne suit pas celui des modificateurs HID (`0x04` tombe sur Win et
> `0x08` sur Alt, alors que HID dit l'inverse). Le masque s'applique à `0x08B2`, un octet d'état
> interne — sa numérotation n'a pas de raison de coïncider avec celle du rapport HID, et la
> conversion se fait ailleurs.

#### Deux entrées à part

- **L5 c8** (indice 53) : `0D 00 00 00` — un sélecteur de rapport `0x0D`, aucune classe, aucun
  usage. La position correspond à **Fn** sur un 75 % ANSI, touche que le firmware traite en
  interne sans jamais l'émettre.
- **L0 c14** (indice 84) : `07 00 00 1D` — rapport `0x07`, usage `0x1D`. C'est la position que
  l'analyse de la matrice avait attribuée à **l'appui sur l'encodeur**.

*Ces deux lectures sont inférées de la position ; le contenu des enregistrements, lui, est lu.*

#### `cl3/2` et `cl3/1` — la luminosité RGB

Les deux entrées `08 03 02 00` et `08 03 01 00` de la rangée de fonction sont la classe d'action
**3**, dont le handler est `0x8C0F`. Le C est sans ambiguïté :

```c
if (g_key_hid_usage != 0) goto rapport;     /* une entree porteuse d'usage saute tout ceci */
switch (g_key_param) {                       /* b2 de l'enregistrement */
  case 1:  if (brightness < tbl_max_brightness[effect]) brightness++;  break;   /* monter */
  case 2:  if (brightness != 0)                          brightness--;  break;   /* descendre */
  case 0:  brightness++; if (brightness > max) brightness = 0;          break;   /* cycler */
}
```

Donc **`cl3/1` = luminosité +**, **`cl3/2` = luminosité −**, et le paramètre `0` cyclerait. Sur la
rangée de fonction, ce sont les deux touches de luminosité du rétroéclairage.

Atteindre une butée (0 ou le maximum) pose `0x2B.6` et charge `0x0D1B = 6` — un signal de fin de
course, vraisemblablement le clignotement de confirmation.

Ce handler **confirme trois lectures faites ailleurs**, par des chemins indépendants :

1. le maximum vient bien de `tbl_max_brightness[effet]`, la table `0xA8D5` ;
2. la persistance suit exactement l'aiguillage de l'offset 5 :

```c
if (g_settings_per_effect == 0) {
    if (g_effect_family_sel == 0)  fx_params[effect].b0 = (b0 & 0x80) | brightness;
    else                           [0x0362 + effect]    = brightness;
} else                             g_global_brightness  = brightness;
```

3. et le masque `0x80` de cette écriture **préserve le bit 7 de `b0`** — le drapeau de sens dont
   j'avais déduit l'existence à la lecture (`rlc a ; mov 0x24.3, c`). L'écriture le confirme :
   c'est bien un champ séparé, conservé au fil des changements de luminosité.

> Une nuance : la lecture masque `b0 & 0x1F`, l'écriture n'en préserve que le bit 7 et laisse donc
> sept bits utiles. La luminosité étant bornée à 9, la différence ne se voit jamais.

### La trame `0x03` — l'annonce de connexion

C'est la plus courte des trois branches du parseur, et **la seule qui ne vérifie aucune somme de
contrôle** — les trames `0x02` et `0x08` la vérifient toutes deux.

```asm
0x0778  a = [0x0120]              ; octet 0 de la trame
0x077D  cjne a, #0x03, autre
0x0780  setb 0x2C.3
0x0782  clr a ; movx [0x0150], a  ; compteur de repos remis a zero
0x0787  r5 = 0 ; r7 = 0xF0
0x078A  lcall euart0_reply        ; -> trame 01 F0 00 00 00 <ck>
0x078D  IDATA[0x18] = 0x06        ; banque 3, R0
0x0790  IDATA[0x17] = 0xFF        ; banque 2, R7
0x0793  ljmp sortie
```

Les deux écritures de registre ne sont pas décoratives : chacune a **un consommateur unique et
identifié**, et c'est ce qui donne le sens de la trame.

| Écriture | Consommateur | Effet |
| --- | --- | --- |
| `[0x0150] = 0` | branche de repos de `0x3901` | remet à cent tics l'échéance de la prochaine requête d'état `0x06` |
| `BANK3_R0 = 6` | queue de `euart0_parse` : `if (++R0 > 5) { R0 = 0; 0x801F(); }` | **la toute prochaine trame déclenche la routine de batterie** |
| `BANK2_R7 = 0xFF` | `0x801F` : `if (R7 == 0xFF) { R7 = [0x0151]; poser le drapeau }` | **la jauge affichée saute d'un coup à la valeur réelle** au lieu de converger |

Autrement dit : à la réception d'une trame `0x03`, le clavier remet à zéro l'horloge de la
liaison, **court-circuite l'amortissement de la jauge** pour qu'elle affiche immédiatement le vrai
niveau, programme un envoi de batterie sans attendre, et accuse réception.

C'est le comportement d'une **annonce de (re)connexion** : on ne fait pas glisser doucement une
jauge quand le lien vient de s'établir, on l'affiche juste. L'absence de somme de contrôle va dans
le même sens — une trame de service minimale.

*Le nom « annonce de connexion » est inféré de ces trois effets ; les effets, eux, sont lus.*

#### Les deux accusés de réception

`euart0_reply(r7, r5)` compose `01 <r7> <r5> 00 00 <somme>`. Les deux seuls appels du parseur :

| Site | Trame émise | Déclencheur |
| --- | --- | --- |
| `0x078A` | `01 F0 00 00 00 <ck>` | trame `0x03` reçue |
| `0x0811` | `01 F1 00 00 00 <ck>` | trame `0x08` dont `octet[2] & 0x7F != 8` |

`0xF0` et `0xF1` sont donc deux **codes d'accusé** dans une plage distincte des commandes
ordinaires (`0x01`–`0x0E`).

#### Un drapeau qui n'est jamais consulté

`0x2C.3` est posé ici, et effacé en un seul endroit — `fcn.0000929E` (`0x935E`), une routine de
réinitialisation qui remplit une zone de `0xFF`, écrit `[0x0C44] = 2` et efface aussi `0x26.7`.

**Aucune instruction de bit ne le teste, et aucune lecture d'octet non plus.**

> **Correction.** J'avais annoncé trois `orl a, 0x2c` en `0x7AD0`, `0x7AEE` et `0x7BF1`, et
> conclu à un possible test agrégé. C'étaient **trois faux positifs**. Validés au désassemblage :
>
> ```
> 0x7ACF  MOV A, #0x45      ; octets 74 45
> 0x7AD1  ADD A, R4         ; octet  2C
> ```
>
> Les octets `45 2C` sont l'opérande de `mov a,#0x45` suivie de l'opcode de `add a, r4`. L'idiome
> `mov a,#0x45 ; add a,r4` construit un pointeur vers `0x0C45 + r4`, et il apparaît trois fois.
> Après validation instruction par instruction : **zéro** accès à l'octet `0x2C` dans toute
> l'image.

`0x2C.3` est donc **un drapeau mort** : posé à la réception d'une trame `0x03`, effacé par la
routine de réinitialisation `fcn.0000929E`, et jamais consulté — ni par bit, ni par octet.

### `fcn.00007AAD` — la fonction qui contenait ces octets

Appelée depuis `main` (`0x919A`), gardée par `0x28.1` et `0x29.0`. Elle balaie **trois tableaux
parallèles** indexés par le même compteur :

| Base | Rôle |
| --- | --- |
| `0x0C37 + i` | état / drapeau par entrée |
| `0x0C45 + i` | valeur courante |
| `0x0390 + i` | valeur souhaitée |

La boucle est bornée à **treize entrées** — la queue de la fonction le dit sans ambiguïté :

```asm
0x7C08  inc r4
0x7C09  mov a, r4
0x7C0A  xrl a, #0x0d      ; 13 entrees
0x7C0C  jz  0x7C11
0x7C0E  ljmp 0x7ABD       ; tour suivant
0x7C11  ret
```

Le motif « si l'état est posé et la valeur courante est nulle, recopier la valeur souhaitée » en
fait une **file de transitions** de treize créneaux, mais elle porte une table de sauts que le
décompilateur n'arrive pas à normaliser (`Could not find normalized switch variable`), plus un
chevauchement d'instructions signalé en `0x7E23`. Je ne la documente pas plus loin : le C n'est pas
fiable ici, et il faudrait la reprendre au désassemblage.

#### Frontières de fonction : les deux outils divergent, et r2 a raison

Ghidra donne à `0x7AAD` un corps de `0x7AAD`–`0x7D73`, soit 711 octets. r2 découpe la même plage en
deux : `fcn.00007AAD` (245 o utiles) et `fcn.00007C12` (354 o). **Le désassemblage tranche en
faveur de r2** — il y a un `ret` en `0x7C11`, et `0x7C12` est la cible d'un `lcall`. Ce sont deux
routines, pas une.

Pourquoi Ghidra les fusionne : il attribue à `0x7C12` un `COMPUTED_JUMP` venant du `jmp @a+dptr`
de `0x7B24` — une destination que la table réelle, treize entrées en `0x7B25`, ne contient pas. Sa
reconstruction de table déborde. Le seul appelant véritable de `0x7C12` est le site `0x1BA5`, que
r2 place hors fonction (`fcn.0000131c`, la plus proche en amont, fait **deux octets**) et dont
Ghidra n'a pas désassemblé les octets. Sans appelant visible d'un côté, avec un appelant imaginaire
de l'autre, les deux outils se trompent en sens contraire.

> Ce site est maintenant identifié : c'est le gestionnaire d'indice 2 de l'ordonnanceur
> d'animation RGB. Voir *L'ordonnanceur d'animation — `0x1AF0` et la table `0x1B13`*.

```asm
0x1B9B  mov dptr, #0x0300   ; compteur 16 bits
0x1B9E  movx a, @dptr
0x1B9F  subb a, r6
0x1BA0  jnc  0x1BA5
0x1BA2  ljmp 0x1DC2         ; seuil non atteint
0x1BA5  lcall fcn.00007C12  ; seul appelant
```

> **Piège de lecture.** `axt @ 0x7c12` annonce « CALL XREF from `fcn.0000131c` @ +0x889 ». C'est un
> **nommage par drapeau le plus proche**, pas une appartenance : `fcn.0000131c` mesure deux octets
> et ne contient évidemment pas un site situé 2 185 octets plus loin. Conclure « `fcn.0000131c`
> appelle `0x7C12` » serait faux. Vérifier la containment (`aflj` et comparer `offset`/`size`)
> avant d'attribuer un appel à une fonction nommée.

Aucun des deux outils n'est autorité sur les bornes. Ici c'est r2 ; ailleurs ce peut être l'inverse.


### `fcn.00007928` — le décodeur de l'encodeur rotatif

Appelée depuis `isr_pwm4` (`0x8284`), donc échantillonnée à chaque tic.

```c
DAT_EXTMEM_0F6D = (P0 >> 5) & 3;          /* deux bits : P0.5 et P0.6 */
if (0x0F6D == 0x0F6C) return;              /* pas de changement */
hist = (nouveau & 3) | (hist << 2) & 0x3C; /* trois etats de deux bits */
if (hist == 0x0B || hist == 0x34)  ... un sens ...
if (hist == 0x38 || hist == 0x07)  ... l'autre ...
0x0F6C = 0x0F6D;                           /* memorise */
```

C'est un **décodeur en quadrature** classique : deux bits, un historique glissant de six bits, et
quatre motifs de transition reconnus — `0x0B` / `0x34` dans un sens, `0x07` / `0x38` dans l'autre.

> **Ajout au brochage.** L'encodeur est sur **`P0.5`** et **`P0.6`**. Ces deux broches ne
> figuraient nulle part dans la carte établie jusqu'ici, qui ne mentionnait de `P0` que le `P0.2`
> du handshake EUART0. Aucun conflit : les lignes, colonnes, PWM et l'UART sont ailleurs.

#### Deux comportements, choisis par l'offset 26 du bloc de réglages

```asm
mov dptr, #0x0327 ; movx a, @dptr ; jnz  <voie luminosite>
```

**`XRAM 0x0327` ≠ 0 → luminosité RGB.** Incrémente ou décrémente `g_rgb_brightness`, bornée à 9,
puis persiste par exactement le même aiguillage que la classe d'action 3 : tableau par effet
(`0x0345 + effet × 2`, masque `0x80` préservé), `0x0362 + effet`, ou la copie globale.

**`XRAM 0x0327` = 0 → volume.** C'est la valeur du dump, donc le comportement d'usine :

```asm
jnb 0x25.6, fin                 ; pas d'evenement en attente
clr 0x25.6
mov dptr, #0x09BD
jnb 0x25.7, +3 ; a = 0xE9       ; sens 1
               a = 0xEA         ; sens 2
movx [0x09BD] = a ; [0x09BE] = 0
setb 0x2A.0
```

`0xE9` et `0xEA` sont les usages **Consumer volume + et volume −**. Et `0x2A.0` est précisément le
drapeau du premier maillon de la chaîne de rapports HID, dont le tampon est **`0x09BC`** et la
longueur **3** dans la table `0x6045`.

Tout se recoupe : `0x09BC` porte le Report ID 2, `0x09BD:0x09BE` l'usage Consumer sur seize bits,
soit trois octets — la longueur annoncée. La lecture de la table des longueurs faite bien plus tôt
se confirme ici par un chemin complètement différent.

> **Donc, sur cet exemplaire, la molette règle le volume**, et un réglage persistant (offset 26 du
> bloc) la bascule sur la luminosité du rétroéclairage.

`IDATA 0x6F` sert de compteur de répétition, comparé à 10 — la gestion de l'auto-répétition quand
on tourne vite.

> *Correction, refermée :* j'avais situé les trois `orl a, 0x2c` dans `fcn.00007928`, puis dans
> « la ou les fonctions suivantes ». En réalité **ces trois instructions n'existent pas** : ce sont
> des octets d'opérande lus à tort comme des opcodes. Voir *Un drapeau qui n'est jamais consulté*
> et `fcn.00007AAD`, ci-dessus.

### Ce qui reste ouvert sur EUART0

Le sous-système est couvert de bout en bout : registres, débit, ISR, les deux handshakes, les
onze commandes émises, les trois types de trame reçue, le protocole interne de la commande `0x08`,
son séquenceur et la carte de relecture de la flash.

> **Correction d'une note ancienne.** Mes premières notes annonçaient quatre types de trame
> dispatchés : `0x02`, `0x03`, `0x08` et `0x15`. Le `0x15` n'existe pas : c'est
> `xrl a,#0x15 ; orl a, r6`, la borne d'une boucle 16 bits de **21 octets** — la portée de la
> somme de contrôle. L'octet 0 de la trame n'est comparé qu'en deux endroits, `0x060E` (`0x02`) et
> `0x0778` (`0x03`), le reste tombant dans la branche `0x08`.

Restent, par ordre décroissant d'intérêt :

| Point | Ce qu'on sait |
| --- | --- |
| Passage en mode 2,4 GHz | aucune écriture directe de la valeur 1 dans `g_transport` |
| Paramètres `0xF0` / `0xF1` de `euart0_reply` | deux appels depuis le parseur, valeurs non interprétées |
| Opcode `0x4A` | un bloc de 2 octets, charge **remplie de zéros** par `0x3CF9` — probablement une fin de transfert |
| Handlers 8, 9, 10 du séquenceur | `0x3D25` et `0x3D1D`, n'émettent aucun opcode |

### Ce que le décompilateur ajoute sur `euart0_parse`

Le firmware a été rechargé dans **Ghidra 12.1.3** (`8051:BE:16:default`), avec l'espace `EXTMEM`
mappé et les noms portés. Le C de `euart0_parse` donne trois choses que le désassemblage n'avait
pas rendues.

#### La somme de contrôle est bien vérifiée en réception

```c
BANK1_R0 = 0;
do { BANK1_R0 += frame[i]; i++; } while (i != 0x15);
BANK1_R0 = 0x55 - BANK1_R0;
if ((DAT_EXTMEM_0135 ^ BANK1_R0) == 0) { /* trame acceptee */ }
```

Vingt-et-un octets sommés, `0x55 − Σ`, comparé à l'octet 21 de la trame. **La même constante
`0x55` est donc vérifiée dans les deux sens** — ce n'était établi qu'à l'émission.

#### Un sous-index dans le quartet haut de l'octet 5

```c
bVar10 = frame[5];                    /* XDATA 0x0125 */
DAT_EXTMEM_038f = bVar10 & 0xf;       /* longueur */
DAT_EXTMEM_038d = bVar10 >> 4;        /* sous-index */
```

`0x038D` est exactement l'adresse que la chaîne de sélection d'effet utilise comme sous-index —
la lecture « quartet haut d'un octet de config » est confirmée, et sa **source est une trame
radio**.

#### Deux sous-commandes de la trame `0x08`

| Sous-index | Effet |
| --- | --- |
| `2` | remplit **toute** une table de couleurs avec un seul triplet RGB |
| `1` | transfert **fragmenté** vers le bloc de réglages de travail |

Sous-index 2 — la boucle écrit les **mêmes** trois octets (`frame[6..8]`) partout :

```c
for (n = 0; n != 0x15; n++)
  for (m = 0; m != 6; m++)
    *(0x0152 + n*0x12 + m*3 + k) = frame[6+k];
```

Soit une table en **XDATA `0x0152`–`0x02CB`**, 378 octets : 21 rangées de 18 octets, six triplets
par rangée. Elle s'arrête juste avant `0x02CC`, que `bt_set_name` utilise — la borne est cohérente.

> *À noter sans trancher :* 21 × 6 = **126 entrées**, alors que le F75 n'a que 90 positions de
> matrice (6 × 15). La table est donc soit surdimensionnée pour la famille de claviers qui partage
> ce firmware, soit indexée autrement que par (ligne, colonne).

Sous-index 1 — l'écriture va vers `0x09BF + offset`, c'est-à-dire le **bloc de travail des
réglages** déjà identifié, avec `frame[4]` en numéro de séquence (contrôle
`frame[4] == précédent + 1`, mémorisé en `0x09AA`), `frame[5] & 0x0F` en longueur, et une borne
sur `0x76` = 118. C'est le chemin par lequel l'hôte — ou l'application mobile via le Bluetooth —
réécrit les réglages persistés.

#### Types de trame confirmés

| Octet 0 | Traitement |
| --- | --- |
| `0x02` | branche principale |
| `0x03` | pose `0x2C.3`, efface `0x0150`, répond par `euart0_reply` |
| `0x08` | vérification de somme puis les sous-commandes ci-dessus |

### La table des commandes — établie au décompilateur

Les douze émetteurs gardés par `P4.7` ont été décompilés. L'octet de commande est l'octet 1 de la
trame, posé par l'appelant en IDATA `0x34` ; `euart0_send(longueur, étiquette)` fait le reste.

| Cmd | Émetteur | Long. | Charge utile | Déclencheur |
| --- | --- | --- | --- | --- |
| `0x01` | `0xED89` | 6 | slot, 0 | radio non connectée (`euart0_parse`, `0xEF5A`) |
| `0x02` | `hid_report_radio` | **30** | 28 o tirés de la file | rapport HID de type 2 |
| `0x03` | `hid_report_radio` | **13** | 11 o tirés de la file | rapport HID de type 3 |
| `0x04` | `0xB06A` | 6 | un paramètre | `0x2C.4` posé |
| `0x06` | `0xED3B` | 6 | `00 00 00` | **requête d'état** → réponse `02 06 …` |
| `0x08` | `0xB093` | 23 | conteneur, sous-code en `[3A]` | 9 appelants |
| `0x09` | `0xA307` | 32 | nom Bluetooth | `main` |
| `0x0B` | `0xB0BC` | 6 | un paramètre | `0xEED1` |
| `0x0C` | `0xAE50` | 6 | deux paramètres | — |
| `0x0D` | `0xB108` | 6 | pourcentage de batterie | `0x801F`, une trame d'état sur six |
| `0x0E` | `0xEC85` | 6 | — | mode filaire |

Tous suivent le même prologue, qui explique les deux gardes déjà relevées :

```c
if (_c_1 != 1 && RD != 0) {        /* pas d'emission en cours, module pret */
    memset(IDATA 0x33, 0, 0x20);
    euart0_tx_buf = 1;             /* octet 0 : en-tete constant */
    DAT_INTMEM_34 = <commande>;
    euart0_send(...);
}
```

`RD` est le nom 8051 générique que Ghidra donne au bit `0xB7` : c'est **`P4.7`**, la ligne
« module prêt ». Ghidra ne renomme pas les bits de l'espace `BITS` avec la carte SFR.

#### Le conteneur `0x08`

`euart0_cmd08` transporte un sous-message. Depuis `hid_report_radio`, quand la file est vide :

```c
IDATA[0x35] = 0x13; [0x36] = 10; [0x37] = 1; [0x38] = 0; [0x39] = 4;
IDATA[0x3A] = <sous-code>;          /* 5 = batterie, 7 = reglages */
IDATA[0x3B] = <valeur>;             /* 0x0C36 (pourcentage) ou g_settings */
IDATA[0x3C] = bit0 si pas plein, bit4 si en charge;
euart0_cmd08();
```

Le site `0x393E`, lui, pose `[35] = 0x13, [36] = 5, [37] = 1, [39] = 0x0A` puis copie dix octets
depuis `CODE:0xB3DC` — même enveloppe, sous-message différent.

#### La structure interne de la commande `0x08`

> **Correction.** J'ai d'abord présenté `IDATA[0x3A]` comme « le sous-code ». C'est faux : `0x3A`
> est le **premier octet de charge**. Le discriminant est `IDATA[0x36]`, et `IDATA[0x35]` n'est pas
> un marqueur mais une **longueur** — `0x13` = 19, exactement le nombre d'octets que
> `euart0_cmd08` additionne (`IDATA 0x35..0x47`).

```
[0x34] = 0x08            commande
[0x35] = 0x13 = 19       longueur de la charge
[0x36] = opcode          <-- le discriminant
[0x37] = echo de 0x02E2  issu de l'octet 3 de la requete recue
[0x38] = echo de 0x0D14  issu de l'octet 4 de la requete recue
[0x39] = (n << 4) | 0x038F   quartet bas : la longueur demandee
[0x3A .. 0x47]           charge, jusqu'a 14 octets
[0x48] somme partielle   [0x49] somme de trame
```

Les octets 3 et 4 de la requête sont **renvoyés tels quels** : c'est un protocole
requête/réponse apparié, pas un flux.

| Opcode `[0x36]` | Charge | Émis par |
| --- | --- | --- |
| `0x05` | gabarit `CODE:0xB3DC`, 10 o | handler 1 |
| `0x0A` | premier octet = 5 (batterie) ou 7 (réglages) | `hid_report_radio` |
| `0x41` | 14 o depuis `CODE:0xD800` | handler 2 |
| `0x42` `0x43` `0x44` | 14 o depuis une base de la zone IAP | handlers 3, 4, 5 |
| `0x49` `0x4A` | idem | handlers 6, 7 |

L'opcode `0x0A` porte donc bien un second niveau dans son premier octet de charge — c'est là que
vivent le 5 (batterie) et le 7 (réglages au démarrage). Les autres opcodes n'en ont pas.

#### Le séquenceur `0x3901`

Les sept handlers sont les entrées d'une table de sauts, choisie par `XRAM 0x0D9E` :

```c
if ((0x0D9E - 1) > 9) { /* repos */ }
else jump  CODE[0x391C + (0x0D9E - 1) * 3];
```

Dix entrées de trois octets en `0x391C` :

| `0x0D9E` | Handler | Opcode émis |
| --- | --- | --- |
| 1 | `0x393A` | `0x05` |
| 2 | `0x3975` | `0x41` |
| 3 | `0x3A25` | `0x42` |
| 4 | `0x3AB5` | `0x43` |
| 5 | `0x3BA7` | `0x44` |
| 6 | `0x3C42` | `0x49` |
| 7 | `0x3CD0` | `0x4A` |
| 8, 9 | `0x3D25` | — |
| 10 | `0x3D1D` | — |

**La branche de repos (`0x0D9E == 0`) est celle qui entretient la liaison** : elle incrémente un
compteur (`0x0150`) et, tous les cent tics, émet `euart0_cmd06` — la requête d'état dont j'ai
décodé la réponse plus haut. Après trois passages, elle relâche le handshake :

```c
if (++counter > 99) {
    counter = 0;
    euart0_cmd06();
    if (++DAT_INTMEM_30 > 2) { DAT_INTMEM_30 = 0; _c_1 = 0; P0CR &= 0xFB; P0_2 = 1; }
}
```

Les handlers se **chaînent** en réécrivant `0x0D9E` (`0x3CCE`, `0x3CD4`) et le remettent à zéro en
fin de service (`0x393E`) : une requête déclenche une rafale de réponses, une par tic.

#### Les huit octets que `0x41` ne lit pas — et `5A A5` enfin tranché

Le handler 2 redispatche lui aussi sur `0x038D`, mais avec **quatre** bases, espacées de
**1024** octets :

| `0x038D` | Base | Page |
| --- | --- | --- |
| 0 | `0xCC00` | 102 |
| 1 | `0xD000` | 104 |
| 2 | `0xD400` | 106 |
| 3 | `0xD800` | 108 |

Et il en lit **504 = 512 − 8** octets. Les huit octets sautés sont la queue de la page. Les voici,
pour la base `0xD800` :

```
offset 504..511 :  00 00 00 00 00 00 5a a5
```

**Les seuls octets non nuls de toute la page sont ce `5A A5`** — et ils tombent précisément dans la
zone que le firmware s'abstient de relire.

##### Le marqueur, balayé sur toute la zone IAP

| Page | Marqueur `5A A5` | Octets non nuls |
| --- | --- | --- |
| 99 | offset **126** (fin du bloc de 128 o) | 87 |
| 100 | offset 506 | 282 |
| 101 | — | 18 |
| **102 – 109** | offset **510** (deux derniers octets) | 86, 100, 75, 57, 2, 2, 2, 2 |
| 110 – 117 | — | **0** |

Trois constats indépendants convergent :

1. le motif est en **fin de page** (offset 510) sur huit pages consécutives ;
2. l'opcode `0x41` s'arrête à **512 − 8**, donc **juste avant** lui ;
3. les pages qui le portent ont du contenu ; celles qui ne l'ont pas sont **entièrement vierges**.

> **Tranché.** `5A A5` est un **tampon de validité de page**, et le chemin de relecture le sait :
> il renvoie les données et laisse la queue. Je l'avais signalé deux fois comme « lecture la mieux
> étayée, non prouvée » — le `504 = 512 − 8` est la preuve structurelle qui manquait, parce qu'elle
> vient du code et non du motif binaire.

Les deux octets sont complémentaires (`0101 1010` / `1010 0101`), ce qui reste la forme classique
d'un tampon : un effacement flash laisse `FF FF` ou `00 00`, jamais cette paire.

##### Ce que cela change sur les profils

Les pages **102 à 109 sont marquées valides**, les pages **110 à 117 ne le sont pas** et sont
vierges. Or l'opcode `0x43` lit exactement `0xDC00`–`0xEA00`, c'est-à-dire les **pages 110 à 117** —
les non marquées — tandis que `0x41` lit quatre des huit marquées.

La grille `0xD800 + n × 512` établie depuis l'offset 0 du bloc de réglages reste exacte comme
**adressage**. Mais la frontière réelle des données est ailleurs : elle passe entre la page 109 et
la page 110, pas à `0xD800`.

##### Pourquoi `0x41` semble sauter une page sur deux — résolu

Il n'en saute aucune : les pages « manquantes » sont couvertes par **d'autres opcodes**. Les bases
des trois derniers handlers complètent la carte.

| Opcode | Base | Page(s) | Blocs × 14 | Contenu |
| --- | --- | --- | --- | --- |
| `0x44` | `0xC600` | 99 | 10 → **140** | **bloc de réglages** |
| `0x49` | `0xC800` | 100 | 35 → **490** | palettes |
| `0x42` | `0xCA00` | 101 | 27 → **378** | **table de couleurs** |
| `0x41` | `0xCC00` +1024 | 102, 104, 106, 108 | 36 → **504** | — |
| `0x43` | `0xDC00` +512 | 110 – 117 | 37 → **512** | — |
| `0x4A` | *aucune* | — | 1 × 2 | charge mise à zéro |

Deux comptes se vérifient d'eux-mêmes :

- `0x44` annonce **10 = ⌈128 / 14⌉**, et le bloc de réglages fait exactement **128 octets**. C'est
  la **troisième** dérivation indépendante de cette taille, après les boucles de copie de
  `settings_restore` / `settings_save` et l'argument de longueur de `flash_read_block`.
- `0x42` annonce 378, la taille exacte de la table de couleurs remplie par le chemin d'écriture.

##### Ce qui reste vraiment, et c'est plus intéressant

La carte couvre les pages 99 à 117 **sauf 103, 105, 107 et 109**. Or ce ne sont pas des pages
vides : elles portent le tampon `5A A5` et, pour la 103, **cent octets non nuls — c'est la table
de remap `0xCE00`**.

L'explication tient au pas de `0x41` : ses régions font **1024 octets**, donc chacune couvre
*deux* pages — 102-103, 104-105, 106-107, 108-109. Les pages « manquantes » sont la **seconde
moitié** de ses régions. Il n'annonce que 504 octets, ce qui s'arrête dans la première page.

Et l'adressage n'est pas borné :

```asm
a = [0x0D14] ; B = 14 ; mul ab      ; offset = sequence x 14
pointeur = base + offset            ; aucun controle de borne
```

Le compte est **consultatif** : il est renvoyé à l'hôte en `IDATA[0x37]`, et rien dans le firmware
n'empêche une requête de séquence plus élevée. `0xCC00 + 36 × 14 = 0xCDF8` — la séquence 36
chevauche déjà la page 103, et les séquences 37 à 73 la couvriraient entièrement.

> **Donc la table de remap est lisible par l'opcode `0x41`, sous-index 0, aux séquences ≥ 36** —
> au-delà du compte annoncé. C'est vérifiable : il suffirait de demander ces séquences. Non tenté,
> rien n'ayant jamais été envoyé à l'appareil.

#### L'opcode `0x43` — un second niveau de dispatch, et ce que valent les 518 octets

Le handler 4 n'émet pas directement : il **redispatche** sur `XRAM 0x038D`, le sous-index que
`euart0_parse` a extrait du quartet haut de l'octet 5 de la requête.

```asm
0x3AB5  movx a, [0x038D] ; mov r7, a
0x3ABA  cjne a, #0x08, +3 ; jnc sortie      ; rejette >= 8
0x3ABF  mov dptr, #0x3AC6 ; a = a*3 ; jmp @a+dptr
```

Huit entrées en `0x3AC6`, chacune posant le poids fort du pointeur de charge :

| `0x038D` | Base | Page |
| --- | --- | --- |
| 0 | `0xDC00` | 110 |
| 1 | `0xDE00` | 111 |
| 2 | `0xE000` | 112 |
| 3 | `0xE200` | 113 |
| 4 | `0xE400` | 114 |
| 5 | `0xE600` | 115 |
| 6 | `0xE800` | 116 |
| 7 | `0xEA00` | 117 |

**Ce sont exactement les pages de profil 2 à 9.** Leur disposition avait été établie ailleurs, à
partir du `0xD800 + n × 512` de l'offset 0 du bloc de réglages — deux fonctions sans rapport qui
donnent la même grille. Le pas de 512 octets et la borne haute `0xEA00` (= profil 9) coïncident.

L'opcode `0x43` est donc **la relecture d'un profil de remap**, le sous-index choisissant lequel.

##### Les 518 octets n'existent pas

L'accès est **aléatoire, pas séquentiel** : le pointeur est recalculé à chaque requête depuis la
base, et non avancé.

```asm
a = [0x0D14] ; B = 14 ; mul ab        ; offset = sequence x 14
[0x013A] += offset_lo ; [0x0139] += offset_hi     ; pointeur = base + sequence x 14
```

Et la réponse renvoie à l'hôte de quoi piloter la boucle :

```asm
IDATA[0x37] = [0x02E2]    ; nombre total de blocs
IDATA[0x38] = [0x0D14]    ; numero du bloc courant
```

Le compte `[0x02E2]` est donc un **plafond de boucle**, pas une taille. Et il se lit :

| Opcode | Blocs | × 14 | Division | Taille réelle de la région |
| --- | --- | --- | --- | --- |
| `0x41` | 36 | 504 | exacte | **504 o** |
| `0x42` | 27 | 378 | exacte | **378 o** |
| `0x43` | 37 | 518 | **reste 6** | **512 o** — `⌈512 / 14⌉ = 37` |
| `0x44` | 10 | 140 | exacte | **140 o** |
| `0x49` | 35 | 490 | exacte | **490 o** |

**`0x43` est le seul dont le compte ne divise pas.** C'est précisément celui qui lit une page
entière de 512 octets : 37 blocs de 14 la couvrent, le dernier dépassant de six octets que l'hôte
ignore. Les quatre autres tombent juste, donc leurs régions font exactement 504, 378, 490 et
140 octets — et non des pages.

Ce qui corrige la lecture du commit précédent : je parlais de « 518 octets » comme d'une taille de
région. C'est un **nombre de blocs arrondi au-dessus**, et la région fait 512.

Au passage, cela renseigne aussi l'opcode `0x41` : sa région fait **504 octets pile**, pas une page.
Huit octets de moins qu'une page de 512 — réservés à autre chose, ou simplement hors du champ
relu.

#### Ce que la charge contient

Le pointeur de charge vit dans `XRAM 0x0139:0x013A` et reçoit `0xD000`, `0xD400` ou `0xD800` selon
le handler — **des pages de la zone IAP**. Le firmware renvoie donc, quatorze octets à la fois, le
contenu de sa flash de configuration.

C'est le **pendant en lecture** du transfert fragmenté déjà identifié en réception (trame `0x08`,
sous-index 1, vers `g_settings_staging`). La liaison radio porte un protocole de lecture *et*
d'écriture sur la zone de configuration, par blocs de 14 octets, avec numéro de séquence.

#### Les treize arms du conteneur `0x08` reçu

`euart0_parse` aiguille les trames `0x08` reçues par un appel au répartiteur inline `0x4E45`, en
`0x0856`. Le répartiteur se lit d'un bloc :

```asm
0x4E45  pop DPH ; pop DPL          ; DPTR = adresse de retour = debut des donnees inline
0x4E49  mov R0,A                   ; R0 = la cle cherchee
0x4E4B  movc a,@a+dptr             ; hi
0x4E4C  jnz 0x4E60                 ; entree valide -> comparer
0x4E50  movc a,@a+dptr (offset 1)  ; lo
0x4E51  jnz 0x4E60
0x4E53  inc dptr ; inc dptr        ; hi == lo == 0 : fin de table, l'adresse par defaut suit
0x4E55  <charger hi/lo dans DPTR et jmp @a+dptr>
0x4E60  movc a,@a+dptr (offset 2)  ; la CLE
0x4E63  xrl a,R0 ; jz 0x4E55       ; trouvee -> sauter a l'adresse de CETTE entree
0x4E66  inc dptr x3 ; sjmp 0x4E4A  ; entree suivante
```

**Les entrées sont `(adresse_hi, adresse_lo, clé)` — la clé en dernier**, la table se termine par une
adresse `00 00`, et l'adresse par défaut suit le terminateur. Ce format vaut pour les **quatre**
sites d'appel de `0x4E45` de l'image : `0x0856` (13 entrées, défaut `0x0F6A`), `0x1741` (19 entrées,
défaut `0x1AEA` — les effets), `0x4136` (17 entrées, défaut `0x44F5` — les raccourcis) et `0x8845`
(9 entrées, défaut `0x8919`).

Deux recoupements indépendants ferment la lecture. L'arm `0x0C4A` se termine par `ljmp 0x0F6A`,
c'est-à-dire **l'adresse par défaut de sa propre table** ; et les sept opcodes que cette page avait
déjà déduits par une recherche d'octets sur `90 0d 9e` (`0x05` en `0x0C4A`, puis `0x41`…`0x4A`)
tombent exactement sur les clés que ce parse donne.

##### La table

| Clé | Arm | Sens | Rôle |
| --- | --- | --- | --- |
| `0x01` | `0x0884` | écriture | profil de **remap** — base `CODE` selon le sous-index `[0x038D]` : `0`→`0xCC00`, `1`→`0xD000`, `2`→`0xD400`, `3`→`0xD800` |
| `0x02` | `0x09BD` | écriture | profil d'**éclairage par touche** — base `0xCA00`, sous-index `0` seulement ; **pas d'écriture flash** |
| `0x03` | `0x0AAC` | écriture | huit profils — bases `0xDC00`, `0xDE00`, `0xE000`, `0xE200`, `0xE400`, `0xE600`, `0xE800`, `0xEA00` |
| `0x04` | `0x0BB0` | écriture | bloc de **réglages** — page fixe, validation par `0xEEF4` |
| `0x05` | `0x0C4A` | lecture | arme le handler **1** dans `[0x0D9E]` (gabarit `0xB3DC`) |
| **`0x06`** | `0x0C53` | action | **réinitialisation d'usine immédiate** |
| `0x09` | `0x0C68` | écriture | base fixe `0xC800` |
| `0x41` | `0x0D10` | lecture | handler 2 — 36 blocs × 14 = **504 o** |
| `0x42` | `0x0D2B` | lecture | handler 3 — 27 × 14 = **378 o** (la table de couleurs) |
| `0x43` | `0x0D45` | lecture | handler 4 — 37 × 14 = **518 o** |
| `0x44` | `0x0D5F` | lecture | handler 5 — 10 × 14 = **140 o** |
| `0x49` | `0x0D79` | lecture | handler 6 — 35 × 14 = **490 o** |
| `0x4A` | `0x0D93` | lecture | handler 7 — 1 bloc de **2 o** ; force aussi `[0x08DB] = 10` |
| — | `0x0F6A` | — | **défaut** : sortie du parser, sans effet |

##### Le squelette commun des cinq arms d'écriture

`0x0884`, `0x09BD`, `0x0AAC`, `0x0BB0` et `0x0C68` sont le même corps, à la base et à la validation
près :

```c
if (sub_index_selectionne)                  /* [0x038D] choisit une base CODE */
    { [0x0139:0x013A] = base; }

if ([0x0D14] == 0)                          /* premier bloc de la sequence */
    memcpy_code(0x09BF, base, 512);         /* amorcer le tampon avec la page actuelle */

[0x0139:0x013A] = [0x0D14] * 14;            /* decalage = numero de sequence x 14 */
memcpy(0x09BF + decalage, 0x0126, [0x038E:0x038F]);   /* le bloc recu */

if ([0x0D14] == [0x02E2])                   /* dernier bloc -> valider */
    <validation propre a l'arm>;

for (i = 2; i < 0x16; i++)                  /* accuse : la requete renvoyee telle quelle */
    IDATA[0x33 + i] = XRAM[0x011F + i];
euart0_cmd08(...);
if (!filaire) { P0CR &= 0xFB; P0.2 = 1; }   /* relacher le handshake d'emission */
```

L'amorçage est ce qui rend le protocole sûr : le tampon de 512 octets en `XRAM 0x09BF` est d'abord
rempli avec **le contenu actuel de la page**, et seuls les octets effectivement transmis l'écrasent.
Un transfert partiel ne détruit donc pas le reste de la page. Les arms `0x03` et `0x04` n'amorcent
pas — ils réécrivent la page entière.

La validation diffère :

| Arm | Validation |
| --- | --- |
| `0x01`, `0x03`, `0x09` | `0xEE4E` → `iap_save(page = base_hi >> 1)` → `0xAAC1`, **le même graveur de page IAP que la réinitialisation d'usine** |
| `0x02` | **aucune écriture flash** : pose le bit `0x2C.7`, écrit `[0x0E24] = 0x20 + [0x038D]` puis appelle `0xAF99` — soit *bascule sur l'effet `0x20 + sous-index`*. Les clés `0x20`, `0x26` et `0x2D` de la table d'effets `0x1744` sont exactement ces effets-là |
| `0x04` | `0xEEF4`, qui déroule l'IAP à la main : `EA = 0`, `matrix_cols_release(99)`, `iap_erase(99)`, `iap_write_page(src = 0x09BF, dst = 0xC600, 512)`, `iap_lock()`, `EA` restauré |

`page = base_hi >> 1` place toutes ces destinations dans la plage **99–117** que cette page a
identifiée comme l'EEPROM émulée : `0xC600` → 99, `0xC800` → 100, `0xCA00` → 101, `0xCC00` → 102, et
les huit profils de l'arm `0x03` sur **110 à 117**, la fin exacte de la plage. Le protocole d'écriture
et la cartographie flash, établis séparément, se rejoignent sur la même borne.

##### Ce que ça change pour le négatif d'appairage

Les treize arms sont maintenant ouverts un par un. **Aucun ne touche l'EUART0 autrement que par
l'accusé `euart0_cmd08`, qui renvoie la requête reçue verbatim.** Aucun n'émet la commande `0x01`, ni
rien d'autre qui puisse détruire un lien enregistré. Le négatif tient dans les deux sens.

Une nuance à l'affirmation antérieure « les sous-opcodes du conteneur `0x08` sont tous des transferts
de données » : **douze sur treize** le sont (cinq écritures, sept lectures). Le treizième, `0x06`, est
une **action** — la réinitialisation d'usine. Et l'arm `0x01` ne transfère pas seulement : il choisit
aussi une base par le sous-index.

#### Qui arme le séquenceur — résolu

Les trois écritures que Ghidra listait étaient internes au séquenceur. Une recherche du motif
`90 0d 9e` sur l'image entière en trouve **treize**, dont sept dans une plage que la liste de
références ne couvrait pas : `0x0C4A`, `0x0D23`, `0x0D3E`, `0x0D58`, `0x0D72`, `0x0D8C`, `0x0DA6`.

Ces adresses tombent **dans le corps de `euart0_parse`** — elles se terminent toutes par
`ljmp 0x0F6A`, l'étiquette de sortie du parser. La fonction que Ghidra avait reconstruite ne
couvrait simplement pas cette plage, donc aucun xref n'avait été enregistré.

> **Ornière de méthode.** Une liste de références n'est complète que pour le code effectivement
> désassemblé. Ici la recherche octet a rattrapé ce que le graphe de références manquait — comme
> pour les pointeurs calculés, mais pour une raison différente : la plage était bien du code, elle
> n'appartenait à aucune fonction.

Six des sept sites sont un même bloc, au comptage près :

```asm
movx [0x02E2], #<n>      ; nombre de blocs a transferer
movx [0x0D14], #0        ; numero de sequence, remis a zero
movx [0x038E], #0        ; longueur de bloc, poids fort
movx [0x038F], #14       ; longueur de bloc
movx [0x0D9E], #<h>      ; arme le handler h
ljmp 0x0F6A              ; sortie du parser
```

| Site | Handler | Opcode | Blocs | Octets/bloc | **Total** |
| --- | --- | --- | --- | --- | --- |
| `0x0C4A` | 1 | `0x05` | — | — | gabarit `0xB3DC` |
| `0x0D10` | 2 | `0x41` | 36 | 14 | **504** |
| `0x0D2B` | 3 | `0x42` | 27 | 14 | **378** |
| `0x0D45` | 4 | `0x43` | 37 | 14 | **518** |
| `0x0D5F` | 5 | `0x44` | 10 | 14 | **140** |
| `0x0D79` | 6 | `0x49` | 35 | 14 | **490** |
| `0x0D93` | 7 | `0x4A` | 1 | **2** | **2** |

**Le total de l'opcode `0x42` vaut 378 octets — exactement la taille de la table de couleurs**
(`XRAM 0x0152`–`0x02CB`) que le chemin d'écriture remplit, trame `0x08` sous-index 2. Les deux
sens du protocole se recoupent sur la même structure, établis séparément.

Les 504 octets de l'opcode `0x41` cadrent avec la page de profil de 512 octets en `0xD800`, à huit
octets près.

Et ce sont **les mêmes trois variables** — `0x02E2` (compte), `0x0D14` (séquence), `0x038F`
(longueur) — que `euart0_parse` renseigne depuis les octets 3, 4 et 5 d'une trame `0x08` reçue.
Selon le chemin, le clavier **répond** à un transfert demandé par l'hôte, ou **initie** le sien.

La boucle est donc complète :

```
trame recue --> euart0_parse pose 0x02E2 / 0x0D14 / 0x038F / 0x0D9E
                                  |
                       tic suivant v
                FUN_CODE_3901 : table de sauts sur 0x0D9E
                                  |
                                  v
                cmd 0x08, opcode, 14 octets de flash, sequence incrementee
                                  |
                        0x0D9E remis a 0 --> branche de repos
                                             (cmd 0x06 tous les 100 tics)
```

##### `CODE:0xB3DC` — un gabarit, pas une chaîne

Le site `0x393E` compose l'enveloppe puis recopie la table :

```asm
IDATA[0x35] = 0x13 ; [0x36] = 0x05 ; [0x37] = 0x01 ; [0x38] = 0 ; [0x39] = 0x0A
r2 = 7
boucle: IDATA[0x33 + r2] = CODE[0xB3DC + r2] ; r2++  jusqu'a r2 == 17
```

L'indice de départ compte : `IDATA[0x33 + 7]` vaut `IDATA[0x3A]`, **la case du sous-code**. La
table n'est donc pas une chaîne de dix caractères — c'est un **sous-message pré-composé** :

```
idx  0  1  2  3  4  5  6 | 7  | 8  9 10 11 12 13 14 15 16
     ff 00 ff ff ff ff ff| 03 | 00 00 00 00 cd 01 00 00 00
                          ^sous-code    ^
```

Sous-code **3**, charge `00 00 00 00 CD 01 00 00 00`. Les sept premiers octets ne sont jamais lus
par ce site.

> *À noter, sans le conclure :* l'octet `0xCD` en cinquième position de la charge est aussi
> l'identifiant interne que le driver OpenRGB associe à l'AULA F75
> (`{ 0xCD, { "AULA F75", aula_f75_layout } }`). Mais `0xCD` **n'apparaît nulle part ailleurs
> comme immédiat** dans l'image, et cette table n'a qu'un seul lecteur : rien dans le code ne relie
> les deux. C'est une coïncidence à vérifier, pas un fait.

##### Le sous-code `7` est un envoi unique au démarrage

`0x2B.5` n'est posé qu'à un seul endroit — `vec.reset`, en `0x917B` :

```asm
jb    0x2B.1, suite
lcall settings_restore        ; 0xA283
setb  0x2A.1
clr   0x2C.2
setb  0x2B.5                  ; <-- arme l'envoi du sous-code 7
```

Il est armé **juste après la restauration des réglages depuis la flash**, et effacé par celui des
deux transports qui l'émet le premier (`hid_report_radio` en `0x47B0`, `hid_report_usb` en
`0x6BDD`). Au démarrage, une fois ses réglages relus, le clavier les annonce **une fois** au module
radio.

### Le clavier a dix profils de remap en flash

La charge du sous-code 7 est `g_settings[0]`, c'est-à-dire l'**offset 0** du bloc de réglages —
le champ le plus référencé du bloc (20 sites) et le seul que je n'avais pas identifié. Le C le
donne :

```asm
a = [0x030D]
add  a, a                     ; x 2
DPL = 0
DPH = 0xD8 + a                ; DPTR = 0xD800 + index x 512
...
mov  B, #4                    ; puis indexe par l'evenement x 4
```

**`DPTR = 0xD800 + index × 512`**, puis un décalage de `événement × 4` : c'est exactement la forme
du descripteur de remap de quatre octets. `XRAM 0x030D` est donc un **index de profil**, et chaque
profil occupe une page de 512 octets à partir de `0xD800`.

| Profil | Adresse | Page | Octets non nuls |
| --- | --- | --- | --- |
| 0 | `0xD800` | 108 | 2 |
| 1 | `0xDA00` | 109 | 2 |
| 2–9 | `0xDC00`–`0xEBFF` | 110–117 | **0** |

**La fin du profil 9 tombe sur `0xEBFF` — exactement la borne haute de la zone IAP.** Cette borne
avait été établie indépendamment, par le `subb a, #0x76` de `iap_erase` qui rejette les pages
au-delà de 117. Les deux se rejoignent au bit près : la zone réinscriptible est dimensionnée pour
**page de réglages + palettes + keymap de base + dix profils**, sans un octet de rab.

Le chemin profil est gardé par l'**offset 22** du bloc (`XRAM 0x0323`) :

```asm
mov dptr, #0x0323 ; movx a, @dptr ; jz  ...   ; si 0 -> on saute le profil
```

Dans le dump, offset 0 et offset 22 valent tous deux `0x00`, et huit des dix pages sont vierges :
**aucun profil n'est configuré**, et c'est la table de remap de base en `0xCE00` qui s'applique.

> *Inféré :* que ces pages soient des tables de remap. *Établi :* l'adressage
> `0xD800 + n × 512 + événement × 4`, la coïncidence exacte avec la borne IAP, et le fait que le
> chemin soit désactivé sur cet exemplaire.

### La trame d'état reçue — `02 06 …`

C'est la branche `octet[0] == 2` de `euart0_parse`, la seule qui restait à lire. Elle est gardée
deux fois avant tout traitement :

```c
if (frame[2] != 0)      return;    /* octet 2 impose a 0 */
if ((frame[1] ^ 6) != 0) return;   /* octet 1 impose a 6 */
sum = 0x55 - Σ frame[0..8];
if (frame[9] != sum)     return;   /* somme sur NEUF octets */
```

Dix octets, somme de contrôle sur les neuf premiers — ce qui confirme le format déjà relevé, en y
ajoutant les deux contraintes. C'est la **réponse à la commande `0x06`** : requête et réponse
portent le même code.

| Octet | Destination | Rôle |
| --- | --- | --- |
| 3 | `0x0F41` | — |
| 4 | `0x09BA` | code d'état — `0x0A` en filaire, `0x0B` autre, comparé au slot en Bluetooth |
| 5 | `0x09AC` | **drapeau « connecté »** |
| 6–7 | `0x02E7`:`0x02E6` | **jauge, 16 bits petit-boutiste** |

> **Correction.** J'avais lu la jauge comme `octet × 256` d'après le désassemblage. Le C montre
> l'affectation finale : `0x02E6` (poids fort) reçoit `frame[7]`, `0x02E7` (poids faible) reçoit
> `frame[6]`. C'est un **vrai champ 16 bits**, ce qui colle mieux aux seuils 737 / 781 / 846 / 912
> et à une conversion sur 10 bits.

Le traitement dépend ensuite du transport :

```c
if (g_transport == 2) {                     /* Bluetooth */
    if (g_bt_slot == 0 || g_bt_slot > 3) g_bt_slot = 1;   /* borne 1..3 */
    if (frame[4] == slot && frame[5] != 0)  /* connecte */ ;
    else  cmd_01(g_bt_slot, 0);
}
else if (g_transport == 1) {                /* 2,4 GHz */
    if (frame[4] != 0 || frame[5] == 0) cmd_01(0, 0);
}
else if (g_transport == 0 && frame[4] != 0x0A) cmd_0E();   /* filaire */
```

**Le slot Bluetooth est borné à 1–3 par le code lui-même** — la plage n'était jusqu'ici qu'une
déduction à partir des trois blocs de sélection.

Et la queue de la fonction relâche le handshake :

```c
if (_c_1 != 1) { P0CR &= 0xFB; P0_2 = 1; }   /* P0.2 en entree, relachee haute */
```

### `0x801F` — la jauge de batterie, et la courbe `0xAF8D`

Appelée une fois toutes les six trames d'état. Elle convertit la valeur brute en pourcentage
(`0x0151`, plafonné à 100, nul sous `0x02CB` = 715), puis **fait converger un pourcentage affiché**
vers cette cible, un pas à la fois :

```c
ecart = |affiche - cible|;
delai = CODE[0xAF8D + ecart/10];
if (++compteur >= delai) { compteur = 0; affiche += ±1; }
```

`CODE:0xAF8D` fait **exactement douze octets** :

```
20 10 05 02 02 02 02 02 02 02 02 02
```

Un **amortissement décélérant** : plus la valeur affichée approche du réel, plus elle ralentit.

> Cette courbe était signalée comme inexpliquée depuis le début de l'analyse. Elle l'est.

Écart de 0–9 → 20 tics par pas ; 10–19 → 10 ; 20–29 → 5 ; au-delà → 2.

> **Le listing ci-dessus est en DÉCIMAL, pas en hexadécimal**, contrairement à la plupart des
> extraits de cette page. Le portage s'y est laissé prendre : il a « corrigé » la glose en 32, 16,
> 5, 2 en supposant `0x20 0x10 0x05 0x02`, puis relu le dump. Les octets réels sont
> `14 0a 05 02 02 02 02 02 02 02 02 02` — **20, 10, 5 puis 2**. La prose avait raison, la
> correction était fausse, et le dump a tranché. Leçon retenue : sur cette page, vérifier la base
> avant de corriger une glose.

Le sens de convergence est choisi par `0x26.0`, avec deux compteurs distincts (`0x08C5` à la
montée, `0x08DC` à la descente) et un traitement particulier à 100 % selon `0x2D.3`.
*Inféré :* `0x26.0` = charge en cours.

### La file d'émission radio — correction

`0x0C57` n'est **pas** « un enregistrement de 28 octets par hôte apparié », comme je l'avais
supposé. C'est une **file circulaire de six emplacements de 28 octets** :

| | |
| --- | --- |
| Écriture | `g_radio_host_idx` (`XRAM 0x0307`) |
| Lecture | `XRAM 0x030C` |
| Bouclage | `if (idx > 5) idx = 0` — **six** emplacements |
| Octet 0 de l'emplacement | type : **2** ou **3**, qui devient l'octet de commande |

À l'émission, le type décide de la taille :

```c
if (type == 2) { memcpy(IDATA 0x34, slot, 0x1C); euart0_send(0x1E, 6); }  /* 28 o -> 30 */
if (type == 3) { memcpy(IDATA 0x34, slot, 0x0B); euart0_send(0x0D, 6); }  /* 11 o -> 13 */
```

Et les drapeaux qui remplissent la file sont **les mêmes** que ceux de la chaîne USB de
`hid_report_usb` : `0x2A.4`, `0x27.0`, `0x2A.6` produisent un type 2 ; `0x2A.0` et `0x29.0` un
type 3. Les deux transports se partagent la même source d'événements — ce qui referme le raccord
décrit plus haut.


> **Portée.** `aula_rf.c` reprend la file telle quelle : six emplacements de 28 octets, index
> d'écriture et de lecture séparés, l'octet 0 de l'emplacement servant d'octet de commande, et le
> même découpage 28 → trame de 30 / 11 → trame de 13. Sans elle, toute frappe émise pendant qu'une
> rafale était en vol, ou avec `P4.7` bas, était **perdue sans trace** : `rf_send_payload_long()`
> rendait `false` et `kb.c` ignorait le retour.
>
> Le débordement écrase le plus ancien, comme en usine — son index d'écriture reboucle sans
> consulter celui de lecture. Six emplacements représentent environ deux millisecondes de réserve à
> 260 kbauds ; les atteindre signifie que le module ne répond plus, auquel cas la fraîcheur prime.
>
> **Ce qui n'est PAS repris : l'extinction de relâchement de l'Air60.** Son pilote SPI martèle six
> fois le dernier relâchement en alternant un rapport vide et un rapport portant `0x01` en
> `keys[0]` (*ErrorRollOver*), pour qu'aucun des deux ne soit dédupliqué en aval. C'est un
> contournement pour un bus sans acquittement applicatif, et **rien d'équivalent n'a été observé
> dans le firmware d'usine du F75**. La file, elle, traite la même panne à la source : un
> relâchement qui ne peut pas partir est mis en attente et rejoué. L'inventer par analogie aurait
> été du code sans source.

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

> **Portée.** `aula_rf.c` émet les deux noms au démarrage, un par passage de `rf_task()` dès que
> `P4.7` autorise l'émission — le firmware d'usine, lui, les émet depuis `main` sans se demander si
> le module écoute. Les seize octets sont recopiés du dump, espace de bourrage compris.
>
> **Le slot Bluetooth est désormais persisté**, ce qu'il n'était pas : il retombait à 1 à chaque
> démarrage. Il va dans `user_settings.rf_link`, libre sur ce clavier — ce champ porte ailleurs
> l'encodage `rf_mode_t` de l'Air60, mais son unique consommateur, `restore_rf_link()` dans
> `src/main.c`, est sous `#ifdef RF_ENABLED`, et `RF_ENABLED` et `RF_EUART0` sont mutuellement
> exclusifs. Aucun champ n'est ajouté à `user_settings_t`, ce qui aurait invalidé les réglages
> enregistrés des cinq cartes : `nvm.c` compare la longueur stockée et n'a aucune migration.
>
> On y range le **slot**, pas le lien : sur le F75 le transport vient du sélecteur matériel, et le
> slot est la seule chose que le sélecteur ne dit pas.

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

### Ce qui est porté, et ce qui ne l'est pas

Les cinq objections qui figuraient ici ont été levées une à une par le travail ci-dessus : les
codes de commande ont leur sémantique, la trame d'état reçue est décodée, l'entrée en mode est
tranchée (sélecteur `P7.4`/`P4.5`, commande `0x01`). Reste la cinquième, qui ne se lève pas au
désassemblage : **rien n'a été flashé, donc rien n'a été exécuté.**

`src/platform/bk3632/rf_controller.c` de SMK suppose le SPI bit-bangé de l'Air60 et n'est pas
réutilisable ici ; le transport EUART0 a donc été écrit à part. `meson.build` déclare
`'wireless': 'euart0'` pour ce clavier, ce qui ajoute `-DRF_EUART0=1` et refuse tout
`debug_sink` autre que `console` — l'EUART0 n'a qu'un seul maître.

| Fichier | Contenu |
| --- | --- |
| `src/keyboards/aula-f75/aula_rf.c` | le pilote : ISR, trames, somme de contrôle, machine à états du sélecteur, sonde de présence |
| `src/keyboards/aula-f75/aula_rf.h` | l'API, avec les deux encodages de slot distingués (`rf_link_t` contre `rf_slot_t`, qui se croisent sur la valeur 0) |
| `src/keyboards/aula-f75/kb.c` | aiguillage des rapports HID, et `LNK_BT1..3` sur `Fn`+`1/2/3` |
| `src/platform/sh68f90/interrupts.h` | le vecteur `_INT_EUART0` revendiqué, avec une erreur de compilation si `DEBUG_SINK_UART` le réclame aussi |

Trois écarts assumés par rapport au firmware d'usine, chacun commenté sur place :

1. **Le transport de départ est lu sur le sélecteur**, pas supposé filaire. Le firmware d'usine
   part de `g_transport = 0` et laisse l'anti-rebond converger ; pendant ces dix tics les frappes
   partiraient sur l'USB alors que la glissière dit « sans fil ».
2. **La commande `0x01` est mise en file**, pas émise une fois pour toutes. Le firmware d'usine la
   perd si `P4.7` est bas à cet instant précis ; `rf_task()` la rejoue dès que le module se
   déclare prêt.
3. **`usb_hw_deinit()` à la place du seul `anl USBCON,#0x7F`** : même coupure du module, plus le
   désarmement de l'interruption USB — sans quoi l'ISR de SMK continuerait à tourner sur un
   périphérique éteint.

#### Trois défauts corrigés après coup

Une relecture croisée du pilote contre cette page en a sorti trois, dont un qui rendait la
détection de lien inopérante.

**1. La longueur des trames reçues dépend du type.** La première rédaction attendait 22 octets pour
toutes les trames et sommait les 21 premiers — le format de la seule `0x08`. Une trame d'état de
dix octets restait donc en attente, les octets de la suivante complétaient le tampon, la somme
échouait toujours, et la sonde de présence déclarait le lien mort en permanence.

Le firmware d'usine ne découpe pas non plus dans son ISR : elle empile dans 23 octets d'IDATA et
jette le dépassement ; c'est `euart0_parse` qui dispatche sur l'octet 0 et vérifie la somme **à la
position propre au type**. Le portage fait désormais pareil, avec les deux gardes d'usine sur les
octets 1 et 2 de la trame d'état.

**2. `usb_deinit()` tuait le NKRO en sans-fil.** Il remet toute la machine à états USB à zéro, dont
`interface0_protocol`, et `host_nkro_active()` exige `USB_PROTOCOL_REPORT`. Le clavier retombait
donc en 6KRO dès l'entrée en mode sans-fil, alors que `NKRO_REPORT_BITS` vaut 20 *« limited by
wireless dongle hid descriptor »*. `usb_hw_deinit()` ne coupe que le matériel — plus fidèle au
`anl USBCON,#0x7F` d'usine, et le NKRO survit.

**3. Deux défauts dans `aula_rgb.c`**, encore inactif à ce stade : `value * (0x04B0 / 255)` — la
division entière vaut **4**, donc l'intensité maximale donnait un rapport cyclique de 180 au lieu
de 0 ; et deux tables d'adresses `const __xdata` de 72 octets remplacées par les jetons
`SET_PWM_DUTY_2` de `platform/sh68f90/pwm.h`.

Coût mesuré à la compilation (SDCC 4.5.0, zéro avertissement, `check_interrupts` OK) :
**+251 o de flash**, **+4 o de XDATA**, **3 o de RAM interne** — la marge interne passe de 22 à
19 octets, la pile reste à 222.

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
