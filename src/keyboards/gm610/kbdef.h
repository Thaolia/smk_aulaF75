#pragma once

#include "sh68f90.h"
#include "keycodes.h"

/*
 * Newmen GM610 -- 60 %, 5 rangées physiques x 14 colonnes.
 *
 * Brochage relevé par reverse du firmware d'usine (voir docs/GM610_SYNTHESE.md,
 * « Brochage physique de la matrice ») :
 *
 *   lignes   0-4   P7.1 P7.2 P7.3 P5.3 P5.4     lues par 0x8309, actif bas
 *   colonnes 0-13  voir plus bas               excitées par 0x1D9C, actif bas
 *
 * Les lignes sont IDENTIQUES à celles de l'eyooso-z11 (même design de
 * référence) ; les colonnes diffèrent.
 *
 * ⚠️ P5 PORTE À LA FOIS DES COLONNES ET DES LIGNES : bits 0-2 en sortie
 * (colonnes 0-2), bits 3-4 en entrée avec tirage (lignes 3-4). Le firmware
 * d'usine le marque lui-même — sa routine de relâchement fait `ORL P5,#0x07`
 * et non `#0x3F`, épargnant précisément les deux bits de ligne.
 *
 * Le bootloader ISP d'usine reste en place : rien n'écrit au-dessus de 0xEFFF,
 * et le secteur marqueur 0xEE00-0xEFFF (qui contient l'octet d'armement
 * 0xEFFB) n'est écrit par personne -- cf. l'assertion statique de nvm.c.
 */

#define MATRIX_ROWS 5
#define MATRIX_COLS 14

// Row Pin Bits
#define KB_R0_P7_1 _P7_1
#define KB_R1_P7_2 _P7_2
#define KB_R2_P7_3 _P7_3
#define KB_R3_P5_3 _P5_3
#define KB_R4_P5_4 _P5_4

// Row Pins
#define KB_R0 P7_1
#define KB_R1 P7_2
#define KB_R2 P7_3
#define KB_R3 P5_3
#define KB_R4 P5_4

// Column Pin Bits
#define KB_C0_P5_0  _P5_0
#define KB_C1_P5_1  _P5_1
#define KB_C2_P5_2  _P5_2
#define KB_C3_P3_5  _P3_5
#define KB_C4_P3_4  _P3_4
#define KB_C5_P3_3  _P3_3
#define KB_C6_P3_2  _P3_2
#define KB_C7_P3_1  _P3_1
#define KB_C8_P3_0  _P3_0
#define KB_C9_P2_5  _P2_5
#define KB_C10_P2_4 _P2_4
#define KB_C11_P2_3 _P2_3
#define KB_C12_P2_2 _P2_2
#define KB_C13_P2_1 _P2_1

// Column Pins
#define KB_C0  P5_0
#define KB_C1  P5_1
#define KB_C2  P5_2
#define KB_C3  P3_5
#define KB_C4  P3_4
#define KB_C5  P3_3
#define KB_C6  P3_2
#define KB_C7  P3_1
#define KB_C8  P3_0
#define KB_C9  P2_5
#define KB_C10 P2_4
#define KB_C11 P2_3
#define KB_C12 P2_2
#define KB_C13 P2_1

// Masques par port, pour l'initialisation et le relâchement groupé.
#define KB_C_P5_MASK (uint8_t)(KB_C0_P5_0 | KB_C1_P5_1 | KB_C2_P5_2)
#define KB_C_P3_MASK (uint8_t)(KB_C3_P3_5 | KB_C4_P3_4 | KB_C5_P3_3 | KB_C6_P3_2 | KB_C7_P3_1 | KB_C8_P3_0)
#define KB_C_P2_MASK (uint8_t)(KB_C9_P2_5 | KB_C10_P2_4 | KB_C11_P2_3 | KB_C12_P2_2 | KB_C13_P2_1)

#define KB_R_P7_MASK (uint8_t)(KB_R0_P7_1 | KB_R1_P7_2 | KB_R2_P7_3)
#define KB_R_P5_MASK (uint8_t)(KB_R3_P5_3 | KB_R4_P5_4)

/* ═══════════════════════════════════════════════════════════════════════════
 * RGB
 *
 * Topologie, relevée dans le firmware d'usine (docs/GM610_SYNTHESE.md, « Le
 * câblage des 21 canaux PWM ») : les colonnes sont des sorties PWM -- les
 * MÊMES broches que les colonnes de matrice -- et les puits sont 3 broches par
 * rangée, une par couleur. `0x1D9C` rebascule une colonne en GPIO le temps du
 * scan, puis la rend au PWM.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Colonne -> canal PWM. Relevé un par un dans la table de sauts de
 * `PwmSetChannel` (0x56A7 -> 0x56B8) : chaque gestionnaire écrit la paire
 * DUTY2 de son canal. Les trois premiers écrivent des SFR (PWM40/41/42),
 * les autres de la XDATA -- d'où le comptage initial erroné à « 18 câblés ».
 *
 * La table prolonge exactement celle de la nuphy-air60 : même design de
 * référence, 21 colonnes au lieu de 16. Nous n'en câblons que 14.
 */
