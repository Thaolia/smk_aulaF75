#include "kbdef.h"
#include "layout.h"
#include "user_layout.h"
#include "report.h"
#include <stdint.h>

/*
 * Une seule couche, entièrement vide. La matrice n'est de toute façon jamais
 * lue -- `user_matrix_read_rows()` rend 0xFF -- mais le tableau doit exister et
 * avoir les bonnes dimensions pour que `matrix.c` s'édite en liens.
 */
const uint16_t keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = {
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
    },
};

bool layout_process_record(uint16_t keycode, bool key_pressed)
{
    (void)keycode;
    (void)key_pressed;
    return true;
}
