#include "indicators.h"
#include "sh68f90.h"
#include "pwm.h"
#include "kbdef.h"
#include "settings.h"
#include "led_effect.h"
#include "user_matrix.h"
#include "aula_rgb.h"

/*
 * Rétroéclairage de l'AULA F75.
 *
 * TOPOLOGIE. Les quinze colonnes de matrice servent de sélecteurs de
 * multiplexage LED, et les dix-huit canaux PWM sont les six lignes de LED
 * multipliées par R/G/B. C'est l'inverse du NuPhy Air60, où le PWM pilote les
 * colonnes. Le seul autre clavier de SMK bâti comme celui-ci est le
 * genesis-thor-300, qui partage lui aussi ses colonnes entre le balayage de
 * touches et l'affichage -- ce fichier suit sa structure, en remplaçant son
 * tramage de trames par du vrai PWM.
 *
 * ORDONNANCEMENT. `src/smk/tick.c` alterne un balayage de matrice et
 * `LED_SUBFRAMES_PER_SCAN` sous-trames LED, le tout dans l'ISR Timer2. On pose
 * ce nombre à `MATRIX_COLS`, donc un balayage LED complet entre deux balayages
 * de touches. Les deux ne peuvent pas se chevaucher : le courant des LED se
 * couple dans la détection de ligne, ce que `matrix.c` évite en encadrant son
 * balayage par `indicators_pwm_disable()` / `indicators_pwm_enable()`.
 *
 * BUDGET. Tout ce qui suit tourne dans l'ISR, avec un créneau de 400 µs
 * (`RELOAD_LED_SUBFRAME`). D'où **une seule** évaluation d'effet par
 * sous-trame : le genesis-thor-300 a mesuré que six -- une par ligne --
 * « does not fit and starves the USB interrupt ». Une trame d'animation
 * complète prend donc 6 x 15 = 90 sous-trames, soit six balayages de matrice.
 *
 * Rien de tout ceci n'a été exécuté : aucun firmware n'a jamais été flashé sur
 * l'appareil.
 */

#define LED_ROWS MATRIX_ROWS
#define LED_COLS MATRIX_COLS

#include LED_GEOMETRY_HEADER
_Static_assert(LED_GEOMETRY_ROWS == LED_ROWS && LED_GEOMETRY_COLS == LED_COLS,
               "la geometrie LED generee ne correspond pas a la matrice de touches");

/*
 * Diviseur d'horloge PWM : les trois bits bas de `PWM00CON`, que le firmware
 * d'usine pose à 0x89 en 0x68D3 -- soit PWM_MODE_ENABLE | PWM_SS | 0b001.
 */
#define LED_PWM_CLK_DIV 0b001
#define LED_PWM_MASTER  (uint8_t)(PWM_MODE_ENABLE | PWM_SS | LED_PWM_CLK_DIV)

/*
 * Luminosité. Le firmware d'usine garde un gain sur dix crans en CODE 0x2937
 * (00 08 10 18 20 28 32 3c 46 50) qu'il applique en multipliant puis en
 * divisant par 80. `led_effect_rgb()` attend au contraire un facteur sur 0-255
 * qu'il applique par un décalage de huit : la table ci-dessous est donc celle
 * d'usine déjà mise à l'échelle (gain x 255 / 80), pour ne pas payer une
 * division par cellule dans l'ISR.
 */
static const __code uint8_t led_brightness_gain[LED_BRIGHTNESS_LEVELS] = {
    0, 25, 51, 76, 102, 127, 159, 191, 223, 255,
};

/* Pas d'animation par trame, indexé par `user_settings.led_speed`. */
static const __code uint8_t led_speeds[] = {1, 2, 4, 8, 16};
#define LED_SPEED_LEVELS (sizeof(led_speeds))

#define LED_BRIGHTNESS_DEFAULT (LED_BRIGHTNESS_LEVELS - 1)
#define LED_SPEED_DEFAULT      2

static uint8_t led_col;   /* colonne affichée par la sous-trame courante */
static uint8_t led_phase; /* phase de l'animation */
static uint8_t regen_row; /* curseur de régénération, une cellule par sous-trame */
static uint8_t regen_col;

/* ------------------------------------------------------------------ sortie */

void indicators_pwm_enable(void)
{
    /*
     * Le canal 0 de chaque banc porte l'activation et le diviseur ; les cinq
     * autres ne portent que PWM_SS. Même découpage que les trois autres
     * claviers de SMK.
     */
    PWM00CON = LED_PWM_MASTER;
    PWM01CON = PWM_SS;
    PWM02CON = PWM_SS;
    PWM03CON = PWM_SS;
    PWM04CON = PWM_SS;
    PWM05CON = PWM_SS;

    PWM10CON = LED_PWM_MASTER;
    PWM11CON = PWM_SS;
    PWM12CON = PWM_SS;
    PWM13CON = PWM_SS;
    PWM14CON = PWM_SS;
    PWM15CON = PWM_SS;

    PWM20CON = LED_PWM_MASTER;
    PWM21CON = PWM_SS;
    PWM22CON = PWM_SS;
    PWM23CON = PWM_SS;
    PWM24CON = PWM_SS;
    PWM25CON = PWM_SS;
}

