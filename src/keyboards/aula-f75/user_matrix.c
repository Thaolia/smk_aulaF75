#include "kbdef.h"
#include "gpio.h"
#include "user_matrix.h"

/*
 * Scan de matrice 6 lignes x 15 colonnes.
 *
 * Sélection de colonne ACTIVE BASSE, lignes lues ACTIVES BASSES avec pull-ups.
 * Ordre de scan lu dans la table de saut du firmware d'usine @ 0x72F4.
 */

#define KB_C_P4_MASK (uint8_t)(KB_C12_P4_0 | KB_C13_P4_2 | KB_C14_P4_3)
#define KB_C_P5_MASK (uint8_t)(KB_C8_P5_0 | KB_C9_P5_1 | KB_C10_P5_2 | KB_C11_P5_7)
#define KB_C_P6_MASK (uint8_t)(KB_C0_P6_0 | KB_C1_P6_1 | KB_C2_P6_2 | KB_C3_P6_3 | \
                               KB_C4_P6_4 | KB_C5_P6_5 | KB_C6_P6_6 | KB_C7_P6_7)

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

void user_matrix_col_select(uint8_t col) // actif bas : on tire à 0
{
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
    return (uint8_t)((P7 & 0x0f) | ((P5 << 1) & 0x30) | 0xc0);
}

void user_matrix_sinks_off(void)
{
    GPIO_LOW(1, LED_P1_MASK);
    GPIO_LOW(2, LED_P2_MASK);
    GPIO_LOW(3, LED_P3_MASK);
}
