#include "kbdef.h"
#include "kb.h"
#include "usb.h"
#include "debug.h"
#include "settings.h"
#include "keyboard.h"
#include "report.h"
#include "isp.h"

#ifdef RF_ENABLED
#    include "rf_controller.h"
#endif

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * Le mode de liaison
 *
 * ⚠️ Le GM610 n'a AUCUN interrupteur. Là où la nuphy-air60 lit deux curseurs
 * (`CONN_MODE_SWITCH`, anti-rebond sur 256 tours), tout se choisit ici au
 * clavier, aux positions et aux temporisations d'usine
 * (docs/GM610_SYNTHESE.md, section 7) :
 *
 *     Fn + Tab, ~3 s     bascule USB <-> sans-fil
 *     Fn + Q / W / E     canaux Bluetooth 1 / 2 / 3, immédiat
 *     Fn + G,   ~3 s     liaison 2,4 GHz
 *
 * Maintenir en plus un sélecteur Bluetooth déjà actif lance l'appairage.
 * ═══════════════════════════════════════════════════════════════════════════
 */

/*
 * Le GM610 n'a pas d'underglow : ce champ de `user_settings` est libre et
 * persiste le mode de liaison. Lui donner son vrai nom demanderait de toucher
 * `src/smk/settings.h`, partagé par toutes les cartes.
 */
#define conn_mode_setting ul_effect

static uint8_t conn_mode;

uint8_t kb_conn_mode(void)
{
    return conn_mode;
}

void kb_send_report(__xdata report_keyboard_t *report)
{
#ifdef RF_ENABLED
    if (conn_mode == KB_CONN_RF) {
        rf_send_report(report);
        return;
    }
#endif
    usb_send_report(report);
}

void kb_send_nkro(__xdata report_nkro_t *report)
{
#ifdef RF_ENABLED
    if (conn_mode == KB_CONN_RF) {
        rf_send_nkro(report);
        return;
    }
#endif
    usb_send_nkro(report);
}

void kb_send_extra(__xdata report_extra_t *report)
{
#ifdef RF_ENABLED
    if (conn_mode == KB_CONN_RF) {
        rf_send_extra(report);
        return;
    }
#endif
    usb_send_extra(report);
}

extern void indicators_next_effect(void);
extern void indicators_brightness_up(void);
extern void indicators_brightness_down(void);
extern void indicators_speed_up(void);
extern void indicators_speed_down(void);
extern void indicators_toggle_diag(void);
extern void indicators_link_flash(void);
extern void indicators_all_off(void);
extern void indicators_boot_announce(void);

/*
 * Durée d'un maintien « long » : 100 battements de 30 ms = **3 s**, la
 * temporisation du firmware d'usine.
 *
 * ⚠️ Comptée en battements de l'ISR systick, PAS en tours de boucle principale.
 * La première version comptait 6 000 tours, calibrés à ~2 kHz en USB au repos --
 * mais la boucle n'a pas de cadence : en Bluetooth non connecté elle s'effondre,
 * et le maintien devenait interminable. `Fn + Tab` et `Fn + B` ne répondaient
 * plus, c'est-à-dire que **la porte de secours était muette dans l'état même où
 * elle sert**. Mesuré sur l'appareil le 2026-09-13.
 */
#define LNK_HOLD_TICKS ((uint8_t)100)

extern volatile uint8_t indicators_ticks; // 8 bits : lecture atomique sur 8051

static uint8_t  hold_start;
static uint16_t hold_keycode;
static bool     hold_done; // le maintien a déjà agi : ne pas répéter

#ifdef RF_ENABLED

static rf_mode_t keycode_to_rf_mode(uint16_t keycode)
{
    switch (keycode) {
        case LNK_BT1:
            return RF_MODE_BT1;
        case LNK_BT2:
            return RF_MODE_BT2;
        case LNK_BT3:
            return RF_MODE_BT3;
        default:
            return RF_MODE_2_4G;
    }
}

static void apply_conn_mode(uint8_t mode)
{
    conn_mode                          = mode;
    user_settings.conn_mode_setting    = mode;
    settings_mark_dirty();

    if (mode == KB_CONN_USB) {
        dprintf("conn USB\r\n");
        rf_apply_usb_mode();
    } else {
        dprintf("conn RF link %u\r\n", (unsigned)user_settings.rf_link);
        rf_set_link((rf_mode_t)user_settings.rf_link);
        rf_kbd_lazy_state_init();
    }
    indicators_link_flash();
}

