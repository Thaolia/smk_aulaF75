#include "stack.h"

#if DEBUG == 1

#    include "sh68f90.h"
#    include "debug.h"
#    include <stdint.h>

#    define STACK_SENTINEL 0xAA
extern uint8_t _start__stack;
#    define STACK_BASE ((uint8_t)((uint16_t)&_start__stack - 1u))
#    define STACK_TOP  0xFF // top of the SH68F90's 256-byte internal RAM

// Fill the unused stack region (above the current SP, up to STACK_TOP) with
// a sentinel. Must run early in main(), while SP is still shallow, so we
// capture the largest possible window for later peak measurement.
void stack_paint(void)
{
    uint8_t addr = SP;
    do {
        addr++;
        *((__idata uint8_t *)addr) = STACK_SENTINEL;
    } while (addr != STACK_TOP);
}

static uint8_t stack_peak(void)
{
    uint8_t addr = STACK_TOP;
    while (addr > STACK_BASE && *((__idata uint8_t *)addr) == STACK_SENTINEL) {
        addr--;
    }
    return addr - STACK_BASE;
}

void stack_task(void)
{
    static uint8_t reported = 0;
    static uint8_t throttle = 0;
    static uint8_t period   = 0;

    if (++throttle != 0) {
        return; // ~once every 256 main-loop iterations
    }

    uint8_t peak = stack_peak();

    /*
     * Repeter periodiquement, pas seulement sur un nouveau maximum.
     *
     * Le pic est atteint dans les toutes premieres millisecondes -- l'interruption USB
     * tire sur chaque SOF, toutes les 1 ms (_SOFIA dans USBIE1) -- donc l'unique message
     * « nouveau maximum » part bien avant qu'un hote ait pu s'attacher a la console. Le
     * tampon de console.c fait 128 octets et jette quand il est plein : la ligne etait
     * donc systematiquement perdue, et le chiffre inaccessible depuis l'hote. Mesure du
     * 2026-09-13 : 130 s d'ecoute, aucun STACK peak.
     *
     * ++period deborde tous les 256 declenchements du throttle, soit ~65 536 tours de
     * boucle : de l'ordre de 30 s. Le pic etant un watermark, il ne decroit jamais, donc
     * reaffecter `reported` ici ne peut pas creer un faux « nouveau maximum ».
     */
    if (peak > reported || ++period == 0) {
        reported = peak;
        dprintf("STACK peak: %02x\r\n", peak);
    }
}

#endif // DEBUG
