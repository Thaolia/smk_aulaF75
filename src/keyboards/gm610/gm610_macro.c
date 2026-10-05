#include "gm610_macro.h"
#include "macro_store.h"

#if defined(MACRO_STORE_ENABLE)

#include "kbdef.h"
#include "layout.h" // keymaps
#include "keycodes.h"
#include "report.h"
#include "settings.h"
#include "sleep.h"
#include "sh68f90.h"
#include <stdint.h>

// Pose render_dirty : la surbrillance doit repeindre dès qu'on bascule le mode
// ou qu'un commit change la liste des touches.
extern void indicators_request_render(void);

#define _BL 0

/*
 * Cadence, reprise telle quelle de aula_macro.c : il n'y a pas de compteur de
 * millisecondes libre, la sous-trame LED (~420 µs) est la seule base de temps.
 */
#define MACRO_MS(ms) ((uint16_t)(((uint32_t)(ms) * 1000ul) / 420ul))

/*
 * L'hôte sonde l'endpoint toutes les millisecondes : un appui plus court qu'un
 * sondage serait écrasé par son propre relâchement. 8 sous-trames ≈ 3,4 ms, soit
 * au moins trois sondages — même valeur que aula_macro.c, autant pour le maintien
 * d'une frappe que pour le temps haut qui sépare deux frappes.
 */
#define MACRO_HOLD 8
#define MACRO_GAP  8

// Couleur de surbrillance par défaut (blob vierge/invalide) : cyan.
#define HI_DEFAULT_R 0u
#define HI_DEFAULT_G 255u
#define HI_DEFAULT_B 255u

static bool             macro_mode;
static __xdata uint16_t macro_keymask[MATRIX_ROWS]; // bit col => porte une macro

// Minuteur : la boucle principale l'arme, l'ISR le décrémente (cf. aula_macro.c).
static volatile __xdata uint16_t mp_wait;
static volatile __bit            mp_due;

// Lecteur de séquence.
#define MP_IDLE  0u
#define MP_NEXT  1u // lire et exécuter le prochain step
#define MP_HOLD  2u // tap émis : attendre MACRO_HOLD puis relâcher
#define MP_DELAY 3u // attendre un délai avant le step suivant
static __xdata uint8_t mp_state;
static __xdata uint8_t mp_off;      // offset du prochain step dans le blob
static __xdata uint8_t mp_left;     // steps restants
static __xdata uint8_t mp_cur_key;  // touche basique tenue (0 = aucune)
static __xdata uint8_t mp_cur_mods; // modificateurs tenus
static uint16_t        mp_trigger;  // keycode déclencheur, avalé jusqu'au relâchement
static uint16_t        mp_rnd;      // état xorshift pour la gigue des délais

// Poussée HID : l'ISR USB remplit le staging, la boucle principale committe.
static __xdata uint8_t           staging[MACRO_BLOB_SIZE];
static volatile __xdata uint16_t stg_len;
static volatile __bit            stg_active;
static volatile __bit            commit_pending;
static volatile __xdata uint8_t  commit_sum;
static volatile __xdata uint16_t commit_len;
static volatile __xdata uint8_t  macro_read_off; // curseur de lecture (GET_REPORT)

/* ═══════════════════════════════════════════════════════════════════════════ */

static void mp_arm(uint16_t subframes)
{
    ET2     = 0; // lecture/écriture 16 bits non atomique : couper Timer2 autour
    mp_wait = subframes;
    mp_due  = 0;
    ET2     = 1;
}

void gm610_macro_tick(void)
{
    if (mp_wait != 0) {
        mp_wait--;
        if (mp_wait == 0) {
            mp_due = 1;
        }
    }
}

/*
 * PRNG xorshift 16 bits pour la gigue des pauses. Semence prise sur Timer2 au vol au
 * premier tirage (seule entropie du MCU, comme aula_macro.c) ; l'état persiste entre
 * exécutions, donc chaque lancement de macro donne des timings différents. Aucune
 * exigence cryptographique : il s'agit juste de casser la régularité.
 */
static uint16_t macro_rand(void)
{
    if (mp_rnd == 0) {
        mp_rnd = (uint16_t)(((uint16_t)TH2 << 8) | TL2);
        if (mp_rnd == 0) {
            mp_rnd = 0xA5A5u;
        }
    }
    mp_rnd ^= (uint16_t)(mp_rnd << 7);
    mp_rnd ^= (uint16_t)(mp_rnd >> 9);
    mp_rnd ^= (uint16_t)(mp_rnd << 8);
    return mp_rnd;
}

bool gm610_macro_mode(void)
{
    return macro_mode;
}

void gm610_macro_hi_color(uint8_t rgb[3])
{
    const __xdata uint8_t *b = user_settings.macro_blob;

    if (b[MACRO_BLOB_VERSION_OFF] == MACRO_STORE_VERSION) {
        rgb[0] = b[MACRO_BLOB_HICOLOR_OFF + 0u];
        rgb[1] = b[MACRO_BLOB_HICOLOR_OFF + 1u];
        rgb[2] = b[MACRO_BLOB_HICOLOR_OFF + 2u];
    } else {
        rgb[0] = HI_DEFAULT_R;
        rgb[1] = HI_DEFAULT_G;
        rgb[2] = HI_DEFAULT_B;
    }
}

