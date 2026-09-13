#include "kbdef.h"
#include "user_matrix.h"

/*
 * Aucune colonne n'est pilotée, aucune ligne n'est lue.
 *
 * Pour mémoire, le brochage relevé sur le firmware d'usine -- à câbler dans la
 * carte définitive, PAS ici :
 *
 *   colonnes 0-2   P5.0 P5.1 P5.2
 *   colonnes 3-8   P3.5 P3.4 P3.3 P3.2 P3.1 P3.0
 *   colonnes 9-13  P2.5 P2.4 P2.3 P2.2 P2.1
 *   lignes   0-4   P7.1 P7.2 P7.3 P5.3 P5.4
 *
 * Tout est actif bas : excitation par `ANL PxCR,#~bit` puis `ANL Px,#~bit`,
 * relâchement par `ORL Px,#masque`. ⚠️ Le masque de P5 vaut 0x07 et non 0x3F :
 * il épargne P5.3/P5.4, qui sont des lignes.
 */
void user_matrix_cols_deselect_all(void) {}
void user_matrix_col_select(uint8_t col) { (void)col; }
void user_matrix_col_deselect(uint8_t col) { (void)col; }
void user_matrix_scan_pre(void) {}
void user_matrix_scan_post(void) {}
void user_matrix_sinks_off(void) {}

/* Toutes les lignes au repos : aucune touche ne sera jamais rapportée. */
uint8_t user_matrix_read_rows(void)
{
    return 0xFF;
}
