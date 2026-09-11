#include "aula_macro.h"
#include "kbdef.h"
#include "keycodes.h"
#include "report.h"
#include "sh68f90.h"
#include "sleep.h"
#include <stdint.h>

/*
 * ATTENTE DU RELÂCHEMENT COMPLET -- ce n'est pas du confort.
 *
 * `Fn` est une touche `MO()` : `process_key_state()` (src/smk/matrix.c) la
 * traite AVANT d'appeler `kb_process_record()` et retourne tôt, donc notre
 * crochet ne la voit jamais. Pire, son relâchement appelle `clear_keys()`, qui
 * vide le rapport clavier sans l'émettre. Si la machine démarrait à
 * l'activation, la séquence `Fn v  Espace v  Espace ^  Fn ^` viderait le
 * rapport en plein milieu de la première lettre.
 *
 * D'où l'état MACRO_WAIT, traversé DEUX fois : avant la première frappe, et
 * après une annulation. Il avale tout jusqu'à ce que la matrice soit vide.
 *
 * `user_matrix_pressed()` (kbdef.h) donne cet état brut par colonne, déjà
 * masqué aux six lignes réelles. Il voit `Fn`, contrairement au crochet : c'est
 * lui qui rattrape le cas « l'utilisateur n'appuie que sur Fn » pendant que le
 * mode tourne.
 */
#define MACRO_OFF  0
#define MACRO_WAIT 1
#define MACRO_RUN  2

#define STEP_KEY_DOWN 0
#define STEP_KEY_UP   1
#define STEP_BSP_DOWN 2
#define STEP_BSP_UP   3

/*
 * Il n'existe pas de compteur de millisecondes libre sur ce firmware. La
 * sous-trame LED est la seule cadence régulière : `src/smk/tick.c` alterne un
 * balayage de matrice et quinze sous-trames, ce qui amortit le créneau de
 * 400 µs à ~420 µs. C'est la base de temps que suit déjà `aula_encoder.c`.
 *
 * L'intermédiaire en `uint32_t` est délibéré : `1500 * 1000` déborde un
 * `uint16_t`, et une cadence plus lente demandée un jour déborderait sans
 * bruit. Une seconde vaut ~2 381 sous-trames, donc compteur 16 bits obligé.
 */
#define MACRO_MS(ms) ((uint16_t)(((uint32_t)(ms) * 1000ul) / 420ul))

/*
 * L'hôte sonde l'endpoint toutes les millisecondes (`bInterval = 1`). Un appui
 * plus court qu'un sondage serait écrasé par son propre relâchement et ne
 * serait JAMAIS vu -- le piège de tout évènement synthétique, qui n'a pas la
 * durée naturelle d'un doigt sur une touche. Huit sous-trames font ~3,4 ms,
 * soit trois sondages au minimum.
 */
#define MACRO_HOLD 8

#define MACRO_MIN  MACRO_MS(500)
#define MACRO_SPAN MACRO_MS(1000)

/*
 * Garde-fou de MACRO_WAIT. Une touche restée collée rendrait `matrix_busy()`
 * vrai pour toujours : l'état ne déboucherait jamais, et comme il avale TOUT,
 * le clavier n'écrirait plus une seule lettre jusqu'au redémarrage. Au bout de
 * cinq secondes on retombe au repos, quoi qu'il arrive.
 *
 * Retomber au repos plutôt que démarrer : avec une touche collée, démarrer
 * enchaînerait annulation et redémarrage sans fin. Contrepartie assumée --
 * tenir la combinaison d'activation plus de cinq secondes ne lance rien.
 */
#define MACRO_WAIT_MAX MACRO_MS(5000)

/* Décrémenté par l'ISR, armé par la boucle principale.
 *
 * En XDATA et non en RAM interne : cette carte n'a que dix-sept octets de marge
 * interne contre plus de deux mille en XRAM, et l'ISR y accède déjà en
 * permanence pour le rendu. Le surcoût est une décroissance 16 bits par
 * sous-trame, soit ~1 µs sur un créneau de 400. Même arbitrage que pour
 * `enc_state`/`enc_hist` dans `aula_encoder.c`. */
static volatile __xdata uint16_t macro_wait;

/* Le tick lève ce bit plutôt que de laisser la boucle lire le compteur : un
 * test de bit est atomique sur 8051, une lecture 16 bits ne l'est pas. */
static volatile __bit macro_due;

static __xdata uint8_t  macro_state;
static __xdata uint8_t  macro_step;
static __xdata uint8_t  macro_letter;
static __xdata uint16_t macro_rnd;

/* MACRO_WAIT sert deux fois ; ce bit dit où il débouche. */
static __bit macro_go_after_wait;

void aula_macro_tick(void)
{
    if (macro_wait != 0) {
        macro_wait--;
        if (macro_wait == 0) {
            macro_due = 1;
        }
    }
}

