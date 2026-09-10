#include "kbdef.h"
#include "gpio.h"
#include "user_matrix.h"

/*
 * Scan de matrice 6 lignes x 15 colonnes.
 *
 * Sélection de colonne ACTIVE BASSE, lignes lues ACTIVES BASSES avec pull-ups.
 * Ordre de scan lu dans la table de saut du firmware d'usine @ 0x72F4.
 */

/* Les 18 sorties PWM du rétroéclairage : 6 lignes de LED x R/G/B */
#define LED_P1_MASK (uint8_t)0x3f
#define LED_P2_MASK (uint8_t)0x3f
#define LED_P3_MASK (uint8_t)0x3f

void user_matrix_cols_deselect_all(void)
{
    GPIO_HIGH(4, KB_C_P4_MASK);
    GPIO_HIGH(5, KB_C_P5_MASK);
    GPIO_HIGH(6, KB_C_P6_MASK);
}

void user_matrix_scan_pre(void)
{
    GPIO_OUTPUT(4, KB_C_P4_MASK);
    GPIO_OUTPUT(5, KB_C_P5_MASK);
    GPIO_OUTPUT(6, KB_C_P6_MASK);
}

void user_matrix_scan_post(void)
{
    GPIO_PULLUP_OFF(4, KB_C_P4_MASK);
    GPIO_INPUT(4, KB_C_P4_MASK);
    GPIO_PULLUP_OFF(5, KB_C_P5_MASK);
    GPIO_INPUT(5, KB_C_P5_MASK);
    GPIO_PULLUP_OFF(6, KB_C_P6_MASK);
    GPIO_INPUT(6, KB_C_P6_MASK);
}

/*
 * Instantané de l'état des touches, pour le rétroéclairage réactif.
 *
 * `src/smk/matrix.c` garde son tableau `matrix[]` privé et ses crochets ne
 * reçoivent pas la position ; `kb_process_record()` ne voit qu'un keycode. Or
 * le moteur réactif a besoin de (ligne, colonne).
 *
 * La position transite pourtant ici deux fois par colonne : `col_select` la
 * reçoit, et `read_rows` est appelée juste après. Mémoriser l'une et ranger
 * l'autre suffit, sans toucher à une seule ligne de code partagé. Coût : quinze
 * octets et un rangement par lecture de lignes.
 *
 * Le balayage de matrice et les sous-trames LED s'excluent dans le temps
 * (`tick.c` les alterne dans la même ISR), donc pas de course à craindre.
 */
static __xdata uint8_t scan_pressed[MATRIX_COLS];
static uint8_t         scan_col;

uint8_t user_matrix_pressed(uint8_t col)
{
    return (col < MATRIX_COLS) ? scan_pressed[col] : 0;
}

void user_matrix_col_select(uint8_t col) // actif bas : on tire à 0
{
    scan_col = col;

    switch (col) {
        case 0:  KB_C0  = 0; break;
        case 1:  KB_C1  = 0; break;
        case 2:  KB_C2  = 0; break;
        case 3:  KB_C3  = 0; break;
        case 4:  KB_C4  = 0; break;
        case 5:  KB_C5  = 0; break;
        case 6:  KB_C6  = 0; break;
        case 7:  KB_C7  = 0; break;
        case 8:  KB_C8  = 0; break;
        case 9:  KB_C9  = 0; break;
        case 10: KB_C10 = 0; break;
        case 11: KB_C11 = 0; break;
        case 12: KB_C12 = 0; break;
        case 13: KB_C13 = 0; break;
        case 14: KB_C14 = 0; break;
    }
}

void user_matrix_col_deselect(uint8_t col)
{
    switch (col) {
        case 0:  KB_C0  = 1; break;
        case 1:  KB_C1  = 1; break;
        case 2:  KB_C2  = 1; break;
        case 3:  KB_C3  = 1; break;
        case 4:  KB_C4  = 1; break;
        case 5:  KB_C5  = 1; break;
        case 6:  KB_C6  = 1; break;
        case 7:  KB_C7  = 1; break;
        case 8:  KB_C8  = 1; break;
        case 9:  KB_C9  = 1; break;
        case 10: KB_C10 = 1; break;
        case 11: KB_C11 = 1; break;
        case 12: KB_C12 = 1; break;
        case 13: KB_C13 = 1; break;
        case 14: KB_C14 = 1; break;
    }
}

/*
 * Ligne N sur le bit N. Reproduit exactement le calcul du firmware d'usine
 * (0x73A6) : P5 décalé d'un cran puis masqué 0x30, combiné aux 4 bits bas de
 * P7, bits inutilisés forcés à 1 -- même convention que SMK, qui inverse
 * ensuite l'échantillon.
 */
uint8_t user_matrix_read_rows(void)
{
    const uint8_t raw = (uint8_t)((P7 & 0x0f) | ((P5 << 1) & 0x30) | 0xc0);

    /* Lignes actives basses : un bit à 1 dans l'instantané est une touche
     * enfoncée. `matrix.c` fait le même complément de son côté. */
    if (scan_col < MATRIX_COLS) {
        scan_pressed[scan_col] = (uint8_t)(~raw) & 0x3f;
    }
    return raw;
}

void user_matrix_sinks_off(void)
{
    GPIO_LOW(1, LED_P1_MASK);
    GPIO_LOW(2, LED_P2_MASK);
    GPIO_LOW(3, LED_P3_MASK);
}
