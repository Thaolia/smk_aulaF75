#pragma once

#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Format du stockage macro du GM610 — SOURCE UNIQUE.
 *
 * Mirroré côté hôte par le GUI (utils/gm610_macro_gui/proto.py) : toute évolution
 * de ces constantes doit y être répercutée.
 *
 * N'est actif (champ dans user_settings, collection HID, handlers) que lorsque
 * MACRO_STORE_ENABLE est défini — posé par meson.build pour la seule carte gm610,
 * pour ne pas alourdir le blob NVM des autres cartes (sh68f881 : 252 o).
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Le store NVM (nvm.c) code la longueur du record sur UN octet, et settings.c la
 * passe en (uint8_t) : sizeof(user_settings_t) doit donc rester <= 255. Avec les
 * 8 octets de réglages existants, le blob est plafonné à 247 ; on prend 240.
 */
#define MACRO_BLOB_SIZE     240u
#define MACRO_MAX_KEYS      16u  /* nombre max de touches porteuses d'une macro */
#define MACRO_STORE_VERSION 0x01u

/*
 * Disposition du blob :
 *   [0]      version (= MACRO_STORE_VERSION ; autre valeur => aucune macro)
 *   [1..3]   couleur de surbrillance R, G, B
 *   [4]      count (0..MACRO_MAX_KEYS)
 *   [5..]    directory[count] : { key_index (row*MATRIX_COLS+col), n_steps }  (2 o)
 *   puis     steps concaténés dans l'ordre du directory : 2 o par step
 *              {0x00, n}        => délai de n * MACRO_DELAY_UNIT_MS ms
 *              {keycode, mods}  => tap (keycode basique HID + masque modificateurs)
 */
#define MACRO_BLOB_VERSION_OFF 0u
#define MACRO_BLOB_HICOLOR_OFF 1u
#define MACRO_BLOB_COUNT_OFF   4u
#define MACRO_BLOB_DIR_OFF     5u

#define MACRO_STEP_DELAY_TAG 0x00u /* KC_NO=0 ne tape jamais : tag de délai sûr */
#define MACRO_DELAY_UNIT_MS  10u

/*
 * Report HID FEATURE (ID = REPORT_ID_MACRO dans report.h) : 7 o de données tiennent
 * dans un paquet EP0 de 8 o (avec l'octet d'ID en tête) — donc jamais de transfert
 * de contrôle multi-paquets, que usb_ep0_out_irq ne sait pas assembler.
 */
#define MACRO_FEATURE_REPORT_SIZE 7u

/*
 * Protocole de poussée hôte→clavier (octet 1 de la trame = opcode) :
 *   BEGIN  : arg = len bas, p0 = len haut   → (ré)initialise le staging à `len` o
 *   DATA   : arg = index de trame, p0..p4   → 5 octets écrits à l'offset (index*5)
 *   COMMIT : arg = checksum (somme 8 bits)  → valide, puis persiste EN BOUCLE
 *                                             PRINCIPALE (jamais dans l'ISR USB)
 *   SEEK   : arg = offset                    → place le curseur de LECTURE (GET)
 *
 * Lecture (dump) : SET SEEK(0) puis GET_REPORT(FEATURE, id) répétés ; chaque GET
 * renvoie [id, 7 octets du blob @ curseur] et avance le curseur de 7. Windows borne
 * un GET à la taille déclarée du report (7 o), d'où la lecture par chunks.
 */
#define MACRO_OP_BEGIN     0x01u
#define MACRO_OP_DATA      0x02u
#define MACRO_OP_COMMIT    0x03u
#define MACRO_OP_READ_SEEK 0x04u

#if defined(MACRO_STORE_ENABLE)
/*
 * Appelé depuis l'ISR USB (usb_ep0_out_irq) avec EP0_OUT_BUF :
 * [0]=report id, [1]=opcode, [2]=arg, [3..7]=5 octets de charge utile.
 * Ne fait QUE recopier dans le staging et lever des drapeaux — aucun appel à une
 * fonction du chemin boucle principale (contrainte de recouvrement SDCC).
 */
void macro_hid_receive(const __xdata uint8_t *buf);

/* Remplit `buf` (EP0_IN_BUF) pour un GET_REPORT : [id, 7 octets du blob au curseur],
 * et avance le curseur. Appelé depuis l'ISR USB. */
void macro_hid_fill_get(__xdata uint8_t *buf);
#endif
