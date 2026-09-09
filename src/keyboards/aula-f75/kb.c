#include "kbdef.h"
#include "keyboard.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Logique clavier -- minimale.
 *
 * Pas de gestion de switch mode-connexion / mode-OS : l'AULA F75 en a
 * peut-être, mais leurs broches ne sont pas établies. Pas de liaison sans fil
 * non plus (le transport est EUART0, pas le SPI bit-bangé de l'Air60).
 */

void kb_init(void)
{
}

void kb_update_switches(void)
{
}
