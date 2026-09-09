#include "aula_rgb.h"
#include "sh68f90.h"

/*
 * Ordre de chargement des canaux -- relevé dans fcn @ 0x6E61 du firmware
 * d'usine et recoupé avec la carte des registres du datasheet SH68F90 CV2.0
 * (tableau XDATA 0xFF80-0xFFFF).
 *
 * Le firmware d'usine lit 36 octets consécutifs du framebuffer (base XRAM
 * 0x05A2 + colonne * 36) et les écrit dans cet ordre exact, en alternant les
 * deux DPTR via INSCON. Chaque canal reçoit DUTY2 en GROS-BOUTISTE : l'octet
 * de poids fort d'abord.
 *
 *   octets  0-11  ->  PWM20..PWM25   (P1.0..P1.5)
 *   octets 12-23  ->  PWM10..PWM15   (P2.0..P2.5)
 *   octets 24-29  ->  PWM03,04,05    (P3.3..P3.5)
 *   octets 30-35  ->  PWM00,01,02    (P3.0..P3.2)
 *
 * Noter la ROTATION dans le groupe P3 : PWM03-05 sont chargés AVANT PWM00-02.
 * C'est la seule irrégularité de la table, et elle est délibérée côté usine.
 *
 * ⚠️ Ce qui n'est PAS établi : quel canal correspond à quelle (ligne, couleur).
 * On sait dans quel ordre l'usine charge les registres, pas ce que chaque
 * registre allume. Cela demande une observation sur matériel : écrire une
 * seule voie et regarder quelle LED s'allume.
 */

/* Adresses DUTY2 H et L, dans l'ordre de chargement d'usine */
static const AULA_RGB_XDATA uint16_t duty_h_addr[AULA_RGB_CHANNELS] = {
    0xfff4, 0xfff5, 0xfff6, 0xfff7, 0xfff8, 0xfff9, /* PWM20..PWM25 */
    0xffee, 0xffef, 0xfff0, 0xfff1, 0xfff2, 0xfff3, /* PWM10..PWM15 */
    0xffeb, 0xffec, 0xffed,                         /* PWM03,04,05  */
    0xffe8, 0xffe9, 0xffea,                         /* PWM00,01,02  */
};

static const AULA_RGB_XDATA uint16_t duty_l_addr[AULA_RGB_CHANNELS] = {
    0xffdc, 0xffdd, 0xffde, 0xffdf, 0xffe0, 0xffe1,
    0xffd6, 0xffd7, 0xffd8, 0xffd9, 0xffda, 0xffdb,
    0xffd3, 0xffd4, 0xffd5,
    0xffd0, 0xffd1, 0xffd2,
};

static AULA_RGB_XDATA uint16_t duty_cache[AULA_RGB_COLS][AULA_RGB_CHANNELS];

/* Rapport cyclique inversé : 0 = éteint, 255 = pleine intensité.
 * Les LED sont à anode commune, le PWM fait office de sink. */
uint16_t aula_rgb_duty(uint8_t value)
{
    return (uint16_t)(AULA_RGB_PERIOD - ((uint16_t)value * (AULA_RGB_PERIOD / 255u)));
}

void aula_rgb_clear(void)
{
    uint8_t col;
    uint8_t ch;
    const uint16_t off = aula_rgb_duty(0);

    for (col = 0; col < AULA_RGB_COLS; col++) {
        for (ch = 0; ch < AULA_RGB_CHANNELS; ch++) {
            duty_cache[col][ch] = off;
        }
    }
}

void aula_rgb_set(uint8_t row, uint8_t col, uint8_t red, uint8_t green, uint8_t blue)
{
    if (row >= AULA_RGB_ROWS || col >= AULA_RGB_COLS) {
        return;
    }
    duty_cache[col][row * AULA_RGB_COLORS + 0] = aula_rgb_duty(red);
    duty_cache[col][row * AULA_RGB_COLORS + 1] = aula_rgb_duty(green);
    duty_cache[col][row * AULA_RGB_COLORS + 2] = aula_rgb_duty(blue);
}

void aula_rgb_load_column(uint8_t col)
{
    uint8_t ch;

    if (col >= AULA_RGB_COLS) {
        return;
    }

    for (ch = 0; ch < AULA_RGB_CHANNELS; ch++) {
        const uint16_t d = duty_cache[col][ch];
        __xdata uint8_t *const h = (__xdata uint8_t *)duty_h_addr[ch];
        __xdata uint8_t *const l = (__xdata uint8_t *)duty_l_addr[ch];
        *h = (uint8_t)(d >> 8);
        *l = (uint8_t)d;
    }
}
