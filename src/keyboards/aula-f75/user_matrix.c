#include "kbdef.h"
#include "gpio.h"
#include "user_matrix.h"

/*
 * Scan de matrice -- NON IMPLÉMENTÉ.
 *
 * Chaque fonction ci-dessous a besoin du brochage réel (masques de port pour
 * les colonnes, broches de ligne, sens actif). Rien n'est devinable : les
 * valeurs du NuPhy Air60 partagent le MCU mais pas le PCB.
 *
 * kbdef.h refuse de compiler tant que AULA_F75_PINMAP_VERIFIED n'est pas
 * défini, donc ce fichier ne peut pas produire de firmware par accident.
 *
 * Modèle à suivre (cf. src/keyboards/nuphy-air60/user_matrix.c) :
 *   - colonnes pilotées en sortie, sélection ACTIVE BASSE
 *   - lignes lues en entrée avec pull-up, un octet par scan
 *   - scan_post remet tout en entrée sans pull-up
 */

void user_matrix_cols_deselect_all(void)
{
    /* TODO: GPIO_HIGH(port, masque) pour chaque port portant des colonnes */
}

void user_matrix_scan_pre(void)
{
    /* TODO: GPIO_OUTPUT(port, masque) pour chaque port portant des colonnes */
}

void user_matrix_scan_post(void)
{
    /* TODO: GPIO_PULLUP_OFF puis GPIO_INPUT sur les mêmes ports */
}

void user_matrix_col_select(uint8_t col)
{
    (void)col; /* TODO: mettre la colonne à 0 (actif bas) */
}

void user_matrix_col_deselect(uint8_t col)
{
    (void)col; /* TODO: remettre la colonne à 1 */
}

uint8_t user_matrix_read_rows(void)
{
    return 0; /* TODO: assembler les 6 bits de ligne en un octet */
}

void user_matrix_sinks_off(void)
{
    /* TODO: couper les sinks RGB avant lecture, pour éviter la diaphonie */
}