/*
 * Extinction complète, et c'est bien deux choses : parquer les bancs PWM ne
 * suffit pas, parce qu'une broche parquée retombe sur la valeur du verrou de
 * port. C'est le RELÂCHEMENT DES COLONNES qui coupe réellement le courant --
 * plus aucune colonne sélectionnée, donc plus de chemin.
 *
 * Appelée depuis trois endroits qui n'ont rien à voir entre eux : `matrix.c`
 * avant chaque balayage, `sleep.c` avant la mise en veille, et
 * `settings_save_pre()` autour de l'effacement de page flash. Les trois veulent
 * la même chose : que le panneau soit noir et les colonnes libres.
 */
void indicators_pwm_disable(void)
{
    PWM00CON = PWM_CON_PARKED;
    PWM10CON = PWM_CON_PARKED;
    PWM20CON = PWM_CON_PARKED;

    user_matrix_cols_deselect_all();
}

/* --------------------------------------------------------------- animation */

/*
 * Une cellule par sous-trame. Le curseur balaie la grille en avançant la phase
 * à chaque tour complet, exactement comme le genesis-thor-300 : avancer la
 * phase par trame et non par sous-trame évite que baisser la luminosité ne
 * ralentisse aussi l'effet.
 */
static void led_regen_one(void)
{
    uint8_t rgb[3];

    if (led_effect_rgb((led_effect_t)user_settings.led_effect, regen_row, regen_col, led_phase,
                       led_brightness_gain[user_settings.led_brightness], rgb)) {
        aula_rgb_set(regen_row, regen_col, rgb[0], rgb[1], rgb[2]);
    }

    if (++regen_col >= LED_COLS) {
        regen_col = 0;
        if (++regen_row >= LED_ROWS) {
            regen_row = 0;
            led_phase = (uint8_t)(led_phase + led_speeds[user_settings.led_speed]);
        }
    }
}

void indicators_pre_update(void)
{
    /*
     * `matrix.c` laisse les colonnes en ENTRÉE derrière lui
     * (`user_matrix_scan_post`). Il faut les reprendre en sortie avant de
     * pouvoir en sélectionner une pour l'affichage.
     */
    user_matrix_scan_pre();
    indicators_pwm_disable();
}

bool indicators_update_step(keyboard_state_t *keyboard, uint8_t current_step)
{
    (void)keyboard;      /* les indicateurs d'état ne sont pas encore portés */
    (void)current_step;  /* `tick.c` passe toujours 0 */

    if (user_settings.led_effect < FX_OFF) {
        led_regen_one();

        /* Charger les dix-huit rapports cycliques PENDANT que les bancs sont
         * parqués, puis sélectionner la colonne, puis seulement rallumer. */
        aula_rgb_load_column(led_col);
        user_matrix_col_select(led_col);
        indicators_pwm_enable();
    }

    if (++led_col >= LED_COLS) {
        led_col = 0;
        return true; /* trame bouclée -- c'est ce que `sleep.c` compte */
    }
    return false;
}

void indicators_post_update(void)
{
}

/* ------------------------------------------------------------- réglages */

void indicators_apply_defaults(void)
{
    user_settings.led_effect     = FX_RADIAL;
    user_settings.led_brightness = LED_BRIGHTNESS_DEFAULT;
    user_settings.led_speed      = LED_SPEED_DEFAULT;
}

void indicators_validate_settings(void)
{
    if (user_settings.led_effect > FX_OFF) {
        user_settings.led_effect = FX_RADIAL;
    }
    if (user_settings.led_brightness >= LED_BRIGHTNESS_LEVELS) {
        user_settings.led_brightness = LED_BRIGHTNESS_DEFAULT;
    }
    if (user_settings.led_speed >= LED_SPEED_LEVELS) {
        user_settings.led_speed = LED_SPEED_DEFAULT;
    }
}

void indicators_init(void)
{
    led_col   = 0;
    led_phase = 0;
    regen_row = 0;
    regen_col = 0;

    aula_rgb_clear();
}

void indicators_start(void)
{
    indicators_validate_settings();
}

/* ------------------------------------------------------- actions clavier */

void indicators_next_effect(void)
{
    if (++user_settings.led_effect > FX_OFF) {
        user_settings.led_effect = 0;
    }
    aula_rgb_clear(); /* l'effet précédent laisserait ses pixels derrière lui */
    settings_mark_dirty();
}

void indicators_prev_effect(void)
{
    if (user_settings.led_effect == 0) {
        user_settings.led_effect = FX_OFF;
    } else {
        user_settings.led_effect--;
    }
    aula_rgb_clear();
    settings_mark_dirty();
}

void indicators_brightness_up(void)
{
    if (user_settings.led_brightness + 1u >= LED_BRIGHTNESS_LEVELS) {
        return;
    }
    user_settings.led_brightness++;
    settings_mark_dirty();
}

void indicators_brightness_down(void)
{
    if (user_settings.led_brightness == 0) {
        return;
    }
    user_settings.led_brightness--;
    settings_mark_dirty();
}

void indicators_speed_up(void)
{
    if (user_settings.led_speed + 1u >= LED_SPEED_LEVELS) {
        return;
    }
    user_settings.led_speed++;
    settings_mark_dirty();
}

void indicators_speed_down(void)
{
    if (user_settings.led_speed == 0) {
        return;
    }
    user_settings.led_speed--;
    settings_mark_dirty();
}
