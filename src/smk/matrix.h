#pragma once

#include <stdint.h>
#include <stdbool.h>

// Posé à true par l'ISR après chaque scan, reposé à false par matrix_task().
// Exposé pour que la boucle principale puisse prioriser les frappes.
extern volatile bool matrix_updated;

void    matrix_init();
uint8_t matrix_task();

void matrix_scan_full();
