#include "kbdef.h"
#include "kb.h"
#include "usb.h"

/*
 * Les rapports partent en USB comme sur n'importe quelle carte filaire. Aucune
 * touche n'étant jamais rapportée -- la matrice rend 0xFF -- ces fonctions ne
 * sont en pratique jamais appelées avec du contenu ; elles existent parce que
 * `host.c` les référence.
 */
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

bool kb_process_record(uint16_t keycode, bool key_pressed)
{
    (void)keycode;
    (void)key_pressed;
    return true;
}
