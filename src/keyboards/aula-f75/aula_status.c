#include "aula_status.h"
#include "sh68f90.h"
#include <stdint.h>

/*
 * Les motifs, un octet d'intensité par pas.
 *
 * UN PAS = UN BOUCLAGE DU BALAYAGE DE RÉGÉNÉRATION, soit ~37,9 ms. Le moteur
 * n'avance pas plus vite, et ce n'est pas un choix de confort : les quatre-
 * vingt-dix cellules sont repeintes UNE PAR SOUS-TRAME, donc changer la valeur
 * ailleurs qu'au bouclage la ferait tomber au milieu d'un cycle de repeinte.
 * Même contrainte que le coeur de la frappe automatique, même solution.
 *
 *   AULA_STATUS_WIRED        1 éclat long           16 pas  0.61 s
 *   AULA_STATUS_24G          2 éclats courts        17 pas  0.64 s
 *   AULA_STATUS_BT           3 éclats courts        28 pas  1.06 s
 *   AULA_STATUS_PAIRING      clignotement rapide     8 pas  0.30 s  (boucle)
 *   AULA_STATUS_LINKED       une respiration        24 pas  0.91 s
 *   AULA_STATUS_LOST         2 éclats longs         32 pas  1.21 s
 *   AULA_STATUS_SLEEP_IN     fondu sortant          21 pas  0.80 s
 *   AULA_STATUS_SLEEP_OUT    fondu entrant          11 pas  0.42 s
 *   AULA_STATUS_LOW_BATT     pulsation lente        40 pas  1.52 s  (boucle)
 *   AULA_STATUS_AZERTY_ON    4 éclats courts        24 pas  0.91 s
 *   AULA_STATUS_AZERTY_OFF   1 éclat court           5 pas  0.19 s
 *   AULA_STATUS_AZERTY       lueur faible fixe       1 pas           (boucle)
 *
 * Tout tient dans un seul tableau plat avec des tables d'offset et de longueur :
 * un tableau de pointeurs `__code` coûterait plus de flash et une indirection de
 * plus à chaque pas, pour rien.
 */
static const __code uint8_t pat_data[227] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   0,   0,
      0,   0,   0, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255,   0,   0,   0,   0,   0, 255, 255, 255, 255,
    255, 255,   0,   0,   0,   0,   0, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255,   0,   0,   0,   0,   0,  12,  30,
     55,  85, 120, 155, 190, 220, 240, 255, 255, 255, 240, 220,
    190, 155, 120,  85,  55,  30,  12,   0,   0, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255,   0,   0,   0,
      0,   0,   0,   0,   0, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 242, 228, 214, 200, 185, 170,
    155, 140, 124, 108,  92,  76,  62,  48,  36,  26,  17,  10,
      4,   0,   0,  20,  50,  90, 135, 180, 215, 240, 255, 255,
      0,   0,   4,  10,  18,  28,  40,  52,  60,  64,  60,  52,
     40,  28,  18,  10,   4,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,
    /* AZERTY_ON : quatre éclats courts, 3 pas allumés / 3 éteints. */
    255, 255, 255,   0,   0,   0, 255, 255, 255,   0,   0,   0,
    255, 255, 255,   0,   0,   0, 255, 255, 255,   0,   0,   0,
    /* AZERTY_OFF : un seul éclat court. */
    255, 255, 255,   0,   0,
    /* AZERTY : un unique pas en boucle, donc une lueur fixe. Assez faible pour
     * ne pas se confondre avec un éclat, assez visible dans le noir. */
     40,
};

static const __code uint8_t pat_off[12] = {0,  16, 33,  61,  69,  93,
                                           125, 146, 157, 197, 221, 226,};
static const __code uint8_t pat_len[12] = {16, 17, 28, 8, 24, 32, 21, 11, 40, 24, 5, 1,};

/* Bit N à 1 : le motif N boucle au lieu de se terminer. Douze motifs, donc
 * douze bits : le masque ne tient PAS dans un octet. */
#define PAT_LOOPING 0x0908u

#define PAT_LOOPS(p) ((PAT_LOOPING & (uint16_t)(1u << (p))) != 0u)

static __xdata uint8_t st_pat = AULA_STATUS_NONE;
static __xdata uint8_t st_pos;

/* Fronts, pour `aula_status_poll()`. `0xFF` force l'annonce du transport au
 * tout premier passage : le clavier dit sur quoi il démarre. */
static __xdata uint8_t prev_link = 0xFF;
static __bit           prev_conn;
static __bit           prev_low;
static __bit           low_batt;
static __bit           az_rest;

void aula_status_event(uint8_t event)
{
    /* `st_pat` et `st_pos` sont lus par l'ISR à chaque sous-trame : les écrire
     * en deux temps montrerait un pas pris dans l'autre motif. */
    ET2    = 0;
    st_pat = event;
    st_pos = 0;
    ET2    = 1;
}

/* Ce qui reste à l'écran quand aucun motif ponctuel ne joue. La priorité est
 * documentée dans `aula_status.h` : deux états de repos sur une LED blanche
 * unique ne se distinguent pas, il faut donc en choisir un. */
static void status_rest(void)
{
    if (low_batt) {
        st_pat = AULA_STATUS_LOW_BATT;
    } else if (az_rest) {
        st_pat = AULA_STATUS_AZERTY;
    } else {
        st_pat = AULA_STATUS_NONE;
    }
    st_pos = 0;
}

void aula_status_set_azerty(bool on)
{
    az_rest = on ? 1 : 0;
    if (!aula_status_busy()) {
        /* Le motif de basculement est posté juste après par `aula_layout.c` ; ce
         * repos est celui qu'il trouvera en finissant. */
        ET2 = 0;
        status_rest();
        ET2 = 1;
    }
}

bool aula_status_lit(void)
{
    return st_pat != AULA_STATUS_NONE;
}

bool aula_status_busy(void)
{
    const uint8_t p = st_pat;

    if (p == AULA_STATUS_NONE) {
        return false;
    }
    return !PAT_LOOPS(p);
}

void aula_status_tick(void)
{
    const uint8_t p = st_pat;

    if (p == AULA_STATUS_NONE) {
        return;
    }
    if (++st_pos < pat_len[p]) {
        return;
    }
    st_pos = 0;
    if (!PAT_LOOPS(p)) {
        status_rest();
    }
}

uint8_t aula_status_level(void)
{
    const uint8_t p = st_pat;

    if (p == AULA_STATUS_NONE) {
        return 0;
    }
    return pat_data[(uint8_t)(pat_off[p] + st_pos)];
}

void aula_status_poll(uint8_t link, bool connected, bool low)
{
    /* La batterie faible ne déclenche pas de motif ponctuel : c'est un ÉTAT, il
     * tient le voyant au repos et cède la place à tout évènement. */
    if (low != prev_low) {
        prev_low = low;
        low_batt = low;
        if (!aula_status_busy()) {
            status_rest();
        }
    }

    if (link != prev_link) {
        prev_link = link;
        /* `rf_link()` rend 0 filaire, 1 la 2,4 GHz, 2 le Bluetooth -- le même
         * ordre que les trois premiers motifs, à dessein. */
        aula_status_event((link <= AULA_STATUS_BT) ? link : (uint8_t)AULA_STATUS_WIRED);
        prev_conn = connected;
        return;
    }

    if (connected != prev_conn) {
        prev_conn = connected;
        aula_status_event(connected ? (uint8_t)AULA_STATUS_LINKED
                                    : (uint8_t)AULA_STATUS_LOST);
    }
}
