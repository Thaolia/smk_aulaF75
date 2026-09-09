#pragma once

#include "sh68f90.h"
#include "keycodes.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Epomaker x AULA F75 (modèle classique, PAS le "F75 Ultra")
 *
 *   MCU      SinoWealth SH68F90A, marquage IC BYK916
 *   USB      258A:010C  (lus dans la flash d'usine en 0x5FCE)
 *   Sans fil BK3632 Beken, relié par EUART0 -- voir la note plus bas
 *
 * ATTENTION : ce portage est INCOMPLET par construction. Le brochage n'a pas
 * été établi et n'est pas devinable depuis le firmware d'usine sans travail
 * supplémentaire. Les symboles manquants provoquent volontairement une erreur
 * de compilation : un fichier qui refuse de compiler est un livrable correct,
 * un fichier qui compile en briquant le clavier ne l'est pas.
 *
 * Feuille de relevé : docs/keyboards/aula-f75.md
 */

/* -------------------------------------------------------------------------
 * Matrice
 *
 * Établi : la table de couleurs d'OpenRGB expose num_leds = 90 et indexe les
 * LED en colonnes -- index = colonne * 6 + ligne. L'indice max est 89 (flèche
 * droite) et l'indice 6 (entre Échap et F1) est le trou de la rangée F.
 * Donc la grille LED est 6 lignes x 15 colonnes.
 *
 * NON établi : que la matrice de touches partage cette géométrie. C'est le cas
 * habituel sur ces claviers, mais le firmware d'usine ne l'a pas confirmé --
 * aucune borne de boucle de scan n'a été isolée. Une 16e colonne sans LED
 * resterait invisible pour OpenRGB.
 * ------------------------------------------------------------------------- */
#define MATRIX_ROWS 6
#define MATRIX_COLS 15

/* -------------------------------------------------------------------------
 * Brochage -- NON ÉTABLI
 *
 * Définir AULA_F75_PINMAP_VERIFIED seulement après avoir relevé ET vérifié
 * chaque broche. Ne pas recopier celles du NuPhy Air60 : même MCU et même
 * marquage BYK916 ne veulent pas dire même câblage de PCB.
 * ------------------------------------------------------------------------- */
#ifndef AULA_F75_PINMAP_VERIFIED
#    error "AULA F75: brochage non établi. Voir docs/keyboards/aula-f75.md avant de compiler."
#endif

/* Lignes de matrice : 6 broches                                    -- À RELEVER */
/* Colonnes de matrice : 15 (ou 16) broches                         -- À RELEVER */
/* Registres PWM de rétroéclairage, un par colonne                  -- À RELEVER */
/* Broches RGB : R/G/B par ligne                                    -- À RELEVER */
/* Switches de configuration (mode connexion / OS), s'ils existent  -- À RELEVER */

/* -------------------------------------------------------------------------
 * Liaison sans fil
 *
 * Le NuPhy Air60 parle à son BK3632 en SPI bit-bangé (RF_BB_SPI_*,
 * src/platform/bb_spi.c). L'AULA F75 ne fonctionne PAS comme ça : son firmware
 * d'usine utilise EUART0 (vecteur 13, SCON 0xD8 / SBUF 0xAA), en half-duplex --
 * l'ISR bascule la direction d'une broche via P0CR puis P0.2.
 *
 * Conséquence : src/platform/bk3632/rf_controller.c n'est PAS réutilisable tel
 * quel pour ce clavier. Il faudrait un transport EUART0. Le sans-fil est donc
 * hors périmètre de ce portage ; ne pas déclarer 'wireless' dans meson.build.
 * ------------------------------------------------------------------------- */

enum custom_keycodes {
    FX_NEXT = SAFE_RANGE, /* effet RGB suivant */
    FX_PREV,              /* effet RGB précédent */
    BRI_UP,               /* luminosité + */
    BRI_DN,               /* luminosité - */
    SPD_UP,               /* vitesse + */
    SPD_DN,               /* vitesse - */

    KB_SAFE_RANGE,
};
