#pragma once

/*
 * Liaison sans-fil de l'AULA F75 : BK3632 relié par EUART0.
 *
 * Rien à voir avec `src/platform/bk3632/rf_controller.c`, qui parle au même SoC
 * mais par SPI bit-bangé (NuPhy Air60). Ici le transport est l'EUART0 du
 * SH68F90A, avec sa propre trame et son propre jeu de commandes.
 *
 * Tout ce qui suit vient du désassemblage du firmware d'usine
 * (`docs/keyboards/aula-f75.md`). Rien n'a été flashé ni mesuré sur l'appareil.
 */

#include <stdint.h>
#include <stdbool.h>
#include "report.h"

/*
 * Transport actif. C'est l'encodage LOCAL du firmware d'usine (`XRAM 0x031B`),
 * pas celui qui circule sur le fil -- voir rf_slot_t.
 */
typedef enum {
    RF_LINK_WIRED = 0, /* USB : la radio est muette, l'USB est initialisé */
    RF_LINK_24G   = 1, /* dongle 2,4 GHz */
    RF_LINK_BT    = 2  /* Bluetooth, slot dans rf_bt_slot() */
} rf_link_t;

/*
 * Octet « slot » de la commande 0x01, tel qu'il part sur le fil.
 *
 * ATTENTION : cet encodage n'est PAS celui de rf_link_t, et les deux se
 * croisent sur la valeur 0 -- qui vaut « filaire » côté rf_link_t et
 * « 2,4 GHz » sur le fil. Ne jamais passer un rf_link_t là où un rf_slot_t est
 * attendu. La conversion se fait dans rf_link_slot().
 */
typedef enum {
    RF_SLOT_24G = 0,
    RF_SLOT_BT1 = 1,
    RF_SLOT_BT2 = 2,
    RF_SLOT_BT3 = 3
} rf_slot_t;

void rf_init(void);

/*
 * À appeler depuis la boucle principale, pas depuis une ISR : échantillonne le
 * sélecteur à glissière, applique les changements de transport et consomme les
 * trames reçues.
 */
void rf_task(void);

rf_link_t rf_link(void);
bool      rf_is_wireless(void);

/*
 * « Une trame d'état valide est arrivée depuis le dernier changement de mode. »
 * Ce n'est PAS le drapeau de connexion du firmware d'usine : le champ qui le
 * porte n'a pas été isolé dans `euart0_parse`. Pas de péremption non plus,
 * faute de base de temps — l'indication ne retombe pas toute seule.
 */
bool rf_connected(void);

uint8_t rf_bt_slot(void);
void    rf_set_bt_slot(uint8_t slot);

/* Réaffirme le lien courant avec le drapeau « appairage » de la commande 0x01. */
void rf_request_pairing(void);

void rf_send_report(__xdata report_keyboard_t *report);
void rf_send_nkro(__xdata report_nkro_t *report);
void rf_send_extra(__xdata report_extra_t *report);

void rf_query_status(void);
void rf_send_battery(uint8_t percent);
