#include "kbdef.h"
#include "kb.h"
#include "keyboard.h"
#include "report.h"
#include "usb.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef RF_EUART0
#    include "aula_rf.h"
#endif

/*
 * Logique clavier.
 *
 * Le sans-fil passe par l'EUART0 (BK3632), pas par le SPI bit-bangé de l'Air60 :
 * voir `aula_rf.c`. Le sélecteur à glissière est échantillonné par `rf_task()`,
 * qui décide seul du transport ; ici on se contente d'aiguiller les rapports.
 *
 * Pas de switch mode-OS : sa broche n'est pas établie. Restent inconnues P0.0,
 * P0.1, P0.5, P0.6, P4.1, P4.4 et P7.7 — P7.4 et P4.5 sont maintenant
 * identifiées comme le sélecteur de connexion, et P4.7 comme la ligne « module
 * prêt » de la liaison série.
 */

void kb_init(void)
{
#ifdef RF_EUART0
    rf_init();
#endif
}

void kb_update_switches(void)
{
}

void kb_update(void)
{
#ifdef RF_EUART0
    rf_task();
#endif
}

void kb_send_report(__xdata report_keyboard_t *report)
{
#ifdef RF_EUART0
    if (rf_is_wireless()) {
        rf_send_report(report);
        return;
    }
#endif
    usb_send_report(report);
}

void kb_send_nkro(__xdata report_nkro_t *report)
{
#ifdef RF_EUART0
    if (rf_is_wireless()) {
        rf_send_nkro(report);
        return;
    }
#endif
    usb_send_nkro(report);
}

void kb_send_extra(__xdata report_extra_t *report)
{
#ifdef RF_EUART0
    if (rf_is_wireless()) {
        rf_send_extra(report);
        return;
    }
#endif
    usb_send_extra(report);
}
