#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Voyant d'état de l'AULA F75 — la LED blanche entre Échap et F1.
 *
 * Ce n'est pas une LED de touche. Elle est câblée sur les canaux VERT et BLEU de
 * la position de grille `[ligne 0][colonne 14]`, celle de l'appui molette, à
 * l'autre bout du clavier ; le canal ROUGE de cette même position pilote le
 * voyant de Verr. Maj. Mesuré sur l'appareil, voir `indicators.c`.
 *
 * Les deux canaux allument la MÊME LED blanche et leurs intensités s'ajoutent :
 * il n'y a donc ni couleur ni second voyant à exploiter. Les motifs se
 * distinguent uniquement par leur RYTHME -- d'où un, deux ou trois éclats pour
 * les trois transports, qui se comptent d'un coup d'oeil.
 *
 * Découpage ISR / boucle principale identique à `aula_encoder.c` et
 * `aula_macro.c` : le moteur avance dans l'ISR, les évènements sont postés par
 * la boucle principale.
 */

#define AULA_STATUS_WIRED     0 /* passage en filaire        : 1 éclat long   */
#define AULA_STATUS_24G       1 /* passage en 2,4 GHz        : 2 éclats       */
#define AULA_STATUS_BT        2 /* passage en Bluetooth      : 3 éclats       */
#define AULA_STATUS_PAIRING   3 /* appairage en cours        : clignotement   */
#define AULA_STATUS_LINKED    4 /* liaison établie           : une respiration*/
#define AULA_STATUS_LOST      5 /* liaison perdue            : 2 éclats longs */
#define AULA_STATUS_SLEEP_IN  6 /* entrée en veille          : fondu sortant  */
#define AULA_STATUS_SLEEP_OUT 7 /* sortie de veille          : fondu entrant  */
#define AULA_STATUS_LOW_BATT  8 /* batterie faible           : pulsation lente*/
#define AULA_STATUS_NONE      0xFF

/* Depuis la boucle principale : joue un motif, en écrasant celui en cours. */
void aula_status_event(uint8_t event);

/*
 * Depuis `rf_task()` : détecte les transitions de transport, de connexion et de
 * batterie, et poste l'évènement correspondant. Toute la logique de front vit
 * ici pour que `aula_rf.c` n'ait qu'une ligne à appeler.
 */
void aula_status_poll(uint8_t link, bool connected, bool low_batt);

/* Vrai tant qu'un motif PONCTUEL joue. Les motifs en boucle -- appairage,
 * batterie faible -- ne comptent pas : ils ne finissent jamais. */
bool aula_status_busy(void);

/* Depuis l'ISR, au bouclage du balayage de régénération (~37,9 ms). */
void aula_status_tick(void);

/* Depuis l'ISR : intensité à écrire sur les canaux vert et bleu. */
uint8_t aula_status_level(void);
