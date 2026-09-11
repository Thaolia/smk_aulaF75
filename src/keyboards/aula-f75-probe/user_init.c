#include "kbdef.h"
#include "user_init.h"

/*
 * Rien. Aucune broche n'est configurée : elles restent en entrée, comme au
 * reset. C'est la propriété de sûreté de cette carte -- voir kbdef.h.
 */
void user_gpio_init(void) {}

void user_init(void)
{
    user_gpio_init();
}
