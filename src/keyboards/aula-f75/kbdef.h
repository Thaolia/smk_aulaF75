#pragma once

#include "sh68f90.h"
#include "keycodes.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Epomaker x AULA F75 (modèle classique, PAS le "F75 Ultra")
 *
 *   MCU      SinoWealth SH68F90A, marquage IC BYK916
 *   USB      258A:010C  (lus dans la flash d'usine en 0x5FCE)
 *   Sans fil BK3632 Beken, relié par EUART0 -- voir la note plus bas
 *
 * ATTENTION : ce portage est INCOMPLET par construction. Le brochage n'a pas
 * été établi et n'est pas devinable depuis le firmware d'usine sans travail
 * supplémentaire. Les symboles manquants provoquent volontairement une erreur
 * de compilation : un fichier qui refuse de compiler est un livrable correct,
 * un fichier qui compile en briquant le clavier ne l'est pas.
 *
 * Feuille de relevé : docs/keyboards/aula-f75.md
 */

/* -------------------------------------------------------------------------
 * Matrice
 *
 * Établi : la table de couleurs d'OpenRGB expose num_leds = 90 et indexe les
 * LED en colonnes -- index = colonne * 6 + ligne. L'indice max est 89 (flèche
 * droite) et l'indice 6 (entre Échap et F1) est le trou de la rangée F.
 * Donc la grille LED est 6 lignes x 15 colonnes.
 *
 * NON établi : que la matrice de touches partage cette géométrie. C'est le cas
 * habituel sur ces claviers, mais le firmware d'usine ne l'a pas confirmé --
 * aucune borne de boucle de scan n'a été isolée. Une 16e colonne sans LED
 * resterait invisible pour OpenRGB.
 * ------------------------------------------------------------------------- */
#define MATRIX_ROWS 6
#define MATRIX_COLS 15

/* -------------------------------------------------------------------------
 * Brochage -- ÉTABLI PAR DÉSASSEMBLAGE, TRIPLEMENT RECOUPÉ
 *
 * Sources concordantes :
 *  1. init GPIO consolidée du firmware d'usine, fcn @ 0xA7EC :
 *       P0CR=0x9C  P1CR=0x3F  P2CR=0x3F  P3CR=0x3F
 *       P4CR=0x4D  P5CR=0x87  P6CR=0xFF  P7CR=0x60
 *     (bit CR à 1 = sortie, cf. GPIO_OUTPUT dans platform/sh68f90/gpio.h)
 *  2. table de saut de sélection de colonne @ 0x72F4 : 20 emplacements dont
 *     15 peuplés, chaque handler relâchant la colonne précédente (setb) avant
 *     de sélectionner la suivante (clr) -- l'emplacement 0 relâche P4.3, ce qui
 *     ferme la boucle et fixe P4.3 comme dernière colonne.
 *  3. portage indépendant tiagoluizo/smk@aula-f75-port, qui aboutit au même
 *     brochage par une analyse séparée.
 *
 * Sélection de colonne ACTIVE BASSE. Lignes ACTIVES BASSES (pull-ups internes).
 * ------------------------------------------------------------------------- */
#define AULA_F75_PINMAP_VERIFIED 1

/* Lignes -- entrées. Confirmé : P7CR=0x60 laisse P7.0-3 en entrée,
 * P5CR=0x87 laisse P5.3 et P5.4 en entrée. */
#define KB_R0_P7_0 _P7_0
#define KB_R1_P7_1 _P7_1
#define KB_R2_P7_2 _P7_2
#define KB_R3_P7_3 _P7_3
#define KB_R4_P5_3 _P5_3
#define KB_R5_P5_4 _P5_4

#define KB_R0 P7_0
#define KB_R1 P7_1
#define KB_R2 P7_2
#define KB_R3 P7_3
#define KB_R4 P5_3
#define KB_R5 P5_4

/* Colonnes -- sorties, dans l'ordre de scan lu dans la table @ 0x72F4 */
#define KB_C0_P6_0  _P6_0
#define KB_C1_P6_1  _P6_1
#define KB_C2_P6_2  _P6_2
#define KB_C3_P6_3  _P6_3
#define KB_C4_P6_4  _P6_4
#define KB_C5_P6_5  _P6_5
#define KB_C6_P6_6  _P6_6
#define KB_C7_P6_7  _P6_7
#define KB_C8_P5_0  _P5_0
#define KB_C9_P5_1  _P5_1
#define KB_C10_P5_2 _P5_2
#define KB_C11_P5_7 _P5_7
#define KB_C12_P4_0 _P4_0
#define KB_C13_P4_2 _P4_2
#define KB_C14_P4_3 _P4_3

#define KB_C0  P6_0
#define KB_C1  P6_1
#define KB_C2  P6_2
#define KB_C3  P6_3
#define KB_C4  P6_4
#define KB_C5  P6_5
#define KB_C6  P6_6
#define KB_C7  P6_7
#define KB_C8  P5_0
#define KB_C9  P5_1
#define KB_C10 P5_2
#define KB_C11 P5_7
#define KB_C12 P4_0
#define KB_C13 P4_2
#define KB_C14 P4_3

/* Masques par port, dérivés des broches ci-dessus */
#define KB_C_P4_MASK (uint8_t)(KB_C12_P4_0 | KB_C13_P4_2 | KB_C14_P4_3)
#define KB_C_P5_MASK (uint8_t)(KB_C8_P5_0 | KB_C9_P5_1 | KB_C10_P5_2 | KB_C11_P5_7)
#define KB_C_P6_MASK (uint8_t)(KB_C0_P6_0 | KB_C1_P6_1 | KB_C2_P6_2 | KB_C3_P6_3 | \
                               KB_C4_P6_4 | KB_C5_P6_5 | KB_C6_P6_6 | KB_C7_P6_7)
