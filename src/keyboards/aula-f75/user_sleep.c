#include "kbdef.h"
#include "user_sleep.h"

/*
 * Veille -- désactivée volontairement.
 *
 * La mise en veille touche à la configuration des broches de réveil, qui
 * dépend du brochage non établi. Déclarer USER_SLEEP_NONE est le choix sûr :
 * le clavier consommera plus, mais ne se réveillera pas de travers.
 */

user_sleep_mode_t user_sleep_supported(void)
{
    return USER_SLEEP_NONE;
}

void user_sleep_prepare(void) {}
void user_sleep_wake(void) {}