static void select_link(rf_mode_t mode)
{
    user_settings.rf_link = (uint8_t)mode;
    settings_mark_dirty();

    if (conn_mode != KB_CONN_RF) {
        apply_conn_mode(KB_CONN_RF); // il relit rf_link et pose l'annonce
        return;
    }

    dprintf("rf link %u\r\n", (unsigned)mode);
    rf_set_link(mode);
    keyboard_state.rf_link   = (uint8_t)mode;
    keyboard_state.connected = 1;
    keyboard_state.paired    = 1;
    indicators_link_flash();
}

#endif // RF_ENABLED

/*
 * ⛔ `kb_init()` NE PEUT PAS lire les réglages.
 *
 * `main.c` appelle `kb_init()` (l. 77) puis `rf_init()` (l. 80) puis seulement
 * `restore_settings()` (l. 83). Y relire `conn_mode_setting` tombait sur un
 * `user_settings` encore à zéro -- donc `0 == KB_CONN_RF` -- et le clavier
 * démarrait TOUJOURS en sans-fil, en appelant `rf_set_link()` avant même que
 * `rf_init()` ait tourné. Mesuré sur l'appareil : la console sortait
 * `ue=01` (USB enregistré) et pourtant le mode courant était le sans-fil.
 *
 * La restauration se fait donc au premier tour de boucle principale, quand
 * tout est en place. `main.c` est partagé par toutes les cartes : on ne le
 * touche pas pour une seule.
 */
static bool conn_restored;

void kb_init(void)
{
    conn_mode     = KB_CONN_USB; // valeur sûre tant que rien n'est chargé
    conn_restored = false;
}

static void conn_restore_once(void)
{
    if (conn_restored) {
        return;
    }
    conn_restored = true;

#ifdef RF_ENABLED
    conn_mode = (user_settings.conn_mode_setting == KB_CONN_RF) ? KB_CONN_RF : KB_CONN_USB;
    dprintf("conn restore %s\r\n", (conn_mode == KB_CONN_RF) ? "RF" : "USB");
    if (conn_mode == KB_CONN_RF) {
        rf_set_link((rf_mode_t)user_settings.rf_link);
        rf_kbd_lazy_state_init();
    }
#endif
}

/*
 * Les raccourcis RGB reprennent les positions d'usine : Fn + \ l'effet,
 * Fn + [ / ] la vitesse, Fn + ; / ' la luminosité. Fn + D est en plus -- le
 * balayage de diagnostic des puits, sans équivalent d'usine.
 */
/*
 * ── Fn + Maj gauche + ↑/↓ = PgPréc / PgSuiv ─────────────────────────────────
 *
 * ⚠️ Le Maj doit être RETIRÉ du rapport. Sans ça l'hôte lit `Maj + PgPréc`,
 * c'est-à-dire « sélectionner une page » et non « tourner une page » -- le
 * contraire de ce qu'on demande. `set_mods_mask()` masque le bit dans le
 * rapport sortant sans toucher à l'état réel du modificateur, donc relâcher le
 * Maj reste correctement vu.
 *
 * Le code substitué est mémorisé par flèche : la touche doit se relâcher sur le
 * MÊME code qu'à l'appui, même si le Maj a été lâché entre-temps. Sinon l'hôte
 * garde un PgPréc enfoncé pour toujours.
 */
#define MOD_EITHER_SHIFT (uint8_t)(MOD_BIT(KC_LSFT) | MOD_BIT(KC_RSFT))

static uint8_t sub_up;   // code réellement envoyé pour ↑, 0 si aucun
static uint8_t sub_down; // idem pour ↓

static bool arrow_page(uint16_t keycode, bool key_pressed)
{
    uint8_t *memo;
    uint8_t  page;

    if (keycode == KC_UP) {
        memo = &sub_up;
        page = KC_PGUP;
    } else {
        memo = &sub_down;
        page = KC_PGDN;
    }

    if (key_pressed) {
        /*
         * Les DEUX Maj. Le droit est juste au-dessus de `Fn` et des flèches :
         * c'est l'accord naturel, d'une seule main -- et c'est celui qui a
         * répondu sur l'appareil quand le gauche ne répondait pas, sans que
         * j'aie su dire pourquoi. Accepter les deux rend la question sans objet
         * et couvre les deux mains.
         */
        const uint8_t shifts = (uint8_t)(get_mods() & MOD_EITHER_SHIFT);
        if (shifts == 0) {
            return false; // pas de Maj : la flèche part normalement
        }
        *memo = page;
        set_mods_mask((uint8_t)~shifts); // ne masquer que ceux réellement tenus
        add_key(page);
    } else {
        if (*memo == 0) {
            return false;
        }
        del_key(*memo);
        *memo = 0;
        if (sub_up == 0 && sub_down == 0) {
            set_mods_mask(0xFF); // les deux relâchées : le Maj repasse
        }
    }
    send_keyboard_report();
    return true;
}

