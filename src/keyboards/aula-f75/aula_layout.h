#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Mode « compensation AZERTY » de l'AULA F75. Persisté en flash : le clavier
 * repart dans le mode où on l'a laissé.
 *
 * Le clavier est un 75 % ANSI dont les capuchons portent la disposition US
 * International. Quand l'hôte est réglé en AZERTY français (Windows fr-FR), les
 * deux se contredisent : la touche marquée `A` écrit `q`, la touche `1` écrit
 * `&`. Ce mode fait porter la traduction par le CLAVIER : on frappe ce qui est
 * écrit sur les capuchons, et le bon caractère sort, sans rien changer côté PC.
 *
 * Trois des cinq touches mortes US International passent par celle de l'hôte
 * (`^`, `` ` ``, `~`) : le clavier l'émet et c'est l'hôte qui compose.
 *
 * Deux demandent une machine à états. L'accent aigu parce que le fr-FR n'en a
 * pas -- `é` y est une touche à part entière. Le TRÉMA parce que l'hôte en a une,
 * mais que son symbole seul est `¨` et non `"` : mesuré sur l'appareil, c'est la
 * seule des quatre dont le caractère nu diffère de celui d'US International.
 *
 * Le découpage tick/task est le même que pour `aula_encoder.c` et
 * `aula_macro.c`, et pour la même raison : `send_keyboard_report()` traverse la
 * pile de rapports, injoignable depuis une ISR sans collision de recouvrement
 * SDCC -- ce que `utils/check_interrupts.py` casse le build pour empêcher.
 */

/* Depuis `kb_process_record` : bascule le mode (touche LAYOUT_AZ enfoncée). */
void aula_layout_toggle(void);

/*
 * Depuis `indicators_validate_settings()`, juste après le chargement de la NVM.
 * PAS depuis `kb_init()` : celui-ci tourne AVANT `restore_settings()` dans
 * `main()`, il n'y aurait rien à lire.
 */
void aula_layout_restore(bool on);

/*
 * Depuis `kb_process_record`, pour TOUT évènement de touche. Rend true quand
 * l'évènement a été pris en charge et doit être avalé.
 */
bool aula_layout_intercept(uint16_t qcode, bool pressed);

/*
 * `€` sur `Fn + E`. Visé pour un hôte FRANÇAIS, où c'est AltGr+E -- donc
 * indépendant du mode, qui n'a pas à être allumé pour que ça marche.
 */
void aula_layout_euro(void);

/* Depuis la sous-trame LED (ISR, ~420 µs) : fait courir les minuteurs. */
void aula_layout_tick(void);

/* Depuis la boucle principale : vide la file d'émission. */
void aula_layout_task(void);