#define KB_R_P5_MASK (uint8_t)(KB_R4_P5_3 | KB_R5_P5_4)
#define KB_R_P7_MASK (uint8_t)(KB_R0_P7_0 | KB_R1_P7_1 | KB_R2_P7_2 | KB_R3_P7_3)

/* Rétroéclairage : 18 sorties PWM = 6 lignes physiques de LED x R/G/B.
 * Confirmé par P1CR=P2CR=P3CR=0x3F (les 18 broches PWM en sortie).
 * Correspondance MCU déduite du NuPhy Air60 : PWM0x<->P3_x, PWM1x<->P2_x,
 * PWM2x<->P1_x. Les colonnes de matrice servent de sélecteurs de multiplexage.
 *
 * Il n'y a PAS de macros LED_PWM_Cn ici, contrairement aux autres claviers de
 * SMK : chez eux un canal PWM pilote une colonne LED, ici il pilote une
 * (ligne, couleur). La correspondance canal -> registre vit en un seul endroit,
 * `aula_rgb_load_column()`, dans l'ordre de chargement du firmware d'usine --
 * qui n'est pas l'ordre naturel des numéros de canal. Une seconde table avec
 * une autre origine d'index n'aurait servi qu'à se contredire.
 *
 * Un balayage LED complet -- les quinze colonnes -- entre deux balayages de
 * matrice, comme le genesis-thor-300, seul autre clavier de SMK à partager ses
 * colonnes entre le scan et le multiplexage LED. */
#define LED_SUBFRAMES_PER_SCAN MATRIX_COLS

/*
 * Instantané de l'état des touches par colonne, bit N = ligne N enfoncée.
 * Alimenté par les crochets de scan de `user_matrix.c`, consommé par le moteur
 * réactif : `matrix.c` garde son tableau privé et ses crochets ne passent pas
 * la position, mais nos propres crochets, eux, la voient.
 */
uint8_t user_matrix_pressed(uint8_t col);

/* Dix crans de luminosité : la table de gain d'usine en CODE 0x2937 en compte
 * dix (00 08 10 18 20 28 32 3c 46 50), bornée par CODE[0xA8D5+effet] = 9. */
#define LED_BRIGHTNESS_LEVELS 10