bool kb_process_record(uint16_t keycode, bool key_pressed)
{
    /*
     * ⚠️ Annuler un maintien dès qu'une AUTRE touche bouge.
     *
     * Sans ça, relâcher Fn avant la touche maintenue arme une bombe à retardement :
     * la couche retombe, donc le relâchement de B remonte en `KC_B` et non en
     * `KB_BOOT`, le compteur n'est jamais remis à zéro, et le saut vers le
     * bootloader part trois secondes plus tard alors que l'utilisateur a lâché.
     * Le même piège vaut pour les sélecteurs de liaison.
     */
    if (hold_keycode && keycode != hold_keycode) {
        hold_keycode = 0;
        hold_done    = true;
    }

    switch (keycode) {
        case KC_UP:
        case KC_DOWN:
            // Rend `true` seulement s'il a pris la main ; sinon la flèche passe.
            return !arrow_page(keycode, key_pressed);

        case FX_NEXT:
            if (key_pressed) indicators_next_effect();
            return false;
        case BRI_UP:
            if (key_pressed) indicators_brightness_up();
            return false;
        case BRI_DN:
            if (key_pressed) indicators_brightness_down();
            return false;
        case SPD_UP:
            if (key_pressed) indicators_speed_up();
            return false;
        case SPD_DN:
            if (key_pressed) indicators_speed_down();
            return false;
        case RGB_DIAG:
            if (key_pressed) indicators_toggle_diag();
            return false;

        case KB_BOOT:
            // N'agit qu'au maintien : `kb_update()` l'arme. Voir kbdef.h.
            if (key_pressed) {
                hold_keycode = keycode;
                hold_start   = indicators_ticks;
                hold_done    = false;
            } else if (hold_keycode == keycode) {
                hold_keycode = 0;
            }
            return false;

#ifdef RF_ENABLED
        case LNK_BT1:
        case LNK_BT2:
        case LNK_BT3:
            // Immédiat, comme en usine. Maintenu, il lance l'appairage.
            if (key_pressed) {
                select_link(keycode_to_rf_mode(keycode));
                hold_keycode = keycode;
                hold_start   = indicators_ticks;
                hold_done    = false;
                dprintf("hold arm bt\r\n");
            } else if (hold_keycode == keycode) {
                hold_keycode = 0;
            }
            return false;

        case LNK_TOGGLE:
        case LNK_24G:
            // Ceux-là n'agissent qu'au maintien : `kb_update()` les arme.
            if (key_pressed) {
                hold_keycode = keycode;
                hold_start   = indicators_ticks;
                hold_done    = false;
                dprintf("hold arm %s\r\n", (keycode == LNK_TOGGLE) ? "tab" : "24g");
            } else if (hold_keycode == keycode) {
                dprintf("hold release\r\n");
                hold_keycode = 0;
            }
            return false;
#endif

        default:
            return true;
    }
}

/*
 * ⛔ NE PAS SERVIR LA RADIO PENDANT UNE ÉNUMÉRATION USB.
 *
 * `bb_spi.c` transfère sous `__critical`, interruptions coupées. C'est assez long
 * pour faire rater des paquets à l'interruption USB -- et un GET_DESCRIPTOR de
 * configuration, qui tient en plusieurs paquets, n'y survit pas. Mesuré sur
 * l'appareil le 2026-09-13, en isolant les variables une par une :
 *
 *     sans radio compilée .................... énumère
 *     radio compilée + rf_init(), mode USB ... énumère
 *     superviseur en marche, mode sans-fil ... ÉCHOUE
 *
 * L'hôte rendait `error -71` (EPROTO) et `-32` (EPIPE), jamais `over-current` :
 * la signature d'un transfert interrompu, pas d'une alimentation qui s'effondre.
 *
 * La règle : on ne parle à la radio que lorsque l'USB est configuré -- donc
 * l'énumération terminée -- OU lorsque aucun hôte ne se manifeste depuis
 * assez longtemps, c'est-à-dire sur batterie, où ce budget n'existe pas.
 *
 * Le compteur repart à zéro dès que l'USB redevient configuré : un rebranchement
 * rouvre donc une fenêtre de silence, et pas seulement le démarrage.
 */
