#include "kbdef.h"
#include "user_matrix.h"

/* Aucune colonne n'est pilotée, aucune ligne n'est lue. */
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
