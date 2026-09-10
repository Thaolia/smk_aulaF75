#include "kbdef.h"
#include "layout.h"
#include "user_layout.h"
#include "report.h"
#include <stdint.h>

/*
 * Disposition physique dérivée de la table de couleurs d'OpenRGB
 * (Controllers/SinowealthController/SinowealthKeyboard10cController,
 *  aula_f75_layout), où index_LED = colonne * 6 + ligne.
 *
 * Recoupé avec une capture HID physique indépendante (tiagoluizo/smk,
 * docs/superpowers/evidence/2026-08-07-aula-f75-keymap-capture.md), qui mappe
 * la matrice 6x15 sur des usages HID uniques 0x04..0x5D. Les six rangées
 * correspondent position par position.
 *
 * Une seule différence, et elle compte : la capture donne **81** commutateurs,
 * pas 80. La position [ligne 0][colonne 14] est l'**appui d'encodeur** -- une
 * vraie position de matrice, invisible pour OpenRGB parce qu'elle n'a pas de
 * LED. C'est pourquoi num_leds vaut 90 pour 81 touches.
 *
 * La ROTATION de l'encodeur est hors matrice : phases sur P0.5 et P0.6, lues
 * par le poller @ 0x7928 (mov a,P0 ; swap ; rrc ; anl #0x03), appelé depuis le
 * tick. Non implémentée ici -- SMK n'a pas de support d'encodeur pour ce clavier.
 */

// clang-format off

#define LAYOUT_75_ansi( \
    K00_0,        K02_0, K03_0, K04_0, K05_0, K06_0, K07_0, K08_0, K09_0, K10_0, K11_0, K12_0, K13_0, K14_0, \
    K00_1, K01_1, K02_1, K03_1, K04_1, K05_1, K06_1, K07_1, K08_1, K09_1, K10_1, K11_1, K12_1, K13_1, K14_1, \
    K00_2, K01_2, K02_2, K03_2, K04_2, K05_2, K06_2, K07_2, K08_2, K09_2, K10_2, K11_2, K12_2, K13_2, K14_2, \
    K00_3, K01_3, K02_3, K03_3, K04_3, K05_3, K06_3, K07_3, K08_3, K09_3, K10_3, K11_3,        K13_3, K14_3, \
    K00_4, K01_4, K02_4, K03_4, K04_4, K05_4, K06_4, K07_4, K08_4, K09_4, K10_4, K11_4,        K13_4, K14_4, \
    K00_5, K01_5, K02_5,                      K05_5,               K08_5, K09_5,        K12_5, K13_5, K14_5  \
) { \
    { K00_0, KC_NO, K02_0, K03_0, K04_0, K05_0, K06_0, K07_0, K08_0, K09_0, K10_0, K11_0, K12_0, K13_0, K14_0 }, \
    { K00_1, K01_1, K02_1, K03_1, K04_1, K05_1, K06_1, K07_1, K08_1, K09_1, K10_1, K11_1, K12_1, K13_1, K14_1 }, \
    { K00_2, K01_2, K02_2, K03_2, K04_2, K05_2, K06_2, K07_2, K08_2, K09_2, K10_2, K11_2, K12_2, K13_2, K14_2 }, \
    { K00_3, K01_3, K02_3, K03_3, K04_3, K05_3, K06_3, K07_3, K08_3, K09_3, K10_3, K11_3, KC_NO, K13_3, K14_3 }, \
    { K00_4, K01_4, K02_4, K03_4, K04_4, K05_4, K06_4, K07_4, K08_4, K09_4, K10_4, K11_4, KC_NO, K13_4, K14_4 }, \
    { K00_5, K01_5, K02_5, KC_NO, KC_NO, K05_5, KC_NO, KC_NO, K08_5, K09_5, KC_NO, KC_NO, K12_5, K13_5, K14_5 }  \
}

#define _BASE 0
#define _FN   1

#define FN MO(_FN)

const uint16_t keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    /* _BASE
     * ,---------------------------------------------------------------.
     * |Esc| F1| F2| F3| F4| F5| F6| F7| F8| F9|F10|F11|F12|
     * |---------------------------------------------------------------|
     * |  `|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|Bspc|Del |
     * |---------------------------------------------------------------|
     * |Tab |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|   \|PgUp|
     * |---------------------------------------------------------------|
     * |Caps |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|Enter |PgDn|
     * |---------------------------------------------------------------|
     * |Shift |  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|Shift| Up|End |
     * |---------------------------------------------------------------|
     * |Ctrl|Win|Alt|      Space      | Fn|Ctrl| Lft| Dn| Rgt|
     * `---------------------------------------------------------------'
     */
    [_BASE] = LAYOUT_75_ansi(
        KC_ESC,           KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_MUTE,
        KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC, KC_DEL,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS, KC_PGUP,
        KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,          KC_ENT,  KC_PGDN,
        KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_RSFT,          KC_UP,   KC_END,
        KC_LCTL, KC_LGUI, KC_LALT,                            KC_SPC,                    FN,      KC_RCTL,          KC_LEFT, KC_DOWN, KC_RGHT
    ),

    /* _FN */
    [_FN] = LAYOUT_75_ansi(
        _______,          KC_BRID, KC_BRIU, _______, _______, _______, _______, KC_MPRV, KC_MPLY, KC_MNXT, KC_MUTE, KC_VOLD, KC_VOLU, _______,
        _______, LNK_BT1, LNK_BT2, LNK_BT3, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_INS,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, BRI_DN,  BRI_UP,  _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, FX_PREV, FX_NEXT, CLR_NEXT, _______,        _______, KC_HOME,
        _______, _______, _______,                            _______,                   _______, _______,          SPD_DN,  _______, SPD_UP
    ),
};
