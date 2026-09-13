#include "kbdef.h"
#include "layout.h"
#include "user_layout.h"
#include "report.h"
#include <stdint.h>

// clang-format off

#define _BL 0
#define _FL 1

/*
 * Disposition transposée de la table d'usine (CODE 0xC600, 128 x 4 o).
 *
 * La ligne SMK N correspond à la ligne N+1 du firmware d'usine : sa « ligne 0 »
 * est la couche Fn, pas une ligne câblée -- sa routine de lecture (0x8309) ne
 * pose jamais le bit 0. La couche _FL ci-dessous EST cette ligne 0.
 *
 * ── La touche Fn : (ligne 4, colonne 13) ────────────────────────────────────
 *
 * Elle n'apparaît pas dans la table d'usine, qui la traite à part : la console
 * la montrait comme `key 0000`, donc sur une position laissée vide. Localisée
 * par une manche de repérage -- un code témoin distinct sur chaque emplacement
 * vide -- qui a rendu `key 006e`, soit KC_F19, le témoin de la colonne 13.
 *
 * Confirmé ensuite sur l'appareil avec cette disposition-ci : Fn + 1/2/3 sort
 * key 003a/003b/003c, soit F1/F2/F3. La couche commute bien depuis cette case.
 *
 * Les six autres emplacements vides restent NON TESTÉS : il n'a été demandé d'appuyer
 * que sur Fn, donc l'absence de leurs témoins ne dit rien de leur câblage. Une rangée
 * basse de 60 % ANSI en compte huit, ce qui rend KC_NO probable -- pas établi. Les
 * tester exigerait de reflasher une version de repérage ; la fenêtre est passée.
 *
 * ── Les raccourcis RGB de la couche Fn ──────────────────────────────────────
 *
 * Ils reprennent les POSITIONS d'usine (docs/GM610_SYNTHESE.md, « La table des
 * raccourcis Fn ») pour que les doigts ne réapprennent rien :
 *
 *   Fn + \\  effet suivant        Fn + [ / ]   vitesse - / +
 *   Fn + ;  luminosité -         Fn + '       luminosité +
 *
 * Et les sélecteurs de liaison, eux aussi aux positions d'usine :
 *
 *   Fn + Tab (~3 s)   bascule USB <-> sans-fil
 *   Fn + Q / W / E    canaux Bluetooth 1 / 2 / 3, immédiat
 *   Fn + G   (~3 s)   liaison 2,4 GHz
 *
 * Les maintiens de ~3 s sont ceux du firmware d'usine : ils évitent qu'un
 * effleurement coupe la liaison en pleine frappe.
 *
 * ── Le pavé fléché ──────────────────────────────────────────────────────────
 *
 * Ce clavier n'a AUCUNE flèche, et la table d'usine n'en contient nulle part --
 * vérifié sur ses 126 enregistrements. Les trois touches de droite sont donc
 * INVERSÉES : flèche sans Fn, fonction d'origine avec.
 *
 *     touche        sans Fn     avec Fn
 *     AltGr            ←         AltGr
 *     Menu             ↓         ↑
 *     Ctrl droit       →         Ctrl droit
 *
 *     Fn + J        Origine      J et M sont tous deux en colonne 7,
 *     Fn + M        Fin          l'un au-dessus de l'autre
 *
 * Un vrai pavé fléché sous la main droite, sans modificateur à tenir. Le ↑ est
 * le seul qui demande Fn : il n'y a plus de quatrième touche pour l'accueillir.
 *
 * ⚠️ AltGr et Ctrl droit ne sont plus directs. Ctrl reste disponible à gauche ;
 * quant à AltGr, la compensation AZERTY l'injecte elle-même pour @ # [ ] { } etc.
 * (voir gm610_layout.c), donc on ne le tape presque jamais à la main.
 *
 * ⚠️ La touche Menu perd sa fonction d'origine : il n'y a plus de KC_APP.
 *
 * Et Maj + ↑/↓ donne PgPréc / PgSuiv, avec le Maj RETIRÉ du rapport (voir kb.c).
 * Les flèches étant sur la couche de BASE, `Maj + Menu` suffit pour PgSuiv, et
 * `Fn + Maj + Menu` pour PgPréc.
 *
 * ── Deux manques du 60 % ────────────────────────────────────────────────────
 *
 * Fn + Échap = ` et ~. Ce clavier n'a aucune touche accent grave : sa rangée du
 * haut va d'Échap à Retour arrière sans elle. Le firmware d'usine y remettait
 * Échap, ce qui ne servait à rien puisque la touche le donne déjà.
 *
 * Fn + U = Impr. écran.
 *
 * ⛔ Fn + B, MAINTENU ~3 s : retour dans le bootloader d'usine.
 *
 * C'est la porte de secours, et elle est obligatoire sur cette carte : elle n'a
 * aucune entrée ISP matérielle (`OP_ISP = 1`, et le bootloader ne lit aucun
 * GPIO), donc sans elle la SEULE voie vers l'ISP passe par l'USB -- inutile le
 * jour où c'est l'USB qui échoue. Vécu.
 *
 * Fn + D est en plus, sans équivalent d'usine : le balayage de diagnostic des
 * puits RGB. Il allume une paire (rangée, couleur) à la fois et l'annonce sur
 * la console -- c'est l'instrument qui sert à corriger la carte des puits.
 */
const uint16_t keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BL] = {
        {KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC},
        {KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS},
        {KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_NUHS, KC_ENT },
        {KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_INT1, KC_NO,   KC_RSFT},
        {KC_LCTL, KC_LGUI, KC_LALT, KC_NO,   KC_NO,   KC_SPC,  KC_NO,   KC_NO,   KC_LEFT, KC_DOWN, KC_NO,   KC_NO,   KC_RGHT, MO(_FL)},
    },
    [_FL] = {
        {KC_GRV,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_DEL },
        {LNK_TOGGLE,LNK_BT1,LNK_BT2,LNK_BT3,_______, _______, _______, KC_PSCR, _______, _______, _______, SPD_DN,  SPD_UP,  FX_NEXT},
        {_______, _______, _______, RGB_DIAG,_______, LNK_24G, _______, KC_HOME, _______, _______, BRI_DN,  BRI_UP,  _______, _______},
        {_______, _______, _______, _______, _______, KB_BOOT, _______, KC_END,  _______, _______, _______, _______, _______, _______},
        {LAYOUT_AZ,_______, _______, _______, _______, _______, _______, _______, KC_RALT, KC_UP,   _______, _______, KC_RCTL, _______},
    },
};

bool layout_process_record(uint16_t keycode, bool key_pressed)
{
    (void)keycode;
    (void)key_pressed;
    return true;
}
