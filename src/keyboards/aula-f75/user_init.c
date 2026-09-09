#include "kbdef.h"
#include "user_init.h"
#include "pwm.h"
#include "gpio.h"

/*
 * Initialisation spécifique carte -- NON IMPLÉMENTÉE.
 * Dépend entièrement du brochage. Voir docs/keyboards/aula-f75.md.
 */

void user_gpio_init(void)
{
    /* TODO: direction et pull-ups des broches matrice, RGB et switches */
}

void user_pwm_init(void)
{
    /* TODO: activer les canaux PWM utilisés par le rétroéclairage */
}

void user_init(void)
{
    user_gpio_init();
    user_pwm_init();
}
