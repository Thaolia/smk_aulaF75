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

/*
 * POLARITÉ ET DÉCALAGE DE PHASE -- ÉTABLIS SUR LE DUMP, plus rien d'inféré ici.
 *
 * Le firmware d'usine écrit DUTY1 UNE SEULE FOIS, à l'initialisation
 * (0x6713-0x68CD), et ne le retouche jamais : sur les 36 références aux
 * registres DUTY1 de l'image entière, les 36 sont dans cette routine, et zéro
 * ailleurs. Seul DUTY2 est modulé, par le chargeur de colonne 0x6E61.
 *
 * Et DUTY1 ne vaut ni 0 ni la période : c'est une constante PAR CANAL, prise
 * dans CODE 0x2922 (18 octets, `0xB4 + indice`, indexés `ligne * 3 + couleur`).
 * Les 18 valeurs relevées à l'init sont exactement cette table, permutée par
 * l'ordre de chargement de 0x6E61 -- les deux relevés se recoupent octet à
 * octet, ce qui verrouille aussi la correspondance canal <-> (ligne, couleur).
 *
 * Le convertisseur 0x76C3 donne l'arithmétique en clair :
 *
 *     DUTY2 = (valeur << 2) + phase[ligne * 3 + couleur]
 *
 * (`MOV B,#4 / MUL AB` puis addition 16 bits de l'octet de table), rangée en
 * GROS-BOUTISTE dans le tampon 0x05A2 + colonne * 36 que le chargeur recopie.
 *
 * D'où le SENS, par monotonicité -- le datasheet ne dit pas laquelle des deux
 * arêtes DUTY1/DUTY2 ouvre l'impulsion, et `pwm.h` non plus, mais on n'en a pas
 * besoin. DUTY1 est figé et DUTY2 croît avec la valeur ; sous la lecture
 * opposée, la valeur 1 donnerait une impulsion de 1196 crans et la valeur 255
 * une de 180, soit une luminosité DÉCROISSANTE avec la valeur, discontinue en
 * zéro. Absurde. Donc l'impulsion va bien de DUTY1 à DUTY2, sa largeur vaut
 * `valeur << 2`, le décalage de phase disparaît du résultat lumineux, et le
 * rapport cyclique est DIRECT -- 0 = éteint (DUTY2 = DUTY1, impulsion nulle),
 * 255 = 1020/1200 soit 85 % de la période.
 *
 * La rédaction précédente posait `AULA_RGB_DUTY_INVERTED 1` avec DUTY1 = période
 * et `duty = période - valeur * 4,6875`. C'était l'exact contraire : le panneau
 * aurait été allumé à fond au repos et noir à luminosité maximale.
 *
 * Les 18 phases sont décalées d'un cran chacune ; l'usine étale ainsi les
 * fronts montants des 18 canaux au lieu de les faire coïncider.
 */
#define AULA_RGB_PHASE_BASE 0xB4u
#define AULA_RGB_PHASE(channel) (uint16_t)(AULA_RGB_PHASE_BASE + (channel))

/* Roue de teintes d'usine (CODE 0x2B2A), 192 entrées. */
#define AULA_RGB_WHEEL_SIZE 192
void aula_rgb_wheel(uint8_t index, uint8_t out[3]);

/* Éteint tout le framebuffer. */
void     aula_rgb_clear(void);
void     aula_rgb_set(uint8_t row, uint8_t col, uint8_t red, uint8_t green, uint8_t blue);
uint16_t aula_rgb_duty(uint8_t channel, uint8_t value);

/* Charge les 18 rapports cycliques de la colonne dans les registres PWM,
 * dans l'ordre relevé dans le firmware d'usine. */
void aula_rgb_load_column(uint8_t col);