/* -------------------------------------------------------------------------
 * Liaison sans fil
 *
 * Le NuPhy Air60 parle à son BK3632 en SPI bit-bangé (RF_BB_SPI_*,
 * src/platform/bb_spi.c). L'AULA F75 ne fonctionne PAS comme ça : son firmware
 * d'usine utilise EUART0 (vecteur 13, SCON 0xD8 / SBUF 0xAA), en half-duplex --
 * l'ISR bascule la direction d'une broche via P0CR puis P0.2.
 *
 * Conséquence : src/platform/bk3632/rf_controller.c n'est PAS réutilisable tel
 * quel pour ce clavier. Le transport EUART0 a donc été écrit à part, dans
 * aula_rf.c, et meson.build déclare 'wireless': 'euart0' pour ce clavier.
 *
 * Sélecteur de connexion, trois positions (établi par fcn.000084E9) :
 *   P7.4 = 0            -> 2,4 GHz (dongle)
 *   P7.4 = 1, P4.5 = 1  -> filaire USB
 *   P7.4 = 1, P4.5 = 0  -> Bluetooth, slot choisi par LNK_BT1..3
 * P4.7 est la ligne « module prêt » : aucune trame ne part quand elle est basse.
 *
 * ÉTAT : transcrit du désassemblage, puis CONFIRMÉ SUR L'APPAREIL -- frappe par
 * le dongle 2,4 GHz et appairage Bluetooth. La panne initiale n'était pas dans
 * le pilote : `tick.c` balaie toute la matrice dans l'ISR Timer2 (~320 µs) et,
 * à 260 870 bauds, un octet tombe toutes les 38 µs sans FIFO sur SBUF. À
 * priorité égale l'EUART0 ne pouvait pas préempter et perdait huit octets par
 * balayage. `rf_uart_init()` pose donc `IPH1/IPL1 |= _ES0`, comme l'usine
 * (IPH1=0x42, IPL1=0x41). Le keycode RF_DIAG reste : c'est l'instrument qui l'a
 * montré, et le seul utilisable sur batterie.
 * ------------------------------------------------------------------------- */

enum custom_keycodes {
    FX_NEXT = SAFE_RANGE, /* effet RGB suivant */
    FX_PREV,              /* effet RGB précédent */
    BRI_UP,               /* luminosité + */
    BRI_DN,               /* luminosité - */
    SPD_UP,               /* vitesse + */
    SPD_DN,               /* vitesse - */
    CLR_NEXT,             /* couleur suivante : 0-6 fixes, 7 arc-en-ciel */
    DIR_TOG,              /* sens de défilement : le bit 0x23 d'usine */

    /*
     * Emplacements Bluetooth. Le transport lui-même vient de la glissière, pas
     * d'une touche : ces trois-là ne choisissent QUE le slot, comme la commande
     * 0x01 du firmware d'usine. Un appui sur le slot déjà actif relance
     * l'appairage -- c'est ce que fait le firmware d'usine quand le bit 0x2C.5
     * est posé (drapeau 1 de la commande 0x01, fcn.0000870C).
     */
    LNK_BT1,
    LNK_BT2,
    LNK_BT3,

    /*
     * Relance d'appairage, sur `Fn + \``  -- la position du raccourci d'usine
     * (clé 0x08 du répartiteur 0x4136, arm 0x426A).
     *
     * DIVERGENCE ASSUMÉE : l'arm d'usine exige le transport 2,4 GHz et ne
     * réappaire donc que le dongle. Ici la commande part avec `rf_link_slot()`,
     * qui rend le slot du transport COURANT : la même touche relance le dongle
     * en 2,4 GHz et l'emplacement actif en Bluetooth. Sans effet en filaire.
     */
    RF_PAIR,

    /*
     * Overlay de diagnostic radio sur les touches `1` à `0`. Le sans-fil échoue
     * clavier sur batterie, USB débranché : la console HID n'existe pas dans ce
     * montage, les LED oui.
     */
    RF_DIAG,

    /*
     * Mode « frappe automatique », sur `Fn + Espace` ET `Fn + appui molette`.
     * Tape une lettre au hasard puis l'efface d'un retour arrière, toutes les
     * 1 s ± 500 ms, jusqu'à ce qu'une touche quelconque l'arrête. Aucun
     * équivalent d'usine. Voir `aula_macro.c`.
     */
    MACRO_TG,

    KB_SAFE_RANGE,
};
