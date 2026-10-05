#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Mode macro du GM610.
 *
 * Activé par Fn + Caps (keycode MACRO_TG). En mode macro, l'appui d'une touche
 * porteuse d'une macro joue sa séquence au lieu de sa fonction normale, et les
 * touches porteuses s'éclairent d'une couleur de surbrillance. Les macros sont
 * configurées depuis le GUI Python et persistées en flash (user_settings).
 *
 * Même découpage que aula_macro.c : l'émission des rapports a lieu UNIQUEMENT en
 * boucle principale (gm610_macro_task), jamais dans le tick ISR —
 * send_keyboard_report() est injoignable depuis une ISR (recouvrement SDCC,
 * vérifié par utils/check_interrupts.py).
 */

/* Reconstruit le bitmap des touches porteuses depuis le blob. À appeler une fois
 * les settings chargés (boucle principale), et après chaque commit HID. */
void gm610_macro_init(void);

/* Bascule le mode macro (depuis kb_process_record, sur MACRO_TG). */
void gm610_macro_toggle(void);
bool gm610_macro_mode(void);

/* Lus par le moteur de rendu LED (ISR) pour la surbrillance. */
bool gm610_macro_has_key(uint8_t row, uint8_t col);
void gm610_macro_hi_color(uint8_t rgb[3]);

/* Depuis kb_process_record : tente de prendre en charge l'évènement en mode
 * macro. Rend true quand l'évènement doit être avalé (déclenchement, lecture en
 * cours, ou relâchement de la touche déclencheuse). */
bool gm610_macro_trigger(uint16_t keycode, bool pressed);

/* Boucle principale : avance la lecture et applique les commits HID. */
void gm610_macro_task(void);

/* Sous-trame LED (ISR, ~420 µs) : ne fait que décrémenter le minuteur. */
void gm610_macro_tick(void);
