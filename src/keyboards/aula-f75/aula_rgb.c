#include "aula_rgb.h"
#include "sh68f90.h"
#include "pwm.h"

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
 * CORRESPONDANCE CANAL <-> (LIGNE, COULEUR) -- établie.
 *
 * Le driver OpenRGB de ce clavier (SinowealthKeyboard10cController::SetLEDsDirect)
 * envoie les couleurs linéairement, 3 octets par LED, à l'offset 0x08 + i*3, où
 * i est l'indice LED de la table de disposition, soit colonne * 6 + ligne.
 * Par colonne, l'hôte émet donc 18 octets dans l'ordre
 * ligne0 R,G,B puis ligne1 R,G,B, etc. -- d'où :
 *
 *     canal = ligne * 3 + couleur
 *
 * Combiné à l'ordre de chargement relevé ci-dessus :
 *
 *     lignes 0-1  ->  P1.0 .. P1.5   (PWM20..PWM25)
 *     lignes 2-3  ->  P2.0 .. P2.5   (PWM10..PWM15)
 *     ligne  4    ->  P3.3 .. P3.5   (PWM03..PWM05)
 *     ligne  5    ->  P3.0 .. P3.2   (PWM00..PWM02)
 *
 * Deux lignes par port, six broches chacun : la régularité du résultat est en
 * elle-même un argument, et elle explique la rotation apparente du groupe P3.
 *
 * Reste une inférence : que le firmware d'usine range les octets reçus dans son
 * framebuffer sans les permuter. C'est l'implémentation naturelle, et le fait
 * que la permutation vive dans la table de registres plutôt que dans les données
 * va dans ce sens. À confirmer sur matériel en n'allumant qu'une voie.
 */

/*
 * Le cache porte des rapports cycliques DÉJÀ convertis, pas des intensités.
 *
 * La conversion coûte une multiplication ; la faire ici, à l'écriture d'un
 * pixel, c'est six conversions par sous-trame (une cellule d'effet), contre
 * dix-huit si on la faisait au chargement de colonne — et le chargement, lui,
 * tourne dans l'ISR.
 */
static AULA_RGB_XDATA uint16_t duty_cache[AULA_RGB_COLS][AULA_RGB_CHANNELS];

/*
 * Rapport cyclique INVERSÉ : 0 = éteint (duty = période), 255 = pleine
 * intensité (duty ≈ 0). Les LED sont à anode commune et le PWM fait office de
 * sink -- c'est l'inverse de la convention du NuPhy Air60, dont le commentaire
 * dit « Do NOT invert » pour son propre câblage.
 *
 * INFÉRÉ, et c'est le seul vrai trou de ce module : l'arithmétique d'inversion
 * du firmware d'usine n'est PAS localisée. Le document a bien la période
 * (0x04B0) et le sens, mais rien ne relie son tampon 8 bits par canal (0x0152)
 * aux 36 octets par colonne que le chargeur lit en 0x05A2, et aucune routine de
 * conversion n'a été isolée.
 *
 * D'où ce choix, qui est le nôtre : `value * 75 >> 4` vaut `value * 4,6875`,
 * soit 1195 au maximum -- 5 de moins que la période, une erreur de 0,4 %. Le
 * produit tient dans 16 bits, et c'est une multiplication suivie d'un décalage.
 *
 * La rédaction précédente écrivait `value * (PERIOD / 255)` : `1200 / 255` vaut
 * QUATRE en division entière, donc l'intensité maximale donnait un rapport de
 * 180 au lieu de 0 -- le blanc plein était inatteignable et toute la plage
 * était comprimée dans [180, 1200].
 */
uint16_t aula_rgb_duty(uint8_t value)
{
    const uint16_t scaled = (uint16_t)(((uint16_t)value * 75u) >> 4);

#if AULA_RGB_DUTY_INVERTED
    return (uint16_t)(AULA_RGB_PERIOD - scaled);
#else
    return scaled;
#endif
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

/*
 * Les dix-huit écritures sont déroulées avec `SET_PWM_DUTY_2` plutôt que
 * parcourues par une table d'adresses. La rédaction précédente gardait deux
 * tables `static const __xdata uint16_t[18]`, soit 72 octets de XRAM plus leur
 * initialisation en flash, pour finir par des écritures via pointeur générique.
 * Les jetons de `pwm.h` donnent des accès directs, et l'ordre reste celui du
 * firmware d'usine -- rotation du groupe P3 comprise.
 */
void aula_rgb_load_column(uint8_t col)
{
    if (col >= AULA_RGB_COLS) {
        return;
    }

    const AULA_RGB_XDATA uint16_t *const d = duty_cache[col];

    SET_PWM_DUTY_2(PWM20, d[0]);  /* ligne 0 R */
    SET_PWM_DUTY_2(PWM21, d[1]);  /* ligne 0 V */
    SET_PWM_DUTY_2(PWM22, d[2]);  /* ligne 0 B */
    SET_PWM_DUTY_2(PWM23, d[3]);  /* ligne 1 R */
    SET_PWM_DUTY_2(PWM24, d[4]);
    SET_PWM_DUTY_2(PWM25, d[5]);
    SET_PWM_DUTY_2(PWM10, d[6]);  /* ligne 2 R */
    SET_PWM_DUTY_2(PWM11, d[7]);
    SET_PWM_DUTY_2(PWM12, d[8]);
    SET_PWM_DUTY_2(PWM13, d[9]);  /* ligne 3 R */
    SET_PWM_DUTY_2(PWM14, d[10]);
    SET_PWM_DUTY_2(PWM15, d[11]);
    SET_PWM_DUTY_2(PWM03, d[12]); /* ligne 4 R -- le groupe P3 est chargé */
    SET_PWM_DUTY_2(PWM04, d[13]); /* dans l'ordre 03,04,05 PUIS 00,01,02, */
    SET_PWM_DUTY_2(PWM05, d[14]); /* seule irrégularité de la table d'usine */
    SET_PWM_DUTY_2(PWM00, d[15]); /* ligne 5 R */
    SET_PWM_DUTY_2(PWM01, d[16]);
    SET_PWM_DUTY_2(PWM02, d[17]);
}
