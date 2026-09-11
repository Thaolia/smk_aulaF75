#pragma once

#include <stdbool.h>

/*
 * Mode « frappe automatique » de l'AULA F75.
 *
 * Tape une lettre au hasard, l'efface aussitôt d'un retour arrière, et
 * recommence toutes les 1 s ± 500 ms. Le texte de l'hôte reste donc inchangé,
 * mais la session le voit comme active. Rien de tel dans le firmware d'usine.
 *
 * Le découpage en deux fonctions n'est pas du style, c'est la même contrainte
 * que pour `aula_encoder.c` : `send_keyboard_report()` traverse la pile de
 * rapports, injoignable depuis une ISR sans collision de recouvrement SDCC --
 * ce que `utils/check_interrupts.py` casse le build pour empêcher.
 */

/* Depuis `kb_process_record` : arme le mode (touche MACRO_TG enfoncée). */
void aula_macro_arm(void);

/*
 * Depuis `kb_process_record`, pour TOUT évènement de touche. Rend true quand
 * l'évènement doit être avalé -- c'est ce qui rend muette la touche qui arrête
 * le mode, et ce qui empêche le relâchement de la combinaison d'activation de
 * l'arrêter aussitôt.
 */
bool aula_macro_intercept(bool pressed);

/* Depuis l'ISR (rendu LED) : le mode tape-t-il en ce moment ? */
bool aula_macro_active(void);

/* Depuis la sous-trame LED (ISR, ~420 µs) : fait courir le minuteur. */
void aula_macro_tick(void);

/* Depuis la boucle principale : émet les rapports et surveille l'annulation. */
void aula_macro_task(void);
