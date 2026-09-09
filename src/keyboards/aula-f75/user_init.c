#include "kbdef.h"
#include "user_init.h"
#include "pwm.h"
#include "user_matrix.h"
#include "gpio.h"

/*
 * Initialisation GPIO transcrite de l'init consolidée du firmware d'usine,
 * fcn @ 0xA7EC. Valeurs relevées telles quelles :
 *
 *   P0CR=0x9C  P1CR=0x3F  P2CR=0x3F  P3CR=0x3F
 *   P4CR=0x4D  P5CR=0x87  P6CR=0xFF  P7CR=0x60
 *   P0PCR=0xF8 P1PCR=0x3F P2PCR=0x3F P3PCR=0x3F
 *   P4PCR=0x6F P5PCR=0x9F P6PCR=0xFF P7PCR=0xDF
 *   P0=0x1C
 *
 * Bit CR à 1 = sortie. On reprend la configuration d'usine en bloc plutôt que
 * de la reconstruire broche par broche : plusieurs broches ont un rôle encore
 * inconnu (P4.1, P4.4, P4.6, P4.7, P5.5, P5.6, P7.7) et les laisser dans l'état
 * d'usine est plus sûr que de deviner.
 */

void user_gpio_init(void)
{
    GPIO_DIR_WRITE(0, 0x9c);
    GPIO_DIR_WRITE(1, 0x3f);
    GPIO_DIR_WRITE(2, 0x3f);
    GPIO_DIR_WRITE(3, 0x3f);
    GPIO_DIR_WRITE(4, 0x4d);
    GPIO_DIR_WRITE(5, 0x87);
    GPIO_DIR_WRITE(6, 0xff);
    GPIO_DIR_WRITE(7, 0x60);

    GPIO_PULLUP_WRITE(0, 0xf8);
    GPIO_PULLUP_WRITE(1, 0x3f);
    GPIO_PULLUP_WRITE(2, 0x3f);
    GPIO_PULLUP_WRITE(3, 0x3f);
    GPIO_PULLUP_WRITE(4, 0x6f);
    GPIO_PULLUP_WRITE(5, 0x9f);
    GPIO_PULLUP_WRITE(6, 0xff);
    GPIO_PULLUP_WRITE(7, 0xdf);

    GPIO_WRITE(0, 0x1c);

    /* Colonnes désélectionnées (actif bas) avant le premier scan */
    user_matrix_cols_deselect_all();
}

void user_pwm_init(void)
{
    /* Rien : ce portage n'a pas de rendu RGB. Laisser les 18 canaux PWM au
     * repos plutôt que de les activer sans savoir quoi y écrire. */
}

void user_init(void)
{
    user_gpio_init();
    user_pwm_init();
}
