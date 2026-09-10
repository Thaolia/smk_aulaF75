#pragma once

#include <stdint.h>

/*
 * Données et primitives des moteurs de rendu du firmware d'usine.
 *
 * Ce fichier ne porte que ce qui a été RELEVÉ dans `assets/f75_full_jtag.bin` :
 * quatre tables recopiées telles quelles, plus le générateur pseudo-aléatoire
 * dont le rôle -- et non le polynôme -- est transcrit. L'ordonnancement et
 * l'état des effets vivent dans `layouts/default/indicators.c`.
 *
 * Encodage d'identifiant de touche du firmware d'usine : `colonne * 8 + ligne`,
 * `0xFF` pour « pas de touche ». C'est celui que `0x9659` déplie en
 * `(id >> 3, id & 7)` avant d'appeler `rgb_key_suppressed`, et celui des
 * couronnes ci-dessous -- la couronne 0 vaut `0x3A` = (colonne 7, ligne 2),
 * exactement le centre que la feuille de relevé annonce pour l'onde.
 *
 * Rien de tout ceci n'a été exécuté : aucun firmware n'a jamais été flashé sur
 * l'appareil.
 */

#define AULA_FX_COLS 15
#define AULA_FX_ROWS 6

/* Seconde palette d'usine, CODE 0x2D6D : 128 triplets (R,G,B), 384 octets.
 * Distincte de la roue de 192 entrées de `aula_rgb.c` -- ses rampes sont plus
 * saturées et son cercle se referme en 128 pas. Lue par le moteur d'indice 15. */
#define AULA_FX_PALETTE_SIZE 128
void aula_fx_palette(uint8_t index, uint8_t out[3]);

/*
 * Modes de couleur d'usine : 0 à 6 sont des couleurs fixes (CODE 0xC800), 7 est
 * l'arc-en-ciel, qui lit la roue de teintes au lieu de la palette. Le firmware
 * d'usine range ce mode dans `b1[3:0]` de son enregistrement par effet, et le
 * verrouille en XRAM 0x0897.
 */
#define AULA_FX_COLOR_MODES  8
#define AULA_FX_COLOR_WHEEL  ((uint8_t)(AULA_FX_COLOR_MODES - 1))
void aula_fx_color(uint8_t mode, uint8_t out[3]);

/* Couronnes de l'onde concentrique, CODE 0x2959 : 9 groupes de 13 identifiants,
 * `0xFF` = emplacement vide. Lues par le moteur d'indice 1 (et d'indice 17, qui
 * est le même moteur avec le paramètre figé à 9). */
#define AULA_FX_RINGS      9
#define AULA_FX_RING_SLOTS 13
uint8_t aula_fx_ring(uint8_t ring, uint8_t slot);

/*
 * Carte de touches d'usine, CODE 0xC500, indexée `colonne * 6 + ligne` : elle
 * rend l'identifiant `colonne * 8 + ligne`, ou AULA_FX_NO_KEY quand la grille
 * ne porte pas de touche. Elle sert à la fois de test de présence et de carte
 * spatiale -- voir le commentaire dans aula_fx.c.
 */
#define AULA_FX_NO_KEY ((uint8_t)0xFF)
uint8_t aula_fx_key_id(uint8_t col, uint8_t row);

/*
 * Colonne spatiale d'une colonne électrique : `aula_fx_key_id() >> 3`. Identité
 * partout sauf sur la ligne 4, qui permute.
 */
uint8_t aula_fx_render_col(uint8_t col, uint8_t row);

/* Image « gaming », plan bleu de CODE 0xCAFC : Échap, W A S D et le pavé
 * fléché. Même encodage en masque de lignes. */
uint8_t aula_fx_gaming(uint8_t col);

/*
 * Champ de phase par touche de l'effet 15, CODE 0x9FEA.
 *
 * Ce n'est pas du code qui l'écrit, c'est l'INITIALISEUR C : la table de
 * décompression Keil en CODE 0x9FDE ne compte que trois enregistrements, et le
 * troisième copie 126 octets vers XDATA 0x0E49 au démarrage. `fcn @ 0xACA3` les
 * y recopie ensuite transposés (pas de 21 -> pas de 6) dans le plan de phase
 * 0x0017. Voilà pourquoi aucune instruction de l'image ne référence 0x0E49 en
 * écriture : c'est une constante, pas une variable.
 *
 * Les valeurs vont de 1 à 127 -- très exactement la plage d'un index dans la
 * palette de 128 -- et dessinent un balayage angulaire : la phase croît le long
 * de la première ligne jusqu'à repasser par zéro entre les colonnes 12 et 13,
 * et décroît vers le bas à gauche. Une vague qui tourne, pas qui translate.
 */
uint8_t aula_fx_keywave(uint8_t col, uint8_t row);

/* Tirage pseudo-aléatoire, 0-255. */
uint8_t aula_fx_rand(void);
