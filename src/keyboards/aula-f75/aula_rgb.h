#pragma once

#include <stdint.h>

#ifdef __SDCC
#    define AULA_RGB_XDATA __xdata
#else
#    define AULA_RGB_XDATA
#endif

/*
 * Matrice LED de l'AULA F75.
 *
 * Topologie (inverse de celle du NuPhy Air60) : les 15 colonnes de matrice
 * servent de sélecteurs de multiplexage, et les 18 sorties PWM sont les
 * 6 lignes de LED x R/G/B. À chaque avance de colonne, les 18 rapports
 * cycliques doivent être rechargés.
 */

#define AULA_RGB_ROWS     6
#define AULA_RGB_COLORS   3
#define AULA_RGB_COLS     15
#define AULA_RGB_CHANNELS (AULA_RGB_ROWS * AULA_RGB_COLORS) /* 18 */

/* Période PWM du firmware d'usine : PWM0PERDH=0x04 @ 0x6707, PWM0PERDL=0xB0 @ 0x670D */
#define AULA_RGB_PERIOD 0x04B0u

void     aula_rgb_clear(void);
void     aula_rgb_set(uint8_t row, uint8_t col, uint8_t red, uint8_t green, uint8_t blue);
uint16_t aula_rgb_duty(uint8_t value);

/* Charge les 18 rapports cycliques de la colonne dans les registres PWM,
 * dans l'ordre relevé dans le firmware d'usine. */
void aula_rgb_load_column(uint8_t col);