#define USB_QUIET_TICKS ((uint8_t)100) // ~3 s, en battements de 30 ms
#define RF_RATION_MASK  0x07u // hors fenetre de silence : une iteration sur 8

static bool rf_service_allowed(void)
{
    static uint8_t quiet_start;         // battement où le silence a commencé
    static bool    quiet = true;        // muet dès le premier tour
    static bool    was_configured;
    static uint8_t ration;

    if (usb_is_configured()) {
        was_configured = true;
        quiet          = false;

        /*
         * ⚠️ « Énumération finie » ne veut PAS dire « plein régime autorisé ».
         *
         * L'hôte continue d'émettre des transferts de contrôle bien après
         * l'énumération, et une fenêtre `__critical` trop longue les casse tout
         * autant -- il réinitialise alors le périphérique, et le cycle
         * recommence. Or la radio travaille le plus quand la liaison N'EST PAS
         * établie : le superviseur retente en boucle, `bb_spi` enchaîne. C'est
         * exactement l'état où un reflash laisse le clavier, appairage perdu.
         *
         * Donc on rationne tant que le lien n'est pas connecté, quel que soit
         * l'état de l'USB. Une fois connecté, le superviseur ne fait presque
         * plus rien et la radio peut parler librement.
         */
        if (!keyboard_state.connected) {
            return (++ration & RF_RATION_MASK) == 0;
        }
        return true;
    }

    // On vient de perdre la configuration : débranchement, reset de bus, ou
    // rebranchement imminent. Une nouvelle énumération est probable -> silence.
    if (was_configured) {
        was_configured = false;
        quiet          = true;
        quiet_start    = indicators_ticks;
    }

    if (quiet) {
        // ⚠️ En battements matériels, pas en tours de boucle : la boucle
        // s'effondre justement quand la radio cherche sa liaison.
        if ((uint8_t)(indicators_ticks - quiet_start) < USB_QUIET_TICKS) {
            return false;
        }
        quiet = false;
    }

    /*
     * Passé ce délai, aucun hôte ne s'est manifesté : on est très probablement
     * sur batterie, où le budget d'énumération n'existe pas. Mais on ne peut PAS
     * l'affirmer -- le câble peut être branché à l'instant, et l'énumération
     * démarrerait pendant que la radio parle. Faute de savoir, on rationne :
     * une fenêtre sur huit suffit à laisser passer un transfert EP0, et coûte
     * moins de 4 ms de latence sur la liaison sans-fil, ce qui ne se voit pas.
     */
    return (++ration & RF_RATION_MASK) == 0;
}

void kb_update(void)
{
    conn_restore_once();

    if (hold_keycode && !hold_done) {
        if ((uint8_t)(indicators_ticks - hold_start) < LNK_HOLD_TICKS) {
            // rien : le battement matériel court, la boucle peut ramer
        } else {
            hold_done = true;
            switch (hold_keycode) {
                case KB_BOOT:
                    /*
                     * Le saut lui-même se fait dans l'ISR (`fx_tick`), après une
                     * annonce rouge d'une demi-seconde : ici on ne fait que le
                     * demander. Sauter depuis la boucle principale marchait en
                     * USB, mais c'est en Bluetooth non connecté -- quand la
                     * boucle rame -- que cette porte doit servir.
                     */
                    indicators_boot_announce();
                    break;
#ifdef RF_ENABLED
                case LNK_TOGGLE:
                    apply_conn_mode((conn_mode == KB_CONN_USB) ? KB_CONN_RF : KB_CONN_USB);
                    break;
                case LNK_24G:
                    select_link(RF_MODE_2_4G);
                    break;
                case LNK_BT1:
                case LNK_BT2:
                case LNK_BT3: {
                    // Un sélecteur Bluetooth maintenu : on repart en appairage.
                    const rf_mode_t mode = keycode_to_rf_mode(hold_keycode);
                    dprintf("rf pairing %u\r\n", (unsigned)mode);
                    keyboard_state.paired    = 0;
                    keyboard_state.connected = 0;
                    rf_set_link_pairing(mode, &keyboard_state);
                    break;
                }
#endif
                default:
                    break;
            }
        }
    }

#ifdef RF_ENABLED
    if (conn_mode == KB_CONN_RF && rf_service_allowed()) {
        rf_link_supervisor(&keyboard_state);
        rf_send_pending_flush();
        rf_blanking_tick();
    }
#endif
}
