#pragma once

/*
 * Liaison sans-fil de l'AULA F75 : BK3632 relié par EUART0.
 *
 * Rien à voir avec `src/platform/bk3632/rf_controller.c`, qui parle au même SoC
 * mais par SPI bit-bangé (NuPhy Air60). Ici le transport est l'EUART0 du
 * SH68F90A, avec sa propre trame et son propre jeu de commandes.
 *
 * Tout ce qui suit vient du désassemblage du firmware d'usine
 * (`docs/keyboards/aula-f75.md`). Le code a été flashé et tourne, mais la
 * liaison elle-même n'a jamais été établie : voir l'overlay de diagnostic.
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
 * « Le module a répondu à une sonde récente. »
 *
 * Ce n'est PAS le drapeau de connexion du firmware d'usine : le champ de la
 * trame d'état qui porte l'appairage n'a pas été isolé dans `euart0_parse`. Ce
 * qui est transcrit, c'est sa sonde de présence (`fcn.00003901`, commande 0x06
 * toutes les cent itérations, trois échecs et la liaison est déclarée morte) —
 * donc l'indication retombe, mais elle dit « la radio répond », pas « un hôte
 * est apparié ».
 *
 * `rf_task()` la recopie dans `keyboard_state.connected`.
 */
bool rf_connected(void);

uint8_t rf_bt_slot(void);
void    rf_set_bt_slot(uint8_t slot);


/*
 * Réaffirme le lien courant avec le drapeau « appairage » de la commande 0x01.
 * Le firmware d'usine fait la même chose depuis son tic lent quand le raccourci
 * d'appairage a posé le bit 0x2C.5 (`fcn.0000870C`).
 */
void rf_request_pairing(void);

/*
 * À appeler depuis `user_sleep_prepare()` / `user_sleep_wake()`, autour de
 * `power_enter_powerdown()`. Sans effet en mode filaire.
 */
void rf_sleep_prepare(void);
void rf_sleep_wake(void);

/*
 * ---------------------------------------------------------------- DIAGNOSTIC
 *
 * Instantané d'état, rempli à chaque passage de `rf_task()` et lu par l'overlay
 * de `indicators.c` (touche `Fn + R`). C'est le SEUL instrument utilisable dans
 * le montage qui échoue -- batterie, USB débranché -- où la console HID n'existe
 * pas.
 *
 * Un tableau et non un accesseur : l'overlay est peint depuis l'ISR Timer2, et
 * un appel vers `aula_rf.c` depuis l'ISR ferait partager au module le
 * recouvrement statique SDCC de la boucle principale.
 */
#define RF_DIAG_P74    0 /* sélecteur, broche 1 */
#define RF_DIAG_P45    1 /* sélecteur, broche 2 */
#define RF_DIAG_P47    2 /* « module prêt » */
#define RF_DIAG_LINK   3 /* rf_link_t courant */
#define RF_DIAG_TXPEND 4 /* link-select encore en attente d'émission */
#define RF_DIAG_NAME   5 /* nombre de noms Bluetooth émis, 0 à 2 */
#define RF_DIAG_TXBUSY 6 /* rafale série en vol */
#define RF_DIAG_CONN   7 /* le module a répondu à une sonde récente */
#define RF_DIAG_QCOUNT 8 /* rapports en file, 0 à 6 -- sature si rien ne part */
#define RF_DIAG_MISSES 9 /* sondes d'état sans réponse */
#define RF_DIAG_FIELDS 10

extern __xdata uint8_t rf_diag_state[RF_DIAG_FIELDS];

void rf_send_report(__xdata report_keyboard_t *report);
void rf_send_nkro(__xdata report_nkro_t *report);
void rf_send_extra(__xdata report_extra_t *report);
