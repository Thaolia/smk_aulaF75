#include "indicators.h"
#include "kbdef.h"
#include "gpio.h"
#include "pwm.h"
#include "settings.h"
#include "keyboard.h"
#include "led_effect.h"
#include "user_matrix.h"
#include "debug.h"
#include "usb.h"
#ifdef RF_ENABLED
#    include "rf_controller.h"
#endif
#include <string.h>

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * RGB du GM610 -- deux effets, et un mode de diagnostic
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Topologie (relevée dans le firmware d'usine, cf. docs/GM610_SYNTHESE.md) :
 * les 14 colonnes sont des sorties PWM, une par colonne ; les puits sont
 * 5 rangées x 3 couleurs = 15 broches GPIO. Une sous-trame allume une paire
 * (rangée, couleur) en chargeant les 14 rapports cycliques puis en levant son
 * puits. 15 sous-trames font une trame.
 *
 * Le firmware d'usine fait exactement cela, en 18 sous-trames (6 rangées) :
 * sa rangée LED supplémentaire n'a aucune touche sur ce modèle.
 *
 * ⚠️ L'affectation rangée/couleur des puits est INFÉRÉE pour 12 des 15 broches
 * (les 3 de la rangée 4 sont établies). Le mode diagnostic -- Fn + D -- allume
 * une paire à la fois et l'annonce sur la console, pour la corriger d'après ce
 * qui s'allume vraiment.
 */

#define LED_ROWS MATRIX_ROWS
#define LED_COLS MATRIX_COLS

// Effets. L'ordre est celui du cycle de Fn + \.
#define GM_FX_BEAT  ((uint8_t)0) // battement : le clavier bat comme un coeur
#define GM_FX_LAKE  ((uint8_t)1) // goutte d'eau : une onde part de chaque frappe
#define GM_FX_COUNT ((uint8_t)2)
#define GM_FX_OFF   GM_FX_COUNT

#define LED_SPEED_DEFAULT 10
#define LED_SPEED_MIN     1
#define LED_SPEED_MAX     16

#define LED_BRIGHTNESS_DEFAULT 255
#define LED_BRIGHTNESS_STEP    32

/*
 * Tampon 8 bits -> DUTY2. DUTY1 reste à 0 : la sortie passe BAS au début de la
 * période, DUTY2 la ramène HAUT ; la LED conduit pendant l'état bas, donc le
 * temps allumé vaut DUTY2. Ne PAS inverser -- mettre 0xFF pour « éteint » fait
 * briller les sous-trames noires à pleine puissance.
 */
#define LED_DUTY(v) (uint16_t)(v)

/*
 * ⚠️ PLAFOND DE DÉMARRAGE -- corrige un vrai échec d'énumération.
 *
 * Avant d'être énuméré, un périphérique USB n'a droit qu'à **100 mA**. Ce
 * clavier allume 63 touches en RGB, et ses code options autorisent 380 mA de
 * puits sur Port6 : à pleine luminosité dès le démarrage, il dépasse le budget
 * et l'hôte échoue -- observé sur l'appareil, Windows rendant successivement
 * « échec de demande de descripteur de périphérique » puis « ...de
 * configuration », les deux signatures d'une alimentation qui s'effondre au
 * milieu d'un transfert.
 *
 * On reste donc au quart de la luminosité jusqu'à ce que l'USB soit configuré.
 * Le délai de repli sert au fonctionnement sur batterie, où l'USB ne sera
 * jamais configuré et où le budget des 100 mA ne s'applique pas.
 */
#define BOOT_LIMIT_BRI    64
#define BOOT_LIMIT_FRAMES 400 // ~6 s à ~66 trames/s

/*
 * Battement de référence, pour tout ce qui doit mesurer du TEMPS.
 *
 * `fx_tick()` est appelé depuis l'ISR systick : sous-trame LED de 400 µs et scan
 * matrice de 100 µs alternés, donc 2 000 sous-trames/s, et une trame = 15
 * sous-trames -> 133 trames/s. On divise par 4 : un battement toutes les 30 ms,
 * qui tient dans un octet pour trois secondes (100 battements).
 *
 * ⚠️ Pourquoi pas la boucle principale : elle N'A PAS de cadence. Mesurée à
 * ~2 kHz en USB au repos, elle s'effondre en Bluetooth non connecté -- la radio
 * retente sa liaison et le témoin fait repeindre l'image. Un maintien compté en
 * tours de boucle devenait alors interminable, et la porte de secours `Fn + B`
 * ne répondait plus dans l'état où elle sert justement.
 */
volatile uint8_t indicators_ticks;

static uint8_t  boot_limit = BOOT_LIMIT_BRI;
static uint16_t boot_frames;

#define SCALE_BRI(v)                                                                   \
    (uint8_t)(((uint16_t)(uint8_t)(v) *                                                \
               ((user_settings.led_brightness < boot_limit) ? user_settings.led_brightness \
                                                            : boot_limit)) >>          \
              8)

static __xdata uint8_t led_fb[LED_ROWS][3][LED_COLS];

static uint8_t led_row;   // rangée en cours d'affichage
static uint8_t led_color; // 0 = R, 1 = V, 2 = B
static uint8_t led_phase;
static uint8_t speed_acc;

static uint8_t regen_row;
static uint8_t regen_col;
static volatile bool render_dirty;

/* ── Détection de frappe pour l'effet réactif ─────────────────────────────
 * Une colonne examinée par sous-trame : chaque colonne passe une fois par
 * trame, exactement la cadence à laquelle `matrix.c` la rafraîchit. Seuls les
 * fronts comptent -- une touche maintenue ne rejette pas de goutte.
 */
static uint8_t        poll_col;
static __xdata uint8_t poll_prev[LED_COLS];

/* ── L'effet « battement » ────────────────────────────────────────────────
 * Deux systoles rapprochées puis une longue diastole : c'est un battement de
 * coeur, pas une respiration. La teinte glisse d'une pulsation à l'autre.
 */
#define BEAT_SIZE 64
static __code const uint8_t beat_lut[BEAT_SIZE] = {
     20,  67, 114, 161, 208, 255, 255, 231, 207, 182, 158, 134, 109,  85,  60,  88,
    116, 144, 172, 200, 177, 154, 131, 108,  85,  62,  39,  15,  15,  16,  17,  18,
     19,  19,  20,  21,  21,  22,  23,  23,  23,  24,  24,  24,  24,  24,  24,  24,
     24,  24,  24,  23,  23,  23,  22,  21,  21,  20,  19,  19,  18,  17,  16,  15,
};

/* ── L'effet « goutte d'eau sur le lac » ──────────────────────────────────
 * Chaque frappe jette une goutte ; une couronne s'en éloigne d'un cran par
 * trame, sa teinte glissant avec la distance, jusqu'à quitter le clavier.
 */
#define LAKE_DROPS    4
#define LAKE_HUE_STEP 6
#define LAKE_BASE_B   18 // le lac au repos : un bleu sombre

static __xdata uint8_t drop_col[LAKE_DROPS];
static __xdata uint8_t drop_row[LAKE_DROPS];
static __xdata uint8_t drop_age[LAKE_DROPS]; // 0 = emplacement libre
static __xdata uint8_t drop_hue[LAKE_DROPS];

/*
 * Distance en « crans de couronne ». Approximation octogonale de la distance
 * euclidienne -- max + moitié du min -- parce que sur un clavier une rangée et
 * une colonne font le même pas physique (~19 mm) : les couronnes doivent être
 * rondes, pas des losanges.
 *
 * Maximum sur cette grille : 13 + 4/2 = 15, d'où l'âge limite à 17.
 */
#define LAKE_MAX_AGE 17

static uint8_t lake_dist(uint8_t col, uint8_t row, uint8_t c0, uint8_t r0)
{
    const uint8_t dc = (col > c0) ? (uint8_t)(col - c0) : (uint8_t)(c0 - col);
    const uint8_t dr = (row > r0) ? (uint8_t)(row - r0) : (uint8_t)(r0 - row);

    return (dc > dr) ? (uint8_t)(dc + (dr >> 1)) : (uint8_t)(dr + (dc >> 1));
}

static void lake_drop(uint8_t col, uint8_t row, uint8_t hue)
{
    uint8_t i, pick = 0, oldest = 0;

    for (i = 0; i < LAKE_DROPS; i++) {
        if (drop_age[i] == 0) {
            pick = i;
            break;
        }
        if (drop_age[i] > oldest) {
            oldest = drop_age[i];
            pick   = i;
        }
    }
    drop_col[pick] = col;
    drop_row[pick] = row;
    drop_hue[pick] = hue;
    drop_age[pick] = 1;
}

/* ── Le mode diagnostic ───────────────────────────────────────────────────
 * Il allume UNE paire (rangée, couleur) à la fois, toutes colonnes à fond, et
 * l'annonce sur la console. Il sert à corriger la carte des puits : dire ce
 * qui s'allume vraiment à chaque pas suffit à la refaire.
 */
static bool    diag_on;
static uint8_t diag_step;  // 0..(LED_ROWS*3 - 1)
static uint8_t diag_ticks;

/*
 * La broche derrière chaque pas, en (port << 4) | bit. Le diagnostic l'annonce
 * telle quelle : dire « le pas P0.2 a allumé la rangée des majuscules en
 * rouge » fixe directement broche <-> position, sans passer par deux couches
 * d'inférence (index de rangée, puis affectation de couleur).
 */
static __code const uint8_t diag_pin[LED_ROWS * 3] = {
    0x04, 0x61, 0x03, // L0 : R P0.4  V P6.1  B P0.3   <- vérifié sur l'appareil
    0x67, 0x62, 0x66, // L1 : R P6.7  V P6.2  B P6.6
    0x02, 0x63, 0x57, // L2 : R P0.2  V P6.3  B P5.7
    0x45, 0x64, 0x46, // L3 : R P4.5  V P6.4  B P4.6
    0x44, 0x65, 0x43, // L4 : R P4.4  V P6.5  B P4.3
};

static void diag_announce(void)
{
    const uint8_t pin = diag_pin[diag_step];

    dprintf("diag L%u %c  P%u.%u\r\n", (unsigned)(diag_step / 3),
            (unsigned)"RGB"[diag_step % 3], (unsigned)(pin >> 4), (unsigned)(pin & 7));
}
#define DIAG_HOLD 90 // trames par pas, ~1,5 s

void indicators_toggle_diag(void)
{
    diag_on    = !diag_on;
    diag_step  = 0;
    diag_ticks = 0;
    if (diag_on) {
        dprintf("diag on\r\n");
        diag_announce();
    } else {
        dprintf("diag off\r\n");
    }
    render_dirty = true;
}

/* ── L'indicateur de liaison ──────────────────────────────────────────────
 *
 * Le firmware d'usine conditionne le témoin de la touche Tab à un bit d'état
 * dans son moteur d'effets -- d'où le clignotement bleu/rouge décrit dans sa
 * notice. On reprend l'idée : la touche qui SÉLECTIONNE la liaison courante
 * porte son état.
 *
 *   USB          Tab, blanc fixe
 *   Bluetooth    Q / W / E, bleu
 *   2,4 GHz      G, vert
 *
 *   pas appairé      clignotement rapide
 *   appairé, absent  respiration
 *   connecté         fixe
 *
 * Et tout changement de mode illumine BRIÈVEMENT LE CLAVIER ENTIER de la
 * couleur de la nouvelle liaison : un basculement ne doit jamais être ambigu,
 * surtout sur un clavier sans interrupteur où il se déclenche au maintien.
 */
#ifdef RF_ENABLED

#    define LINK_FLASH_FRAMES 45 // ~0,7 s

static uint8_t link_flash;
static uint8_t link_pulse;

void indicators_link_flash(void)
{
    link_flash   = LINK_FLASH_FRAMES;
    render_dirty = true;
}

// Position du sélecteur de la liaison courante, sur la couche de base.
static void link_selector(uint8_t *row, uint8_t *col)
{
    if (kb_conn_mode() == KB_CONN_USB) {
        *row = 1; *col = 0; // Tab
        return;
    }
    switch (keyboard_state.rf_link) {
        case RF_MODE_BT1: *row = 1; *col = 1; break; // Q
        case RF_MODE_BT2: *row = 1; *col = 2; break; // W
        case RF_MODE_BT3: *row = 1; *col = 3; break; // E
        default:          *row = 2; *col = 5; break; // G, 2,4 GHz
    }
}

static void link_color(uint8_t out[3])
{
    if (kb_conn_mode() == KB_CONN_USB) {
        out[0] = 255; out[1] = 255; out[2] = 255; // blanc
    } else if (keyboard_state.rf_link == RF_MODE_2_4G) {
        out[0] = 0;   out[1] = 255; out[2] = 40;  // vert
    } else {
        out[0] = 0;   out[1] = 90;  out[2] = 255; // bleu
    }
}

// 0..255 : la modulation qui dit l'état du lien.
static uint8_t link_scale(void)
{
    if (kb_conn_mode() == KB_CONN_USB) {
        return 255; // l'USB est là ou le clavier ne tourne pas
    }
    if (!keyboard_state.paired) {
        return (uint8_t)((link_pulse & 0x08) ? 255 : 0); // clignotement rapide
    }
    if (!keyboard_state.connected) {
        return beat_lut[(uint8_t)(link_pulse & (BEAT_SIZE - 1))]; // respiration
    }
    return 255;
}

#else
void indicators_link_flash(void) {}
#endif // RF_ENABLED

/* ── Réglages ─────────────────────────────────────────────────────────────*/

void indicators_apply_defaults(void)
{
    user_settings.led_effect     = GM_FX_LAKE;
    user_settings.led_brightness = LED_BRIGHTNESS_DEFAULT;
    user_settings.led_speed      = LED_SPEED_DEFAULT;
    /*
     * ⚠️ `ul_effect` n'est PAS un effet ici : le GM610 n'a pas d'underglow, et
     * kb.c s'en sert pour persister le mode de liaison (KB_CONN_USB /
     * KB_CONN_RF). On pose donc l'USB, pas un indice d'effet -- sans quoi
     * l'étage d'éclairage déciderait du mode de liaison par accident.
     */
    user_settings.ul_effect      = KB_CONN_USB;
    user_settings.ul_brightness  = 0;
    user_settings.ul_speed       = LED_SPEED_DEFAULT;
}

void indicators_validate_settings(void)
{
    /*
     * ⚠️ Un enregistrement entièrement nul, c'est de la NVM jamais écrite, pas
     * un réglage choisi -- et `settings_load()` le rend quand même vrai, donc
     * `main.c` n'applique PAS les valeurs par défaut. Mesuré sur cet appareil :
     * la console du build précédent sortait `settings le=00 lb=00 ls=00`.
     * Sans ce garde-fou la luminosité vaudrait 0 et le clavier paraîtrait mort
     * alors que tout fonctionne -- on chercherait la panne dans le brochage.
     *
     * Le test porte sur DEUX champs : baisser la luminosité à zéro est un choix
     * légitime, mais il laisse toujours une vitesse non nulle.
     */
    if (user_settings.led_brightness == 0 && user_settings.led_speed == 0) {
        indicators_apply_defaults();
    }

    if (user_settings.led_effect > GM_FX_OFF) {
        user_settings.led_effect = GM_FX_OFF;
    }
    if (user_settings.led_speed < LED_SPEED_MIN) {
        user_settings.led_speed = LED_SPEED_MIN;
    }
    if (user_settings.led_speed > LED_SPEED_MAX) {
        user_settings.led_speed = LED_SPEED_MAX;
    }
}

void indicators_next_effect(void)
{
    if (++user_settings.led_effect > GM_FX_OFF) {
        user_settings.led_effect = 0;
    }
    for (uint8_t i = 0; i < LAKE_DROPS; i++) {
        drop_age[i] = 0;
    }
    led_phase    = 0;
    render_dirty = true;
    dprintf("fx %u\r\n", (unsigned)user_settings.led_effect);
    settings_mark_dirty();
}

void indicators_brightness_up(void)
{
    if (user_settings.led_brightness > (uint8_t)(255 - LED_BRIGHTNESS_STEP)) {
        user_settings.led_brightness = 255;
    } else {
        user_settings.led_brightness = (uint8_t)(user_settings.led_brightness + LED_BRIGHTNESS_STEP);
    }
    render_dirty = true;
    settings_mark_dirty();
}

void indicators_brightness_down(void)
{
    if (user_settings.led_brightness < LED_BRIGHTNESS_STEP) {
        user_settings.led_brightness = 0;
    } else {
        user_settings.led_brightness = (uint8_t)(user_settings.led_brightness - LED_BRIGHTNESS_STEP);
    }
    render_dirty = true;
    settings_mark_dirty();
}

void indicators_speed_up(void)
{
    if (user_settings.led_speed < LED_SPEED_MAX) {
        user_settings.led_speed++;
    }
    settings_mark_dirty();
}

void indicators_speed_down(void)
{
    if (user_settings.led_speed > LED_SPEED_MIN) {
        user_settings.led_speed--;
    }
    settings_mark_dirty();
}

/* ── Couche matérielle ────────────────────────────────────────────────────*/

void indicators_pre_update(void)
{
    // Tous les puits bas : aucune LED ne conduit pendant qu'on recharge.
    GPIO_LOW(0, RGB_P0_MASK);
    GPIO_LOW(4, RGB_P4_MASK);
    GPIO_LOW(5, RGB_P5_MASK);
    GPIO_LOW(6, RGB_P6_MASK);
}

/*
 * Extinction complète, avant de sauter dans le bootloader. Deux raisons :
 *   - le bootloader ne connaît pas ces broches et ne les remettrait jamais au
 *     repos, donc une rangée resterait allumée à plein pendant tout l'ISP ;
 *   - surtout, il doit énumérer sous les 100 mA autorisés avant énumération --
 *     et c'est précisément ce budget qui nous a déjà coûté un flash.
 */
void indicators_all_off(void)
{
    indicators_pwm_disable();
    GPIO_LOW(0, RGB_P0_MASK);
    GPIO_LOW(4, RGB_P4_MASK);
    GPIO_LOW(5, RGB_P5_MASK);
    GPIO_LOW(6, RGB_P6_MASK);
}

void indicators_post_update(void)
{
    PWM00CON &= (uint8_t)~(1 << 5);
}

void indicators_pwm_enable(void)
{
    PWM00CON = (uint8_t)(PWM_MODE_ENABLE | PWM_SS | PWM_CLK_DIV_4);
    PWM01CON = PWM_SS;
    PWM02CON = PWM_SS;
    PWM03CON = PWM_SS;
    PWM04CON = PWM_SS;
    PWM05CON = PWM_SS;

    PWM10CON = (uint8_t)(PWM_MODE_ENABLE | PWM_SS | PWM_CLK_DIV_4);
    PWM11CON = PWM_SS;
    PWM12CON = PWM_SS;
    PWM13CON = PWM_SS;
    PWM14CON = PWM_SS;
    PWM15CON = PWM_SS;

    PWM40CON = (uint8_t)(PWM_MODE_ENABLE | PWM_SS | PWM_CLK_DIV_4);
    PWM41CON = PWM_SS;
    PWM42CON = PWM_SS;
}

void indicators_pwm_disable(void)
{
    PWM00CON = PWM_CON_PARKED;
    PWM01CON = PWM_CON_PARKED;
    PWM02CON = PWM_CON_PARKED;
    PWM03CON = PWM_CON_PARKED;
    PWM04CON = PWM_CON_PARKED;
    PWM05CON = PWM_CON_PARKED;
    PWM10CON = PWM_CON_PARKED;
    PWM11CON = PWM_CON_PARKED;
    PWM12CON = PWM_CON_PARKED;
    PWM13CON = PWM_CON_PARKED;
    PWM14CON = PWM_CON_PARKED;
    PWM15CON = PWM_CON_PARKED;
    PWM40CON = PWM_CON_PARKED;
    PWM41CON = PWM_CON_PARKED;
    PWM42CON = PWM_CON_PARKED;
}

static void led_enable_sink(void)
{
    switch (led_row) {
        case 0:
            if (led_color == 0)      RGB_R0R = 1;
            else if (led_color == 1) RGB_R0G = 1;
            else                     RGB_R0B = 1;
            break;
        case 1:
            if (led_color == 0)      RGB_R1R = 1;
            else if (led_color == 1) RGB_R1G = 1;
            else                     RGB_R1B = 1;
            break;
        case 2:
            if (led_color == 0)      RGB_R2R = 1;
            else if (led_color == 1) RGB_R2G = 1;
            else                     RGB_R2B = 1;
            break;
        case 3:
            if (led_color == 0)      RGB_R3R = 1;
            else if (led_color == 1) RGB_R3G = 1;
            else                     RGB_R3B = 1;
            break;
        default:
            if (led_color == 0)      RGB_R4R = 1;
            else if (led_color == 1) RGB_R4G = 1;
            else                     RGB_R4B = 1;
            break;
    }
}

static bool led_set_columns(void)
{
    __xdata uint8_t *fb = led_fb[led_row][led_color];
    uint8_t          any = 0;

    SET_PWM_DUTY_2(LED_PWM_C0, LED_DUTY(fb[0]));   any = (uint8_t)(any | fb[0]);
    SET_PWM_DUTY_2(LED_PWM_C1, LED_DUTY(fb[1]));   any = (uint8_t)(any | fb[1]);
    SET_PWM_DUTY_2(LED_PWM_C2, LED_DUTY(fb[2]));   any = (uint8_t)(any | fb[2]);
    SET_PWM_DUTY_2(LED_PWM_C3, LED_DUTY(fb[3]));   any = (uint8_t)(any | fb[3]);
    SET_PWM_DUTY_2(LED_PWM_C4, LED_DUTY(fb[4]));   any = (uint8_t)(any | fb[4]);
    SET_PWM_DUTY_2(LED_PWM_C5, LED_DUTY(fb[5]));   any = (uint8_t)(any | fb[5]);
    SET_PWM_DUTY_2(LED_PWM_C6, LED_DUTY(fb[6]));   any = (uint8_t)(any | fb[6]);
    SET_PWM_DUTY_2(LED_PWM_C7, LED_DUTY(fb[7]));   any = (uint8_t)(any | fb[7]);
    SET_PWM_DUTY_2(LED_PWM_C8, LED_DUTY(fb[8]));   any = (uint8_t)(any | fb[8]);
    SET_PWM_DUTY_2(LED_PWM_C9, LED_DUTY(fb[9]));   any = (uint8_t)(any | fb[9]);
    SET_PWM_DUTY_2(LED_PWM_C10, LED_DUTY(fb[10])); any = (uint8_t)(any | fb[10]);
    SET_PWM_DUTY_2(LED_PWM_C11, LED_DUTY(fb[11])); any = (uint8_t)(any | fb[11]);
    SET_PWM_DUTY_2(LED_PWM_C12, LED_DUTY(fb[12])); any = (uint8_t)(any | fb[12]);
    SET_PWM_DUTY_2(LED_PWM_C13, LED_DUTY(fb[13])); any = (uint8_t)(any | fb[13]);

    return any != 0;
}

/* ── Génération d'une image ───────────────────────────────────────────────*/

static void led_regen_one(void)
{
    uint8_t rgb[3];
    uint8_t r = 0, g = 0, b = 0;

    if (diag_on) {
        const uint8_t on = (uint8_t)((regen_row == (uint8_t)(diag_step / 3)) ? 255 : 0);
        const uint8_t ci = (uint8_t)(diag_step % 3);
        led_fb[regen_row][0][regen_col] = (ci == 0) ? on : 0;
        led_fb[regen_row][1][regen_col] = (ci == 1) ? on : 0;
        led_fb[regen_row][2][regen_col] = (ci == 2) ? on : 0;
        goto next;
    }

    switch (user_settings.led_effect) {
        case GM_FX_BEAT: {
            // Tout le clavier bat ensemble ; la teinte dérive lentement.
            const uint8_t v = beat_lut[(uint8_t)(led_phase & (BEAT_SIZE - 1))];
            led_color_wheel((uint8_t)(led_phase >> 2), rgb);
            r = (uint8_t)(((uint16_t)rgb[0] * v) >> 8);
            g = (uint8_t)(((uint16_t)rgb[1] * v) >> 8);
            b = (uint8_t)(((uint16_t)rgb[2] * v) >> 8);
            break;
        }
        case GM_FX_LAKE: {
            b = LAKE_BASE_B; // le lac plat
            for (uint8_t i = 0; i < LAKE_DROPS; i++) {
                if (drop_age[i] == 0) {
                    continue;
                }
                const uint8_t d = lake_dist(regen_col, regen_row, drop_col[i], drop_row[i]);
                const uint8_t a = drop_age[i];
                uint8_t       w;
                if (d == a) {
                    w = 255; // la crête
                } else if (d + 1 == a || a + 1 == d) {
                    w = 96; // les flancs
                } else {
                    continue;
                }
                // L'onde s'éteint en s'éloignant du point d'impact.
                w = (uint8_t)(((uint16_t)w * (uint16_t)(LAKE_MAX_AGE - a)) / LAKE_MAX_AGE);
                led_color_wheel((uint8_t)(drop_hue[i] + (uint8_t)(a * LAKE_HUE_STEP)), rgb);
                const uint8_t nr = (uint8_t)(((uint16_t)rgb[0] * w) >> 8);
                const uint8_t ng = (uint8_t)(((uint16_t)rgb[1] * w) >> 8);
                const uint8_t nb = (uint8_t)(((uint16_t)rgb[2] * w) >> 8);
                if (nr > r) r = nr;
                if (ng > g) g = ng;
                if (nb > b) b = nb;
            }
            break;
        }
        default:
            break; // GM_FX_OFF : tout noir
    }

#ifdef RF_ENABLED
    {
        uint8_t lc[3];
        uint8_t srow, scol;

        if (link_flash) {
            // Annonce : tout le clavier prend la couleur de la nouvelle liaison.
            link_color(lc);
            r = lc[0];
            g = lc[1];
            b = lc[2];
        } else {
            link_selector(&srow, &scol);
            if (regen_row == srow && regen_col == scol) {
                const uint8_t k = link_scale();
                link_color(lc);
                r = (uint8_t)(((uint16_t)lc[0] * k) >> 8);
                g = (uint8_t)(((uint16_t)lc[1] * k) >> 8);
                b = (uint8_t)(((uint16_t)lc[2] * k) >> 8);
            }
        }
    }
#endif

    led_fb[regen_row][0][regen_col] = SCALE_BRI(r);
    led_fb[regen_row][1][regen_col] = SCALE_BRI(g);
    led_fb[regen_row][2][regen_col] = SCALE_BRI(b);

next:
    if (++regen_col >= LED_COLS) {
        regen_col = 0;
        if (++regen_row >= LED_ROWS) {
            regen_row = 0;
        }
    }
}

void indicators_render(void)
{
    if (!render_dirty) {
        return;
    }
    render_dirty = false; // remis à zéro d'abord : une avance de phase le réarme

    for (uint8_t i = 0; i < (uint8_t)(LED_ROWS * LED_COLS); i++) {
        led_regen_one();
    }
}

/* ── Cadence ──────────────────────────────────────────────────────────────*/

static void led_react_poll(void)
{
    const uint8_t now   = user_matrix_pressed(poll_col);
    const uint8_t fresh = (uint8_t)(now & (uint8_t)~poll_prev[poll_col]);

    poll_prev[poll_col] = now;

    if (fresh && user_settings.led_effect == GM_FX_LAKE) {
        for (uint8_t row = 0; row < LED_ROWS; row++) {
            if (fresh & (uint8_t)(1u << row)) {
                lake_drop(poll_col, row, led_phase);
            }
        }
    }

    if (++poll_col >= LED_COLS) {
        poll_col = 0;
    }
}

static void fx_tick(void)
{
    static uint8_t div4;

    if (++div4 >= 4) {
        div4 = 0;
        indicators_ticks++;
    }

    if (boot_limit != 255) {
        if (usb_is_configured() || ++boot_frames >= BOOT_LIMIT_FRAMES) {
            boot_limit   = 255;
            render_dirty = true;
            dprintf("bri released\r\n");
        }
    }

#ifdef RF_ENABLED
    // L'indicateur bat à la trame, indépendamment de la vitesse de l'effet :
    // un lien qui cherche son hôte doit clignoter à cadence lisible, pas à la
    // cadence que l'utilisateur a choisie pour son animation.
    link_pulse++;
    if (link_flash) {
        link_flash--;
        render_dirty = true;
    } else if (kb_conn_mode() == KB_CONN_RF && !keyboard_state.connected && div4 == 0) {
        /*
         * Le témoin clignote ou respire, donc il faut repeindre -- mais une fois
         * sur quatre suffit (~33 Hz, l'œil n'y voit rien) là où chaque trame
         * régénérait les 70 cases et écrasait la boucle principale.
         */
        render_dirty = true;
    }
    // Sinon on laisse la cadence de l'effet décider : repeindre les 70 cases à
    // chaque trame pour une seule touche fixe affamerait la boucle principale.
#endif

    if (diag_on) {
        if (++diag_ticks >= DIAG_HOLD) {
            diag_ticks = 0;
            if (++diag_step >= (uint8_t)(LED_ROWS * 3)) {
                diag_step = 0;
            }
            diag_announce();
            render_dirty = true;
        }
        return;
    }

    /*
     * Vitesse : 1 = le plus lent (une avance toutes les 16 trames), 16 = chaque
     * trame. Le battement a son propre diviseur, quatre fois plus court : sa
     * courbe fait 64 pas, et au pas commun un cycle cardiaque durerait près de
     * dix secondes -- ce ne serait plus un battement.
     */
    uint8_t div = (uint8_t)(LED_SPEED_MAX + 1 - user_settings.led_speed);
    if (user_settings.led_effect == GM_FX_BEAT) {
        div = (uint8_t)(div >> 2);
        if (div == 0) {
            div = 1;
        }
    }
    if (++speed_acc < div) {
        return;
    }
    speed_acc = 0;
    led_phase++;

    if (user_settings.led_effect == GM_FX_LAKE) {
        for (uint8_t i = 0; i < LAKE_DROPS; i++) {
            if (drop_age[i] != 0 && ++drop_age[i] >= LAKE_MAX_AGE) {
                drop_age[i] = 0; // l'onde a quitté le clavier
            }
        }
    }
    render_dirty = true;
}

bool indicators_update_step(keyboard_state_t *keyboard, uint8_t current_step)
{
    (void)keyboard;
    (void)current_step;

    indicators_pwm_disable();

    if (led_set_columns()) {
        led_enable_sink();
        indicators_pwm_enable();
    }

    led_react_poll();

    bool wrapped = false;
    if (++led_color >= 3) {
        led_color = 0;
        if (++led_row >= LED_ROWS) {
            led_row = 0;
            wrapped = true;
            fx_tick();
        }
    }
    return wrapped;
}

/* ── Démarrage ────────────────────────────────────────────────────────────*/

void indicators_init(void)
{
    memset(led_fb, 0, sizeof(led_fb));
    memset(poll_prev, 0, sizeof(poll_prev));
    for (uint8_t i = 0; i < LAKE_DROPS; i++) {
        drop_age[i] = 0;
    }
}

void indicators_start(void)
{
    led_row      = 0;
    led_color    = 0;
    led_phase    = 0;
    speed_acc    = 0;
    regen_row    = 0;
    regen_col    = 0;
    poll_col     = 0;
    render_dirty = true;

    indicators_pwm_enable();
}
