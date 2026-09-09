#include "kbdef.h"
#include "user_sleep.h"
#include "user_init.h"
#include "user_matrix.h"
#include "gpio.h"
#include "extint.h"

#ifdef SLEEP_ENABLE

/*
 * Veille -- transcrite de la séquence du firmware d'usine.
 *
 *   fcn @ 0x006E  parking du panneau (lignes en entrée + pull-up, colonnes basses)
 *   0x7DCB-0x7DF1 parking des broches hors matrice
 *   0x7DF4        EXF1 = 0            (extint_wake_clear)
 *   0x7DF6        IENC = 0xF3         (identique à platform/sh68f90/extint.c)
 *   0x7DF9        EXF0 = 0x40         (idem)
 *   0x7DFC        IEN0 |= 0x02        (EX4 = 1)
 *   0x7E34        PCON |= 0x02        (power-down)
 *
 * Mécanisme de réveil : toutes les colonnes sont tenues BASSES et les lignes
 * sont en entrée avec pull-up. Un appui quelconque tire une ligne au 0, ce qui
 * déclenche INT4.
 *
 * L'ordre des écritures est celui du firmware d'usine et doit le rester : la
 * mise à l'état bas précède le passage en sortie.
 *
 * ⚠️ Non vérifié sur matériel. Un parking faux, c'est un clavier qui ne se
 * réveille pas et qu'il faut reflasher.
 */

user_sleep_mode_t user_sleep_supported(void)
{
    /* Pas de sans-fil sur ce portage : la veille n'a de sens qu'en USB. */
    return USER_SLEEP_USB;
}

static void park_panel(void)
{
    /* Lignes : entrée + pull-up (fcn @ 0x006E, 0x006E-0x0091) */
    GPIO_INPUT(7, KB_R_P7_MASK);
    GPIO_PULLUP_ON(7, KB_R_P7_MASK);
    GPIO_INPUT(5, KB_R_P5_MASK);
    GPIO_PULLUP_ON(5, KB_R_P5_MASK);

    /* Colonnes et broches annexes : niveau bas AVANT de passer en sortie
     * (0x0092-0x00B9), puis direction sortie (0x00BC et suivants). */
    GPIO_LOW(6, KB_C_P6_MASK);
    GPIO_LOW(5, KB_C_P5_MASK);
    GPIO_LOW(4, (uint8_t)(KB_C_P4_MASK | _P4_5 | _P4_6));
    GPIO_LOW(7, (uint8_t)_P7_4);
    GPIO_LOW(0, (uint8_t)(_P0_5 | _P0_6 | _P0_7));

    GPIO_OUTPUT(6, KB_C_P6_MASK);
    GPIO_OUTPUT(5, KB_C_P5_MASK);
    GPIO_OUTPUT(4, (uint8_t)(KB_C_P4_MASK | _P4_5 | _P4_6));

    /* Broches hors matrice (0x7DCB-0x7DF1) */
    GPIO_PULLUP_OFF(0, (uint8_t)(_P0_0 | _P0_1));
    GPIO_OUTPUT(0, (uint8_t)(_P0_0 | _P0_1));
    GPIO_LOW(0, (uint8_t)(_P0_0 | _P0_1));

    GPIO_PULLUP_OFF(0, (uint8_t)(_P0_5 | _P0_6)); /* phases d'encodeur */
    GPIO_OUTPUT(0, (uint8_t)(_P0_5 | _P0_6));
    GPIO_LOW(0, (uint8_t)(_P0_5 | _P0_6));

    GPIO_PULLUP_OFF(7, (uint8_t)(_P7_4 | _P7_7));
    GPIO_OUTPUT(7, (uint8_t)(_P7_4 | _P7_7));
    GPIO_LOW(7, (uint8_t)(_P7_4 | _P7_7));

    GPIO_PULLUP_OFF(4, (uint8_t)_P4_5);
    GPIO_OUTPUT(4, (uint8_t)_P4_5);
    GPIO_LOW(4, (uint8_t)_P4_5);

    P0_7 = 1;
    P7_6 = 0;
}

void user_sleep_prepare(void)
{
    park_panel();
    extint_wake_arm(); /* IENC=0xF3, EXF0=0x40, EX4=1 -- mêmes valeurs qu'en usine */
}

void user_sleep_wake(void)
{
    extint_wake_disable();
    user_gpio_init();
}

#endif // SLEEP_ENABLE
