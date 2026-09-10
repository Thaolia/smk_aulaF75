#include "kbdef.h"
#include "user_init.h"
#include "pwm.h"
#include "user_matrix.h"
#include "aula_rgb.h"
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

/*
 * Les 18 canaux du rétroéclairage : trois bancs de six, période et DUTY1
 * communs. La période 0x04B0 et les dix-huit constantes DUTY1 sont celles du
 * firmware d'usine, relevées dans sa routine 0x6707-0x68D3.
 *
 * Les bancs ne sont PAS activés ici : c'est `indicators_pwm_enable()` qui pose
 * `PWM_MODE_ENABLE`, et `indicators_pwm_disable()` qui le retire avant chaque
 * balayage de matrice. Allumer ici laisserait les LED conduire pendant le
 * premier scan, avant que le moteur de rendu n'ait écrit quoi que ce soit.
 */
void user_pwm_init(void)
{
    const uint8_t perdh = (uint8_t)(AULA_RGB_PERIOD >> 8);
    const uint8_t perdl = (uint8_t)(AULA_RGB_PERIOD);

    PWM0PERDH = perdh;
    PWM0PERDL = perdl;
    PWM1PERDH = perdh;
    PWM1PERDL = perdl;
    PWM2PERDH = perdh;
    PWM2PERDL = perdl;

    /*
     * DUTY1 = la phase du canal, écrite ici et JAMAIS retouchée ensuite --
     * c'est très exactement ce que fait le firmware d'usine en 0x6713-0x68CD,
     * et les valeurs ci-dessous sont les siennes, relevées une à une.
     * DUTY2 part sur la même valeur : impulsion nulle, panneau noir.
     */
    SET_PWM_DUTY(PWM00, AULA_RGB_PHASE(15), AULA_RGB_PHASE(15));
    SET_PWM_DUTY(PWM01, AULA_RGB_PHASE(16), AULA_RGB_PHASE(16));
    SET_PWM_DUTY(PWM02, AULA_RGB_PHASE(17), AULA_RGB_PHASE(17));
    SET_PWM_DUTY(PWM03, AULA_RGB_PHASE(12), AULA_RGB_PHASE(12));
    SET_PWM_DUTY(PWM04, AULA_RGB_PHASE(13), AULA_RGB_PHASE(13));
    SET_PWM_DUTY(PWM05, AULA_RGB_PHASE(14), AULA_RGB_PHASE(14));
    SET_PWM_DUTY(PWM10, AULA_RGB_PHASE(6), AULA_RGB_PHASE(6));
    SET_PWM_DUTY(PWM11, AULA_RGB_PHASE(7), AULA_RGB_PHASE(7));
    SET_PWM_DUTY(PWM12, AULA_RGB_PHASE(8), AULA_RGB_PHASE(8));
    SET_PWM_DUTY(PWM13, AULA_RGB_PHASE(9), AULA_RGB_PHASE(9));
    SET_PWM_DUTY(PWM14, AULA_RGB_PHASE(10), AULA_RGB_PHASE(10));
    SET_PWM_DUTY(PWM15, AULA_RGB_PHASE(11), AULA_RGB_PHASE(11));
    SET_PWM_DUTY(PWM20, AULA_RGB_PHASE(0), AULA_RGB_PHASE(0));
    SET_PWM_DUTY(PWM21, AULA_RGB_PHASE(1), AULA_RGB_PHASE(1));
    SET_PWM_DUTY(PWM22, AULA_RGB_PHASE(2), AULA_RGB_PHASE(2));
    SET_PWM_DUTY(PWM23, AULA_RGB_PHASE(3), AULA_RGB_PHASE(3));
    SET_PWM_DUTY(PWM24, AULA_RGB_PHASE(4), AULA_RGB_PHASE(4));
    SET_PWM_DUTY(PWM25, AULA_RGB_PHASE(5), AULA_RGB_PHASE(5));

    aula_rgb_clear();
}

void user_init(void)
{
    user_gpio_init();
    user_pwm_init();
}