bool aula_macro_active(void)
{
    return macro_state == MACRO_RUN;
}

static void macro_arm_timer(uint16_t subframes)
{
    ET2        = 0;
    macro_wait = subframes;
    macro_due  = 0;
    ET2        = 1;
}

static uint16_t macro_rand(void)
{
    macro_rnd ^= (uint16_t)(macro_rnd << 7);
    macro_rnd ^= (uint16_t)(macro_rnd >> 9);
    macro_rnd ^= (uint16_t)(macro_rnd << 8);
    return macro_rnd;
}

static bool matrix_busy(void)
{
    uint8_t col = MATRIX_COLS;

    while (col-- != 0) {
        if (user_matrix_pressed(col) != 0) {
            return true;
        }
    }
    return false;
}

static void macro_release_all(void)
{
    /* Sans ça, la touche en cours reste enfoncée côté hôte : il n'y a pas de
     * doigt pour la relever. */
    if (macro_letter != 0) {
        del_key(macro_letter);
        macro_letter = 0;
    }
    del_key((uint8_t)KC_BACKSPACE);
    send_keyboard_report();
}

static void macro_cancel(void)
{
    macro_release_all();
    macro_arm_timer(MACRO_WAIT_MAX);
    macro_state         = MACRO_WAIT;
    macro_go_after_wait = 0;
}

void aula_macro_arm(void)
{
    if (macro_state != MACRO_OFF) {
        return;
    }

    /* Semence prise sur Timer2 en vol. L'instant d'activation est décidé par un
     * humain : c'est la seule entropie de ce MCU, et elle suffit largement à un
     * tirage qui n'a aucune exigence cryptographique. */
    macro_rnd = (uint16_t)(((uint16_t)TH2 << 8) | TL2);
    if (macro_rnd == 0) {
        macro_rnd = 0xA5A5u;
    }

    macro_letter        = 0;
    macro_state         = MACRO_WAIT;
    macro_go_after_wait = 1;
    macro_arm_timer(MACRO_WAIT_MAX);
}

bool aula_macro_intercept(bool pressed)
{
    if (macro_state == MACRO_OFF) {
        return false;
    }
    if (macro_state == MACRO_RUN && pressed) {
        macro_cancel();
    }
    return true;
}

void aula_macro_task(void)
{
    if (macro_state == MACRO_OFF) {
        return;
    }

    if (macro_state == MACRO_WAIT) {
        if (matrix_busy()) {
            if (!macro_due) {
                return;
            }
            macro_state = MACRO_OFF; /* touche collée : voir MACRO_WAIT_MAX */
            return;
        }
        if (macro_go_after_wait) {
            macro_state = MACRO_RUN;
            macro_step  = STEP_KEY_DOWN;
            macro_due   = 1; /* première frappe sans attendre */
        } else {
            macro_state = MACRO_OFF;
        }
        return;
    }

    /* `Fn` seule ne passe pas par `kb_process_record` : c'est ici qu'elle est
     * rattrapée, avec toute autre touche que le crochet aurait manquée. */
    if (matrix_busy()) {
        macro_cancel();
        return;
    }

    if (!macro_due) {
        return;
    }
    macro_due = 0;

    /* Sans ça, le mode s'arrêterait tout seul au bout d'environ 2 min 15 s en
     * sans-fil : `sleep.c` endort après 21 000 trames et une trame vaut quinze
     * sous-trames plus un balayage, soit ~6,3 ms -- PAS les quatre-vingt-dix
     * sous-trames d'une trame d'animation. Contrepartie assumée : tant que le
     * mode tourne, le clavier ne dort plus et vide sa batterie. */
    sleep_note_activity();

    switch (macro_step) {
        case STEP_KEY_DOWN:
            macro_letter = (uint8_t)((uint8_t)KC_A + (uint8_t)(macro_rand() % 26u));
            add_key(macro_letter);
            send_keyboard_report();
            macro_arm_timer(MACRO_HOLD);
            macro_step = STEP_KEY_UP;
            break;

        case STEP_KEY_UP:
            del_key(macro_letter);
            macro_letter = 0;
            send_keyboard_report();
            macro_arm_timer((uint16_t)(MACRO_MIN + (macro_rand() % MACRO_SPAN)));
            macro_step = STEP_BSP_DOWN;
            break;

        case STEP_BSP_DOWN:
            add_key((uint8_t)KC_BACKSPACE);
            send_keyboard_report();
            macro_arm_timer(MACRO_HOLD);
            macro_step = STEP_BSP_UP;
            break;

        default:
            del_key((uint8_t)KC_BACKSPACE);
            send_keyboard_report();
            macro_arm_timer((uint16_t)(MACRO_MIN + (macro_rand() % MACRO_SPAN)));
            macro_step = STEP_KEY_DOWN;
            break;
    }
}
