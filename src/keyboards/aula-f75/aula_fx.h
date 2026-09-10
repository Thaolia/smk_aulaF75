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

/* Couronnes de l'onde concentrique, CODE 0x2959 : 9 groupes de 13 identifiants,
 * `0xFF` = emplacement vide. Lues par le moteur d'indice 1 (et d'indice 17, qui
 * est le même moteur avec le paramètre figé à 9). */
#define AULA_FX_RINGS      9
#define AULA_FX_RING_SLOTS 13
uint8_t aula_fx_ring(uint8_t ring, uint8_t slot);

/* Carte de présence, CODE 0xC500 : les six emplacements de la grille 6x15 qui
 * n'ont pas de touche. Un bit par ligne, un octet par colonne. */
uint8_t aula_fx_present(uint8_t col);

/* Image « gaming », plan bleu de CODE 0xCAFC : Échap, W A S D et le pavé
 * fléché. Même encodage en masque de lignes. */
uint8_t aula_fx_gaming(uint8_t col);

/* Tirage pseudo-aléatoire, 0-255. */
uint8_t aula_fx_rand(void);