bool gm610_macro_has_key(uint8_t row, uint8_t col)
{
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return false;
    }
    return (macro_keymask[row] & (uint16_t)(1u << col)) != 0;
}

void gm610_macro_init(void)
{
    const __xdata uint8_t *b = user_settings.macro_blob;
    uint8_t                i;

    for (i = 0; i < MATRIX_ROWS; i++) {
        macro_keymask[i] = 0;
    }
    if (b[MACRO_BLOB_VERSION_OFF] != MACRO_STORE_VERSION) {
        return; // blob vierge ou d'une autre version : aucune macro
    }

    const uint8_t count = b[MACRO_BLOB_COUNT_OFF];
    for (i = 0; i < count && i < MACRO_MAX_KEYS; i++) {
        const uint8_t ki  = b[MACRO_BLOB_DIR_OFF + (uint8_t)(i * 2u)];
        const uint8_t row = (uint8_t)(ki / MATRIX_COLS);
        const uint8_t col = (uint8_t)(ki % MATRIX_COLS); // toujours < MATRIX_COLS
        if (row < MATRIX_ROWS) {
            macro_keymask[row] |= (uint16_t)(1u << col);
        }
    }
}

void gm610_macro_toggle(void)
{
    macro_mode = !macro_mode;
    indicators_request_render();
}

/*
 * Cherche la macro déclenchée par `keycode` : une entrée du directory dont la
 * touche porte, sur la couche de base, ce keycode. Pose *off (offset des steps
 * dans le blob) et *n (nombre de steps). Les keycodes de base du gm610 sont
 * uniques : l'identification par keycode est donc sans ambiguïté.
 */
static bool macro_find(uint16_t keycode, uint8_t *off, uint8_t *n)
{
    const __xdata uint8_t *b = user_settings.macro_blob;

    if (b[MACRO_BLOB_VERSION_OFF] != MACRO_STORE_VERSION || keycode == KC_NO) {
        return false;
    }

    const uint8_t count     = b[MACRO_BLOB_COUNT_OFF];
    uint8_t       steps_off = (uint8_t)(MACRO_BLOB_DIR_OFF + (uint8_t)(count * 2u));

    for (uint8_t i = 0; i < count && i < MACRO_MAX_KEYS; i++) {
        const uint8_t ki  = b[MACRO_BLOB_DIR_OFF + (uint8_t)(i * 2u)];
        const uint8_t ni  = b[MACRO_BLOB_DIR_OFF + (uint8_t)(i * 2u) + 1u];
        const uint8_t row = (uint8_t)(ki / MATRIX_COLS);
        const uint8_t col = (uint8_t)(ki % MATRIX_COLS); // toujours < MATRIX_COLS

        if (row < MATRIX_ROWS && keymaps[_BL][row][col] == keycode) {
            *off = steps_off;
            *n   = ni;
            return true;
        }
        steps_off = (uint8_t)(steps_off + (uint8_t)(ni * 2u));
    }
    return false;
}

static void macro_release_held(void)
{
    if (mp_cur_key != 0) {
        del_key(mp_cur_key);
        mp_cur_key = 0;
    }
    if (mp_cur_mods != 0) {
        del_mods(mp_cur_mods);
        mp_cur_mods = 0;
    }
    send_keyboard_report();
}

static void macro_cancel(void)
{
    macro_release_held();
    mp_state   = MP_IDLE;
    mp_trigger = 0;
    mp_due     = 0;
}

static void macro_start(uint8_t off, uint8_t n)
{
    mp_off      = off;
    mp_left     = n;
    mp_cur_key  = 0;
    mp_cur_mods = 0;
    mp_state    = MP_NEXT;
    mp_due      = 1; // premier step sans attendre
}

bool gm610_macro_trigger(uint16_t keycode, bool pressed)
{
    /*
     * 1) La touche déclencheuse : on avale ses évènements jusqu'à son
     *    relâchement, SANS annuler la lecture — sinon lever le doigt couperait la
     *    macro aussitôt (sa lecture a démarré pendant que la touche est tenue).
     */
    if (mp_trigger != 0 && keycode == mp_trigger) {
        if (!pressed) {
            mp_trigger = 0;
        }
        return true;
    }

    // 2) Toute AUTRE touche pressée pendant la lecture annule.
    if (mp_state != MP_IDLE) {
        if (pressed) {
            macro_cancel();
        }
        return true; // on avale tout le temps que ça joue
    }

    // 3) Au repos : démarrer si la touche porte une macro non vide.
    if (pressed) {
        uint8_t off, n;
        if (macro_find(keycode, &off, &n) && n != 0) {
            macro_start(off, n);
            mp_trigger = keycode;
            return true;
        }
    }
    return false; // pas une touche-macro : frappe normale
}

