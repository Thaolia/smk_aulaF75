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

/* Rétroéclairage : 18 sorties PWM = 6 lignes physiques de LED x R/G/B.
 * Confirmé par P1CR=P2CR=P3CR=0x3F (les 18 broches PWM en sortie).
 * Correspondance MCU déduite du NuPhy Air60 : PWM0x<->P3_x, PWM1x<->P2_x,
 * PWM2x<->P1_x. Les colonnes de matrice servent de sélecteurs de multiplexage. */
#define LED_PWM_C0  PWM00
#define LED_PWM_C1  PWM01
#define LED_PWM_C2  PWM02
#define LED_PWM_C3  PWM03
#define LED_PWM_C4  PWM04
#define LED_PWM_C5  PWM05
#define LED_PWM_C6  PWM10
#define LED_PWM_C7  PWM11
#define LED_PWM_C8  PWM12
#define LED_PWM_C9  PWM13
#define LED_PWM_C10 PWM14
#define LED_PWM_C11 PWM15
#define LED_PWM_C12 PWM20
#define LED_PWM_C13 PWM21
#define LED_PWM_C14 PWM22
#define LED_PWM_C15 PWM23
#define LED_PWM_C16 PWM24
#define LED_PWM_C17 PWM25

/* -------------------------------------------------------------------------
 * Liaison sans fil
 *
 * Le NuPhy Air60 parle à son BK3632 en SPI bit-bangé (RF_BB_SPI_*,
 * src/platform/bb_spi.c). L'AULA F75 ne fonctionne PAS comme ça : son firmware
 * d'usine utilise EUART0 (vecteur 13, SCON 0xD8 / SBUF 0xAA), en half-duplex --
 * l'ISR bascule la direction d'une broche via P0CR puis P0.2.
 *
 * Conséquence : src/platform/bk3632/rf_controller.c n'est PAS réutilisable tel
 * quel pour ce clavier. Il faudrait un transport EUART0. Le sans-fil est donc
 * hors périmètre de ce portage ; ne pas déclarer 'wireless' dans meson.build.
 * ------------------------------------------------------------------------- */

enum custom_keycodes {
    FX_NEXT = SAFE_RANGE, /* effet RGB suivant */
    FX_PREV,              /* effet RGB précédent */
    BRI_UP,               /* luminosité + */
    BRI_DN,               /* luminosité - */
    SPD_UP,               /* vitesse + */
    SPD_DN,               /* vitesse - */

    KB_SAFE_RANGE,
};
