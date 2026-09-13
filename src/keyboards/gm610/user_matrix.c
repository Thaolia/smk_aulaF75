#include "kbdef.h"
#include "gpio.h"
#include "user_matrix.h"

/*
 * Actif bas, comme le firmware d'usine : une colonne est excitée en la mettant
 * à 0, les lignes sont lues avec tirage vers le haut (touche appuyée = 0).
 */

static uint8_t         scan_col;
static __xdata uint8_t scan_pressed[MATRIX_COLS];

void user_matrix_cols_deselect_all(void)
{
    GPIO_HIGH(5, KB_C_P5_MASK);   // n'effleure PAS P5.3/P5.4, qui sont des lignes
    GPIO_HIGH(3, KB_C_P3_MASK);
    GPIO_HIGH(2, KB_C_P2_MASK);
}

void user_matrix_col_select(uint8_t col)
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
    }
}

void user_matrix_scan_pre(void) {}
void user_matrix_scan_post(void) {}

/*
 * Appelé par `matrix_scan_full()` AVANT le balayage. Les puits RGB partagent la
 * grille avec la matrice : laissés hauts, ils injectent du courant dans les
 * lignes et faussent l'échantillon. On les coupe tous.
 */
void user_matrix_sinks_off(void)
{
    GPIO_LOW(0, RGB_P0_MASK);
    GPIO_LOW(4, RGB_P4_MASK);
    GPIO_LOW(5, RGB_P5_MASK);
    GPIO_LOW(6, RGB_P6_MASK);
}

/*
 * Instantané des touches enfoncées, par colonne, pour le moteur réactif.
 * Rempli pendant le balayage lui-même : l'effet « goutte d'eau » a besoin de la
 * POSITION d'une frappe, que `kb_process_record` ne reçoit pas -- il ne voit
 * qu'un code de touche.
 */
uint8_t user_matrix_pressed(uint8_t col)
{
    return (col < MATRIX_COLS) ? scan_pressed[col] : 0;
}

/*
 * Lignes 0-2 sur P7.1-P7.3, lignes 3-4 sur P5.3-P5.4, repliées sur les bits 0-4.
 * Les bits 5-7 restent à 1 : aucune touche.
 *
 * Expression identique à celle de l'eyooso-z11 -- mêmes broches de ligne.
 * Le firmware d'usine, lui, décale d'un bit (lignes 1-5) parce qu'il réserve le
 * bit 0 à sa couche Fn ; SMK n'en a pas besoin.
 */
uint8_t user_matrix_read_rows(void)
{
    const uint8_t raw = (uint8_t)(((P7 >> 1) & 0x07) | (P5 & 0x18) | 0xE0);

    if (scan_col < MATRIX_COLS) {
        scan_pressed[scan_col] = (uint8_t)(~raw) & 0x1F;
    }
    return raw;
}
