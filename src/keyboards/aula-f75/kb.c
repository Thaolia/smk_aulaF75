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

extern void indicators_next_effect(void);
extern void indicators_prev_effect(void);
extern void indicators_brightness_up(void);
extern void indicators_brightness_down(void);
extern void indicators_speed_up(void);
extern void indicators_speed_down(void);

/*
 * Les keycodes LNK_BT1..3 ne choisissent que l'emplacement Bluetooth : le
 * transport, lui, vient de la glissière. Appuyer sur l'emplacement déjà actif
 * relance l'appairage, comme le raccourci d'usine qui pose le bit 0x2C.5.
 *
 * Les six keycodes RGB étaient posés dans la keymap depuis le début sans que
 * rien ne les traite : ni ce fichier ni `layout_process_record` ne les
 * reconnaissaient, et ils tombaient dans `send_keycode()` où aucun prédicat ne
 * les prend. Ils appellent maintenant le moteur de rendu.
 */
bool kb_process_record(uint16_t keycode, bool key_pressed)
{
    uint8_t slot;

    switch (keycode) {
        case FX_NEXT:
            if (key_pressed) indicators_next_effect();
            return false;
        case FX_PREV:
            if (key_pressed) indicators_prev_effect();
            return false;
        case BRI_UP:
            if (key_pressed) indicators_brightness_up();
            return false;
        case BRI_DN:
            if (key_pressed) indicators_brightness_down();
            return false;
        case SPD_UP:
            if (key_pressed) indicators_speed_up();
            return false;
        case SPD_DN:
            if (key_pressed) indicators_speed_down();
            return false;

        case LNK_BT1:
            slot = 1;
            break;
        case LNK_BT2:
            slot = 2;
            break;
        case LNK_BT3:
            slot = 3;
            break;
        default:
            return true;
    }

#ifdef RF_EUART0
    if (key_pressed) {
        if (rf_link() == RF_LINK_BT && rf_bt_slot() == slot) {
            rf_request_pairing();
        } else {
            rf_set_bt_slot(slot);
        }
    }
#else
    slot;
    key_pressed;
#endif
    return false;
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