#define LED_PWM_C0  PWM40 // P5.0
#define LED_PWM_C1  PWM41 // P5.1
#define LED_PWM_C2  PWM42 // P5.2
#define LED_PWM_C3  PWM05 // P3.5
#define LED_PWM_C4  PWM04 // P3.4
#define LED_PWM_C5  PWM03 // P3.3
#define LED_PWM_C6  PWM02 // P3.2
#define LED_PWM_C7  PWM01 // P3.1
#define LED_PWM_C8  PWM00 // P3.0
#define LED_PWM_C9  PWM15 // P2.5
#define LED_PWM_C10 PWM14 // P2.4
#define LED_PWM_C11 PWM13 // P2.3
#define LED_PWM_C12 PWM12 // P2.2
#define LED_PWM_C13 PWM11 // P2.1

/*
 * Puits : 5 rangées x 3 couleurs = 15 broches.
 *
 * ⚠️ ÉTAT DE LA PREUVE -- à lire avant de s'y fier.
 *
 * ÉTABLI dans le firmware GM610 : la rangée 4. Un chemin d'indicateur dédié
 * (phases 19-21, canal 13) fait `SETB P4.3` / `SETB P6.5` / `SETB P4.4` en
 * 0x1A79 / 0x1AA4 / 0x1AD3 -- soit B / G / R.
 *
 * CORROBORÉ par l'initialisation des ports du firmware d'usine (0x8500), qui
 * met en sortie et à zéro EXACTEMENT ces 15 broches et pas d'autres :
 *     P0PCR = 0x1C  -> P0.2 P0.3 P0.4        P0 = 0x20 (seul P0.5, radio, haut)
 *     P4PCR = 0x7B  -> inclut P4.3..P4.6     P4 = 0x80 (seul P4.7, radio, haut)
 *     P5CR  = 0x87  -> inclut P5.7           P5 = 0x07 (colonnes relâchées)
 *     P6CR  = 0xFF                           P6 = 0x00
 *
 * ✅ VÉRIFIÉ SUR L'APPAREIL le 2026-09-13, par le balayage de diagnostic --
 * un seul canal allumé à la fois, donc aucune couleur composée à interpréter :
 *
 *     pas « L0 R  P6.1 » -> VERT      -> P6.1 = vert
 *     pas « L0 G  P0.4 » -> ROUGE     -> P0.4 = rouge
 *     pas « L0 B  P0.3 » -> BLEU      -> P0.3 = bleu
 *
 * ...c'est-à-dire la carte de la nuphy-air60, telle quelle.
 *
 * ⛔ J'avais permuté rouge et vert un moment, sur une observation faite avec
 * une COULEUR COMPOSÉE (l'illumination de la 2,4 GHz, rendue « rouge »). Deux
 * canaux allumés ensemble ne permettent pas de conclure sur l'un d'eux, et la
 * console montrait d'ailleurs que le raccourci soupçonné n'avait jamais abouti.
 * La leçon : pour identifier un canal, n'en allumer qu'un.
 *
 * ÉTABLI PAR MESURE : l'alignement rangée LED <-> rangée physique. L'onde de
 * l'effet « goutte » part bien de la touche frappée, ce qu'une permutation de
 * rangées rendrait illisible.
 */
#define RGB_R0R P0_4
#define RGB_R0G P6_1
#define RGB_R0B P0_3
#define RGB_R1R P6_7
#define RGB_R1G P6_2
#define RGB_R1B P6_6
#define RGB_R2R P0_2
#define RGB_R2G P6_3
#define RGB_R2B P5_7
#define RGB_R3R P4_5
#define RGB_R3G P6_4
#define RGB_R3B P4_6
#define RGB_R4R P4_4 // 0x1AD3 : SETB P4.4
#define RGB_R4G P6_5 // 0x1AA4 : SETB P6.5
#define RGB_R4B P4_3 // 0x1A79 : SETB P4.3


