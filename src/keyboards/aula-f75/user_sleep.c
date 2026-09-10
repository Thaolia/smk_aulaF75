#include "kbdef.h"
#include "user_sleep.h"
#include "user_init.h"
#include "user_matrix.h"
#include "gpio.h"
#include "extint.h"

#ifdef RF_EUART0
#    include "aula_rf.h"
#endif

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

/*
 * Le mode suit le sélecteur en direct, comme chez le NuPhy Air60 :
 * `sleep_task()` interroge ce crochet à CHAQUE passage.
 *
 * La distinction n'est pas cosmétique. En `USER_SLEEP_USB`, l'endormissement est
 * décidé par la suspension du bus ; en `USER_SLEEP_RF`, il l'est par le
 * compteur d'inactivité de `sleep.c` — et hors mode RF ce compteur est remis à
 * zéro à chaque passage, donc il ne sert à rien. Rendre `USB` en dur, comme le
 * faisait ce fichier, revenait à n'avoir aucune veille sur batterie.
 */
user_sleep_mode_t user_sleep_supported(void)
{
#ifdef RF_EUART0
    return rf_is_wireless() ? USER_SLEEP_RF : USER_SLEEP_USB;
#else
    return USER_SLEEP_USB;
#endif
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
#ifdef RF_EUART0
    rf_sleep_prepare(); /* vidange l'émission puis coupe l'EUART0, comme 0x7D74 */
#endif
    park_panel();
    extint_wake_arm(); /* IENC=0xF3, EXF0=0x40, EX4=1 -- mêmes valeurs qu'en usine */
}

void user_sleep_wake(void)
{
    extint_wake_disable();
    user_gpio_init();
#ifdef RF_EUART0
    /*
     * APRÈS `user_gpio_init()`, et pas avant : c'est lui qui rend P7.4 et P4.5
     * au sélecteur (P4CR=0x4D, P7CR=0x60 les laissent en entrée avec pull-up),
     * broches que `park_panel()` tenait en sortie basse pendant le sommeil --
     * comme le firmware d'usine en 0x7DCB-0x7DF1.
     */
    rf_sleep_wake();
#endif
}

#endif // SLEEP_ENABLE
