#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Molette rotative de l'AULA F75.
 *
 * L'APPUI est une position de matrice ordinaire -- `[ligne 0][colonne 14]`,
 * câblée sur « muet » dans la disposition. La ROTATION, elle, est hors matrice :
 * deux phases en quadrature sur `P0.5` et `P0.6`.
 *
 * Le découpage en deux fonctions n'est pas du style. `aula_encoder_sample()`
 * vit dans l'ISR Timer2 et `aula_encoder_task()` dans la boucle principale :
 * `host_consumer_send()` traverse la pile de rapports, qui n'est pas joignable
 * depuis une ISR sans provoquer une collision de recouvrement SDCC -- ce que
 * `utils/check_interrupts.py` casse le build pour empêcher.
 */

/* Depuis la sous-trame LED (ISR, ~400 µs) : échantillonne et accumule. */
void aula_encoder_sample(void);

/* Depuis la boucle principale : émet les rapports consumer en attente. */
void aula_encoder_task(void);

/*
 * Témoin de rotation, lu depuis l'ISR de rendu. Le clavier bat comme un coeur
 * pendant ~1,5 s après la dernière détente, et la cadence suit le sens : monter
 * accélère, descendre ralentit. Le niveau est RELATIF -- rien dans le protocole
 * USB ni radio ne dit au clavier où en est le volume de l'hôte.
 */
bool aula_encoder_beating(void);

/* Pas de motif par bouclage de balayage, en 1/256e. 256 = cadence du coeur de
 * la frappe automatique. */
uint16_t aula_encoder_beat_rate(void);

/* Le cran courant, 0 (le plus bas) à 7. Le rendu en tire la teinte, pour que la
 * COULEUR dise le niveau pendant que la CADENCE dit le sens. */
uint8_t aula_encoder_beat_level(void);
