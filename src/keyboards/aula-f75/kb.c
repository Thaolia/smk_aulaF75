#include "kbdef.h"
#include "kb.h"
#include "keyboard.h"
#include "report.h"
#include "usb.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Logique clavier -- minimale.
 *
 * Pas de switch mode-connexion / mode-OS : l'AULA F75 en a peut-être, mais
 * leurs broches ne sont pas établies (l'init GPIO @ 0xA7EC laisse P0.0, P0.1,
 * P0.5, P0.6, P4.1, P4.4, P4.5, P4.7, P5.5, P5.6, P7.4 et P7.7 en entrée sans
 * qu'on sache à quoi ils servent).
 *
 * Pas de sans-fil : le transport est EUART0, pas le SPI bit-bangé de l'Air60.
 * Toutes les remontées passent donc par l'USB.
 */

void kb_init(void)
{
}

void kb_update_switches(void)
{
}

void kb_send_report(__xdata report_keyboard_t *report)
{
    usb_send_report(report);
}

void kb_send_nkro(__xdata report_nkro_t *report)
{
    usb_send_nkro(report);
}

void kb_send_extra(__xdata report_extra_t *report)
{
    usb_send_extra(report);
}