// Validé et persisté EN BOUCLE PRINCIPALE : gm610_macro_init() et
// settings_mark_dirty() ne doivent pas être atteints depuis l'ISR USB.
static void macro_do_commit(void)
{
    uint16_t i;
    uint8_t  sum = 0;

    commit_pending = 0;
    if (commit_len > MACRO_BLOB_SIZE) {
        return;
    }
    for (i = 0; i < commit_len; i++) {
        sum = (uint8_t)(sum + staging[i]);
    }
    if (sum != commit_sum) {
        return; // trame perdue : le GUI renverra
    }

    for (i = 0; i < commit_len; i++) {
        user_settings.macro_blob[i] = staging[i];
    }
    for (; i < MACRO_BLOB_SIZE; i++) {
        user_settings.macro_blob[i] = 0;
    }
    settings_mark_dirty();
    gm610_macro_init();
    indicators_request_render();
}

void gm610_macro_task(void)
{
    if (commit_pending) {
        macro_do_commit();
    }

    if (mp_state == MP_IDLE || !mp_due) {
        return;
    }
    mp_due = 0;

    // Sinon sleep.c endormirait le clavier au milieu d'une macro longue.
    sleep_note_activity();

    switch (mp_state) {
        case MP_NEXT: {
            if (mp_left == 0 || (uint16_t)mp_off + 1u >= MACRO_BLOB_SIZE) {
                macro_release_held();
                mp_state = MP_IDLE;
                // mp_trigger reste armé : son relâchement sera avalé.
                break;
            }

            const uint8_t b0 = user_settings.macro_blob[mp_off];
            const uint8_t b1 = user_settings.macro_blob[mp_off + 1u];
            mp_off = (uint8_t)(mp_off + 2u);
            mp_left--;

            if (b0 == MACRO_STEP_DELAY_TAG) {
                // Gigue ±10 ms re-tirée à chaque exécution : casse la régularité des
                // pauses pour que la séquence ressemble moins à une macro.
                int16_t ms = (int16_t)((uint16_t)b1 * MACRO_DELAY_UNIT_MS);
                ms += (int16_t)(macro_rand() % 21u) - 10;
                if (ms < 1) {
                    ms = 1;
                }
                uint16_t sf = MACRO_MS((uint16_t)ms);
                if (sf == 0) {
                    sf = 1; // jamais 0 : sinon le tick ne débloque pas MP_DELAY
                }
                mp_arm(sf);
                mp_state = MP_DELAY;
            } else {
                mp_cur_key  = b0;
                mp_cur_mods = b1;
                if (b1 != 0) {
                    add_mods(b1);
                }
                add_key(b0);
                send_keyboard_report();
                mp_arm(MACRO_HOLD);
                mp_state = MP_HOLD;
            }
            break;
        }

        case MP_HOLD:
            macro_release_held(); // relâche la touche et les modificateurs, émet
            mp_arm(MACRO_GAP);
            mp_state = MP_NEXT;
            break;

        default: // MP_DELAY
            mp_state = MP_NEXT;
            mp_due   = 1; // enchaîner le step suivant immédiatement
            break;
    }
}

/*
 * ISR USB (usb_ep0_out_irq) : recopie dans le staging et lève des drapeaux. Aucun
 * appel à une fonction du chemin boucle principale — le commit (validation,
 * écriture flash, reconstruction du bitmap) est fait par gm610_macro_task().
 */
void macro_hid_receive(const __xdata uint8_t *buf)
{
    const uint8_t op  = buf[1];
    const uint8_t arg = buf[2];

    if (op == MACRO_OP_BEGIN) {
        uint16_t len = (uint16_t)arg | ((uint16_t)buf[3] << 8);
        if (len > MACRO_BLOB_SIZE) {
            len = 0;
        }
        stg_len    = len;
        stg_active = (len != 0);
    } else if (op == MACRO_OP_DATA) {
        if (!stg_active) {
            return;
        }
        // arg * 5 sans multiplication logicielle (pas de __mulint dans l'ISR USB).
        const uint16_t base = ((uint16_t)arg << 2) + (uint16_t)arg;
        for (uint8_t i = 0; i < 5u; i++) {
            const uint16_t pos = base + i;
            if (pos < stg_len) {
                staging[pos] = buf[3u + i];
            }
        }
    } else if (op == MACRO_OP_COMMIT) {
        if (!stg_active) {
            return;
        }
        commit_sum     = arg;
        commit_len     = stg_len;
        stg_active     = 0;
        commit_pending = 1;
    } else if (op == MACRO_OP_READ_SEEK) {
        macro_read_off = arg;
    }
}

// GET_REPORT : renvoie [id, 7 octets du blob au curseur] et avance le curseur.
// Appelé depuis l'ISR USB — simple lecture + incrément, aucun appel profond.
void macro_hid_fill_get(__xdata uint8_t *buf)
{
    buf[0] = REPORT_ID_MACRO;
    for (uint8_t i = 0; i < 7u; i++) {
        const uint16_t pos = (uint16_t)macro_read_off + i;
        buf[1u + i] = (pos < MACRO_BLOB_SIZE) ? user_settings.macro_blob[pos] : 0;
    }
    macro_read_off = (uint8_t)(macro_read_off + 7u);
}

#endif // MACRO_STORE_ENABLE