// Masques groupés. Chacun n'inclut QUE des broches de puits : P0 épargne la
// radio (P0.5-7), P4 épargne l'horloge radio (P4.7), P5 épargne colonnes et
// lignes (bits 0-4).
#define RGB_P0_MASK (uint8_t)(_P0_2 | _P0_3 | _P0_4)
#define RGB_P4_MASK (uint8_t)(_P4_3 | _P4_4 | _P4_5 | _P4_6)
#define RGB_P5_MASK (uint8_t)(_P5_7)
#define RGB_P6_MASK (uint8_t)(_P6_1 | _P6_2 | _P6_3 | _P6_4 | _P6_5 | _P6_6 | _P6_7)

/* Instantané des touches enfoncées d'une colonne, pour l'effet réactif :
 * `kb_process_record` ne reçoit qu'un code de touche, jamais sa position. */
uint8_t user_matrix_pressed(uint8_t col);

/* ═══════════════════════════════════════════════════════════════════════════
 * Liaison radio -- SoC BK3632, bus bit-bang
 *
 * Les SIX broches sont celles relevées dans le firmware d'usine
 * (docs/GM610_SYNTHESE.md, section 8) et elles coïncident **une à une** avec
 * celles que `src/platform/bb_spi.c` attend, y compris l'acquittement P4.2 que
 * le firmware d'usine ne lit qu'en `JNB` -- invisible à une recherche
 * d'écritures. `bb_spi.c` fige d'ailleurs les numéros de port (0, 4, 7) dans
 * ses macros : rien d'autre ne conviendrait.
 * ═══════════════════════════════════════════════════════════════════════════ */
#define RF_BB_SPI_CS   P7_4
#define RF_BB_SPI_SCK  P4_7
#define RF_BB_SPI_MISO P0_6
#define RF_BB_SPI_MOSI P0_7
#define RF_BB_SPI_MOT  P0_5
#define RF_BB_SPI_ACK  P4_2

#define RF_BB_SPI_CS_P7_4   _P7_4
#define RF_BB_SPI_SCK_P4_7  _P4_7
#define RF_BB_SPI_MISO_P0_6 _P0_6
#define RF_BB_SPI_MOSI_P0_7 _P0_7
#define RF_BB_SPI_MOT_P0_5  _P0_5
#define RF_BB_SPI_ACK_P4_2  _P4_2

// Mode de liaison courant, pour l'étage d'éclairage.
#define KB_CONN_RF  ((uint8_t)0)
#define KB_CONN_USB ((uint8_t)1)
uint8_t kb_conn_mode(void);

enum custom_keycodes {
    FX_NEXT = SAFE_RANGE, // Fn + \  : effet suivant
    BRI_UP,               // Fn + '   : luminosité +
    BRI_DN,               // Fn + ;   : luminosité -
    SPD_UP,               // Fn + ]   : vitesse +
    SPD_DN,               // Fn + [   : vitesse -
    RGB_DIAG,             // Fn + D   : balayage de diagnostic des puits

    // Liaison. Positions et temporisations reprises de l'usine.
    LNK_TOGGLE,           // Fn + Tab, ~3 s : bascule USB <-> sans-fil
    LNK_BT1,              // Fn + Q   : canal Bluetooth 1
    LNK_BT2,              // Fn + W   : canal Bluetooth 2
    LNK_BT3,              // Fn + E   : canal Bluetooth 3
    LNK_24G,              // Fn + G, ~3 s : liaison 2,4 GHz

    /*
     * ⛔ LA PORTE DE SECOURS -- Fn + B, maintenu ~3 s.
     *
     * Cette carte n'a AUCUNE entrée ISP matérielle : `OP_ISP = 1` dans ses code
     * options, et les 4 096 octets du bootloader ne lisent pas un seul GPIO. La
     * seule voie vers le bootloader passait par un SET_REPORT sur la collection
     * vendeur de l'USB -- inutile le jour où c'est justement l'USB qui échoue.
     *
     * Vécu le 2026-09-13 : le clavier tournait (LED, touches, Bluetooth) mais
     * l'hôte échouait sur GET_DESCRIPTOR, et il n'existait plus aucun moyen de
     * le reflasher. `isp_jump()` tient en quatre instructions et n'a besoin de
     * rien -- il n'y avait aucune raison de ne pas l'exposer.
     */
    KB_BOOT,

    /*
     * Fn + Ctrl gauche : bascule la compensation AZERTY.
     *
     * Les capuchons portent la disposition US International ; quand l'hôte est
     * en AZERTY français, les deux se contredisent -- la touche marquée `A`
     * écrit `q`. Ce mode fait porter la traduction par le CLAVIER : on frappe
     * ce qui est écrit sur les capuchons, sans rien changer côté PC.
     *
     * ACTIF PAR DÉFAUT sur cette carte.
     */
    LAYOUT_AZ,

    KB_SAFE_RANGE,
};
