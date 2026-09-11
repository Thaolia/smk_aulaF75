#include "indicators.h"
#include "sh68f90.h"
#include "pwm.h"
#include "kbdef.h"
#include "settings.h"
#include "tick.h"
#include "led_effect.h"
#include "user_matrix.h"
#include "aula_rgb.h"
#include "aula_fx.h"

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

/*
 * Les effets. Les quatre premiers viennent de `led_effect.c`, partagé ; on lui
 * emprunte sa GÉOMÉTRIE (`led_effect_index`) mais pas ses couleurs, qui sont une
 * interpolation linéaire sur trois secteurs. La teinte vient de la roue d'usine,
 * dont les rampes sont perceptuelles.
 *
 * Le cinquième est propre à ce clavier : le moteur réactif du firmware d'usine
 * (`fcn.000064F1`, l'effet d'indice 12), où chaque touche porte sa couleur et
 * son intensité, et où l'intensité décroît d'une trame à l'autre. C'est l'effet
 * le plus caractéristique du F75, et le seul qui demande de savoir QUELLE touche
 * a été frappée.
 */
#define AULA_FX_REACTIVE  ((uint8_t)(FX_COUNT + 0)) /*  4  0x64F1 */
#define AULA_FX_RAIN      ((uint8_t)(FX_COUNT + 1)) /*  5  0x9B2B */
#define AULA_FX_TWINKLE   ((uint8_t)(FX_COUNT + 2)) /*  6  0xAC1C -> 0x4EA9 */
#define AULA_FX_SNAKE     ((uint8_t)(FX_COUNT + 3)) /*  7  0x8DDF */
#define AULA_FX_SNAKE_RGB ((uint8_t)(FX_COUNT + 4)) /*  8  0x1D05, effet 0x26 -- adapté */
#define AULA_FX_RIPPLE    ((uint8_t)(FX_COUNT + 5)) /*  9  0x746A / 0x1D8D */
#define AULA_FX_KEYWAVE   ((uint8_t)(FX_COUNT + 6)) /* 10  0x82A5 */
#define AULA_FX_VRAINBOW  ((uint8_t)(FX_COUNT + 7)) /* 11  0x9659 */
#define AULA_FX_GAMING    ((uint8_t)(FX_COUNT + 8)) /* 12  0x1DAB -> 0x95A4 */
#define AULA_FX_SHIMMER   ((uint8_t)(FX_COUNT + 9)) /* 13  0x60C9, effet d'usine 6 */
#define AULA_FX_OFF       ((uint8_t)(FX_COUNT + 10))/* 14 */

/*
 * Les six premiers de cette liste (REACTIVE à RIPPLE) partagent la MÊME
 * mécanique, et c'est celle du firmware d'usine : un plan d'intensité par
 * touche qui décroît d'une trame à l'autre, et un semeur qui y remet des
 * touches à fond. L'usine range la couleur de base en XRAM 0x0428, l'intensité
 * en 0x0017, et son rendu 0x62E6 calcule `base * intensité >> 5` vers 0x0152 --
 * exactement le modèle à deux plans que ce fichier tient déjà pour le moteur
 * réactif. Seul le SEMEUR change d'un effet à l'autre :
 *
 *   REACTIVE   les touches frappées            (déjà en place)
 *   RAIN       une gouttelette par colonne, colonnes relancées au hasard
 *   TWINKLE    des touches tirées au hasard
 *   SNAKE      un point qui parcourt la grille en serpentin et rebondit
 *   SNAKE_RGB  le même, teinte tirée au hasard  (mode couleur 7 de l'usine)
 *   RIPPLE     une couronne par trame depuis (colonne 7, ligne 2)
 *
 * Les trois derniers n'ont pas d'état : ce sont des fonctions pures de la
 * position et de la phase.
 *
 * UNE ADAPTATION ASSUMÉE, SNAKE_RGB. Chez l'usine, l'effet 0x26 n'est pas un
 * effet permanent mais une TRANSITION : 0x9184 sauvegarde l'effet courant en
 * XRAM 0x0E24 avant de poser 0x26, et le gestionnaire 0x1D37 restaure cette
 * sauvegarde dès que le sens vertical du serpent repasse à zéro, c'est-à-dire
 * quand le balayage a fini de parcourir la grille. L'usine s'en sert comme d'un
 * habillage de changement de mode.
 *
 * SMK n'a pas de notion d'effet transitoire et notre liste se parcourt à la
 * touche, donc on le garde permanent. Le rendu est transcrit ; c'est sa DURÉE
 * qui est notre choix.
 */

/*
 * État du moteur réactif. Le firmware d'usine y met 378 octets de couleur et
 * 126 d'intensité ; ici une teinte sur la roue suffit, ce qui tient en un octet
 * par touche au lieu de trois.
 */
/*
 * Décroissance du plan d'intensité, en points par trame. Elle suit la vitesse,
 * comme le diviseur de trame des semeurs : `4 << vitesse` donne 4, 8, 16, 32,
 * 64 pendant que le diviseur donne 16, 8, 4, 2, 1. Le produit reste constant,
 * donc la TRAÎNÉE garde la même longueur -- environ quatre touches -- à toutes
 * les vitesses, et seul le mouvement accélère.
 */
/*
 * Décroissance du plan d'intensité, par TRAME d'animation et non par balayage.
 *
 * Elle ne dépend plus de la vitesse, et c'est le firmware d'usine qui le dit :
 * son rendu calcule `base * intensité >> 5`, donc l'intensité ne porte que
 * TRENTE-ET-UN niveaux, et le moteur réactif `0x64F1` en retire exactement un
 * par trame rendue. 255 / 31 = 8. Toute la réponse à la vitesse est portée par
 * la période de trame ci-dessous, exactement comme en usine.
 */
#define SPARK_DECAY ((uint8_t)8)
static __xdata uint8_t spark_hue[LED_COLS][LED_ROWS];
static __xdata uint8_t spark_val[LED_COLS][LED_ROWS];
static __xdata uint8_t spark_prev[LED_COLS];

/*
 * À purger en même temps que le framebuffer sur un changement d'effet : sinon
 * on revient dans le mode réactif avec des touches qui finissent de s'éteindre
 * depuis la dernière fois, et un bit resté dans `spark_prev` avale le premier
 * appui réel d'une touche qui était enfoncée à la sortie du mode.
 */
static void spark_reset(void)
{
    uint8_t col;
    uint8_t row;

    for (col = 0; col < LED_COLS; col++) {
        spark_prev[col] = 0;
        for (row = 0; row < LED_ROWS; row++) {
            spark_val[col][row] = 0;
        }
    }
}

static uint8_t led_col;   /* colonne affichée par la sous-trame courante */
static uint8_t led_phase; /* phase de l'animation */
static uint8_t regen_row; /* curseur de régénération, une cellule par sous-trame */
static uint8_t regen_col;

/* ------------------------------------------------------- semeurs d'usine */

/*
 * État des semeurs. Vingt-deux octets en tout : l'usine en dépense bien plus,
 * mais elle range aussi la couleur de base de chaque touche, ce que le plan de
 * teintes ci-dessus remplace pour un tiers du prix.
 */
static __xdata uint8_t rain_row[LED_COLS]; /* 0-5, ou 0xFF quand la colonne dort */
static uint8_t         snake_col;
static uint8_t         snake_row;
static uint8_t         snake_right; /* sens horizontal courant */
static uint8_t         snake_down;  /* sens vertical courant */
static uint8_t         ripple_ring;
static uint8_t         fx_ms_acc;    /* millisecondes accumulées depuis la dernière trame */
static __bit           spark_decay_due; /* la trame écoulée autorise la décroissance */

/* Allume une touche à fond, si la grille en porte une à cette position. */
static void spark_seed(uint8_t col, uint8_t row, uint8_t hue)
{
    if (col >= LED_COLS || row >= LED_ROWS) {
        return;
    }
    if (aula_fx_key_id(col, row) == AULA_FX_NO_KEY) {
        return;
    }
    spark_hue[col][row] = hue;
    spark_val[col][row] = 255;
}

/*
 * Pluie -- transcrite de 0x9B2B.
 *
 * Une colonne tirée au hasard est relancée si elle dort, puis chaque colonne
 * active descend d'une ligne et s'éteint après la sixième. L'usine écrit
 * exactement cette boucle : `si état < 6 : peindre, état++ ; sinon état = 0xFF`,
 * précédée d'un `si état[hasard] == 0xFF : état[hasard] = 0`.
 */
static void rain_step(void)
{
    const uint8_t seed = (uint8_t)(aula_fx_rand() % LED_COLS);
    uint8_t       col;

    if (rain_row[seed] == 0xff) {
        rain_row[seed] = 0;
    }
    for (col = 0; col < LED_COLS; col++) {
        const uint8_t row = rain_row[col];

        if (row < LED_ROWS) {
            spark_seed(col, row, (uint8_t)(led_phase + col));
            rain_row[col] = (uint8_t)(row + 1);
        } else {
            rain_row[col] = 0xff;
        }
    }
}

/*
 * Scintillement -- 0xAC1C, dont le rendu 0x4EA9 tire une position au hasard
 * dans la table 0x2EED puis l'allume. Deux touches par trame : l'usine boucle
 * sur un compteur que le relevé ne fixe pas, et deux donne une densité qui
 * ressemble à ce que montre le clavier en vidéo.
 */
static void twinkle_step(void)
{
    uint8_t n;

    for (n = 0; n < 2; n++) {
        const uint8_t r = aula_fx_rand();

        spark_seed((uint8_t)(r % LED_COLS), (uint8_t)((r >> 4) % LED_ROWS), led_phase);
    }
}

/*
 * Serpent -- transcrit de 0x8DDF.
 *
 * Le point avance horizontalement ; arrivé au bord il inverse son sens ET
 * descend (ou monte) d'une ligne ; arrivé en haut ou en bas il inverse aussi
 * son sens vertical. L'usine tient les quatre variables en XRAM 0x0ECA-0x0ECD.
 *
 * `random` distingue SNAKE de SNAKE_RGB : l'effet 0x26 force le mode couleur 7,
 * qui tire une teinte au hasard à chaque touche (0x1D2B).
 */
static void snake_step(uint8_t random_hue)
{
    spark_seed(snake_col, snake_row, random_hue ? aula_fx_rand() : led_phase);

    if (snake_right) {
        if (snake_col + 1u < LED_COLS) {
            snake_col++;
            return;
        }
        snake_right = 0;
    } else {
        if (snake_col != 0) {
            snake_col--;
            return;
        }
        snake_right = 1;
    }

    if (snake_down) {
        if (snake_row + 1u < LED_ROWS) {
            snake_row++;
        } else {
            snake_down = 0;
        }
    } else {
        if (snake_row != 0) {
            snake_row--;
        } else {
            snake_down = 1;
        }
    }
}

/*
 * Onde concentrique -- couronnes de CODE 0x2959, une par trame, la teinte
 * avançant de 13 crans par couronne comme le relève la feuille sur 0x746A.
 *
 * Les identifiants valent `colonne * 8 + ligne` ; ceux qui désignent une
 * colonne au-delà de la quinzième viennent d'un modèle plus large et sont
 * écartés par `spark_seed`.
 */
static void ripple_step(void)
{
    uint8_t slot;

    for (slot = 0; slot < AULA_FX_RING_SLOTS; slot++) {
        const uint8_t id = aula_fx_ring(ripple_ring, slot);

        /*
         * `0xFF` marque une place vide dans la couronne -- mais la table d'usine
         * contient aussi des touches que CE clavier n'a pas : huit identifiants
         * des colonnes 15 et 16 (`0x7A`-`0x7D`, `0x80`-`0x85`), qui appartiennent
         * au modèle de vingt-et-une colonnes de la même famille. `spark_seed()`
         * les écarte par sa borne de colonne ; c'est voulu, pas un hasard.
         */
        if (id == AULA_FX_NO_KEY) {
            continue;
        }
        spark_seed((uint8_t)(id >> 3), (uint8_t)(id & 7u),
                   (uint8_t)(led_phase + (uint8_t)(ripple_ring * 13u)));
    }
    if (++ripple_ring >= AULA_FX_RINGS) {
        ripple_ring = 0;
    }
}

/*
 * Appelé UNE FOIS par trame d'animation, au bouclage du curseur de
 * régénération -- et pas depuis `led_regen_one()`, qui voit chaque colonne six
 * fois par trame. Y semer ferait courir la pluie et le serpent six fois trop
 * vite, et rien dans le journal de compilation ne le montrerait.
 */
/*
 * Remise à plat complète sur changement d'effet : le plan d'intensité ET l'état
 * des semeurs. Sans cela, on rentre dans la pluie avec des colonnes déjà à
 * mi-course et dans le serpent avec un point posé n'importe où.
 */
static void fx_reset(void)
{
    uint8_t col;

    spark_reset();
    for (col = 0; col < LED_COLS; col++) {
        rain_row[col] = 0xff;
    }
    snake_col    = 0;
    snake_row    = 0;
    snake_right  = 1;
    snake_down   = 1;
    ripple_ring     = 0;
    fx_ms_acc       = 0;
    spark_decay_due = 0;

    /*
     * La phase repart de zéro, comme en usine. L'aiguillage de 0x1AA8 ressème
     * le plan de l'effet choisi À CHAQUE sélection -- une branche par effet, la
     * nôtre appelant le transposeur 0xACA3. Sans cette ligne, revenir sur la
     * vague la reprendrait là où un autre effet a laissé le compteur commun,
     * au lieu de repartir du champ de phase d'usine.
     */
    led_phase = 0;
}

/*
 * PÉRIODES D'USINE -- relevées, plus inférées.
 *
 * `CODE 0x2FE9` porte quinze tables de cinq octets, indexées par la vitesse
 * 0 à 4. L'unité est le tic de systick d'usine, et ce tic vaut UNE
 * MILLISECONDE : le raccourci de réinitialisation exige que son compteur
 * atteigne 3000 tics (`0x870C`), et un appui long, c'est trois secondes.
 *
 * Quelle table chaque effet charge est décidé par son SEMEUR, dans la table de
 * répartition `0x1744` -- une entrée par effet, la clé étant l'effet demandé.
 * D'où la correspondance ci-dessous, entièrement tirée du dump :
 *
 *   notre effet         effet d'usine  moteur    table d'usine
 *   ------------------  -------------  --------  ------------------------------
 *   FX_RADIAL                 1        0x746A    aucune -- période FIXE 10, en dur dans `0x1B61`
 *   FX_HORIZONTAL             2        0x7C12    `0x2FF3`
 *   FX_VERTICAL              11        0x6C9B    `0x3016`
 *   FX_SOLID                  3        0x9DA4    `0x2FF8`
 *   AULA_FX_REACTIVE         12        0x64F1    `0x301B`
 *   AULA_FX_RAIN              5        0x9B2B    `0x2FE9`
 *   AULA_FX_TWINKLE           8        0xAC1C    AUCUNE -- choix : celle de l'autre scintillement (effet 6)
 *   AULA_FX_SNAKE            10        0x8DDF    `0x3011`
 *   AULA_FX_SNAKE_RGB      0x26        0x1D05    AUCUNE -- choix : celle du serpent, même moteur
 *   AULA_FX_RIPPLE           17        0x746A    `0x302F`
 *   AULA_FX_KEYWAVE          15        0x82A5    `0x3025`
 *   AULA_FX_VRAINBOW         16        0x9659    `0x302A`
 *   AULA_FX_GAMING         0x20        0x95A4    AUCUNE -- image fixe : choix, celle de l'uni
 *   AULA_FX_SHIMMER           6        0x60C9    `0x3002`
 *
 * Trois lignes sont donc un CHOIX et non une transcription ; elles sont
 * signalées comme telles. Les dix autres sont les octets du dump.
 */
static const __code uint8_t fx_period_ms[AULA_FX_OFF][LED_SPEED_LEVELS] = {
    { 10,  10,  10,  10,  10}, /* FX_RADIAL     -- période fixe de 0x1B61     */
    { 45,  35,  25,  15,   5}, /* FX_HORIZONTAL -- CODE 0x2FF3                */
    { 30,  24,  18,  12,   6}, /* FX_VERTICAL   -- CODE 0x3016                */
    { 45,  35,  25,  15,   6}, /* FX_SOLID      -- CODE 0x2FF8                */
    { 32,  24,  18,  16,   6}, /* REACTIVE      -- CODE 0x301B                */
    {120, 100,  80,  50,  20}, /* RAIN          -- CODE 0x2FE9                */
    { 20,  15,  10,   5,   1}, /* TWINKLE       -- CODE 0x3002, choix         */
    {120,  90,  70,  45,   1}, /* SNAKE         -- CODE 0x3011                */
    {120,  90,  70,  45,   1}, /* SNAKE_RGB     -- celle du serpent, choix    */
    { 50,  40,  30,  20,   8}, /* RIPPLE        -- CODE 0x302F                */
    { 32,  24,  16,   8,   1}, /* KEYWAVE       -- CODE 0x3025                */
    { 46,  36,  26,  16,   6}, /* VRAINBOW      -- CODE 0x302A                */
    { 45,  35,  25,  15,   6}, /* GAMING        -- image fixe, choix          */
    { 20,  15,  10,   5,   1}, /* SHIMMER       -- CODE 0x3002                */
};

/*
 * Durée d'un balayage de régénération, en millisecondes.
 *
 * `tick.c` alterne UN balayage de matrice et `LED_SUBFRAMES_PER_SCAN` = 15
 * sous-trames LED. Une sous-trame dure 400 us (`RELOAD_LED_SUBFRAME`) et le
 * créneau de balayage environ 420 us, soit 6,42 ms par groupe de quinze. Une
 * cellule est régénérée par sous-trame, donc les 90 cellules demandent six
 * groupes : 38,5 ms.
 */
#define LED_SWEEP_MS ((uint8_t)38)

/*
 * ⚠️ CE QUE CETTE ARCHITECTURE NE PEUT PAS RENDRE.
 *
 * L'usine redessine le panneau entier à chaque trame et peut donc descendre à
 * une milliseconde. Ici une trame COÛTE un balayage de régénération, 38,5 ms :
 * toute période inférieure est ramenée à « une trame ». Le tableau ci-dessus
 * n'en est pas dénaturé -- il garde les écarts entre effets, et la pluie comme
 * le serpent conservent leur gradient de vitesse -- mais les périodes rapides
 * s'y écrasent, et c'est notre plancher, pas celui du firmware d'usine.
 *
 * C'est aussi pourquoi `led_speeds[]` continue de faire varier le PAS de phase :
 * l'usine garde un pas fixe et fait varier la cadence, nous ne pouvons faire
 * varier la cadence que d'un facteur trois. Sans le pas variable, SPD_UP et
 * SPD_DN ne se verraient presque plus. Ce point-là est un choix, assumé.
 */
static bool fx_frame_advance(void)
{
    const uint8_t period = fx_period_ms[user_settings.led_effect][user_settings.led_speed];

    fx_ms_acc = (uint8_t)(fx_ms_acc + LED_SWEEP_MS);
    if (fx_ms_acc < period) {
        return false;
    }
    fx_ms_acc = (uint8_t)(fx_ms_acc - period);
    if (fx_ms_acc >= period) {
        fx_ms_acc = 0; /* une trame ne peut pas aller plus vite qu'un balayage */
    }

    led_phase = (uint8_t)(led_phase + led_speeds[user_settings.led_speed]);

    switch (user_settings.led_effect) {
        case AULA_FX_RAIN:      rain_step();     break;
        case AULA_FX_TWINKLE:   twinkle_step();  break;
        case AULA_FX_SNAKE:     snake_step(0);   break;
        case AULA_FX_SNAKE_RGB: snake_step(1);   break;
        case AULA_FX_RIPPLE:    ripple_step();   break;
        default:                                 break;
    }
    return true;
}

#define LED_BRIGHTNESS_DEFAULT (LED_BRIGHTNESS_LEVELS - 1)
#define LED_SPEED_DEFAULT      2


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
/*
 * L'écriture des réglages efface une page de flash, et cet effacement tourne
 * interruptions coupées pendant ~5 ms. Sans ces deux crochets, `tick_dispatch`
 * reste gelé pendant ce temps avec la colonne de la dernière sous-trame TOUJOURS
 * sélectionnée et les trois bancs PWM actifs : cette colonne conduirait 5 ms au
 * lieu de 400 µs, soit une douzaine de fois son rapport cyclique nominal.
 *
 * Et ce n'est pas un cas rare : chaque changement d'effet, de luminosité, de
 * vitesse ou de slot Bluetooth appelle `settings_mark_dirty()`. Même remède que
 * le NuPhy Air60, pour la même raison.
 */
void settings_save_pre(void)
{
    tick_pause();
    indicators_pwm_disable();
}

void settings_save_post(void)
{
    indicators_pwm_enable();
    tick_resume();
}

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
static uint8_t led_scale(uint8_t value, uint8_t gain)
{
    return (uint8_t)(((uint16_t)value * gain) >> 8);
}

/*
 * LE CHOIX DE COULEUR D'USINE.
 *
 * Le firmware d'usine ne rend pas tout en arc-en-ciel : chacun de ses effets
 * porte un mode de couleur sur quatre bits, `b1[3:0]`, verrouillé en XRAM
 * 0x0897, et son rendu se lit d'une ligne (`0x60C9`, `0x6C9B`) :
 *
 *     couleur = (mode == 7) ? roue[phase] : palette[effet][mode]
 *
 * Les modes 0 à 6 sont les sept couleurs fixes de `CODE 0xC800` -- identiques
 * pour les dix-huit effets --, le mode 7 est l'arc-en-ciel, et lui seul lit la
 * roue de teintes. Ce portage ne rendait que le mode 7 ; les huit y sont.
 *
 * Le mode vit dans `user_settings.ul_effect`, un octet que la structure partagée
 * réserve à l'éclairage d'ambiance -- que ce clavier n'a pas. Le réemployer
 * évite d'allonger `user_settings_t`, ce qui ferait retomber TOUS les réglages
 * enregistrés aux valeurs par défaut au premier démarrage (`nvm.c` compare la
 * longueur du bloc).
 */
static void fx_color(uint8_t wheel_index, uint8_t out[3])
{
    if (user_settings.ul_effect >= AULA_FX_COLOR_WHEEL) {
        aula_rgb_wheel(wheel_index, out);
    } else {
        aula_fx_color(user_settings.ul_effect, out);
    }
}

static void led_regen_one(void)
{
    const uint8_t gain = led_brightness_gain[user_settings.led_brightness];
    uint8_t       rgb[3];

    /*
     * SUPPRESSION DES POSITIONS SANS TOUCHE, transcrite du moteur d'usine.
     *
     * Les moteurs continus ne peignent pas la grille entière : `0x82A5` lit la
     * position de la touche dans la table A (`CODE 0x2EED`) et ne rend QUE si
     * elle est inférieure à quinze, puis passe encore par `rgb_key_suppressed`
     * (`0x598F`) ; `0x6C9B` fait le même test avant d'écrire. Six des quatre-
     * vingt-dix emplacements de la grille 6 x 15 ne portent aucune touche, et
     * cette page les tient de `CODE 0xC500`.
     *
     * Les semeurs du plan d'intensité testaient déjà la présence ; les chemins
     * continus -- vague, arc-en-ciel vertical, uni, géométrie de SMK -- ne le
     * faisaient pas et payaient une recherche de couleur pour rien.
     */
    if (aula_fx_key_id(regen_col, regen_row) == AULA_FX_NO_KEY) {
        aula_rgb_set(regen_row, regen_col, 0, 0, 0);
    } else if (user_settings.led_effect >= AULA_FX_REACTIVE &&
        user_settings.led_effect <= AULA_FX_RIPPLE) {
        /* Les six effets à plan d'intensité : le rendu est le même pour tous,
         * seul le semeur diffère. La décroissance vit ici parce que chaque
         * cellule est régénérée exactement une fois par trame. */
        const uint8_t val = spark_val[regen_col][regen_row];

        if (val == 0) {
            aula_rgb_set(regen_row, regen_col, 0, 0, 0);
        } else {
            const uint8_t k = led_scale(val, gain);

            fx_color(spark_hue[regen_col][regen_row], rgb);
            aula_rgb_set(regen_row, regen_col, led_scale(rgb[0], k), led_scale(rgb[1], k),
                         led_scale(rgb[2], k));
            if (spark_decay_due) {
                const uint8_t decay = SPARK_DECAY;

                spark_val[regen_col][regen_row] = (val > decay) ? (uint8_t)(val - decay) : 0;
            }
        }
    } else if (user_settings.led_effect == AULA_FX_VRAINBOW) {
        /*
         * Arc-en-ciel vertical défilant -- transcrit de 0x9659 : la teinte
         * avance de 25 crans PAR LIGNE, modulo 192, et sa base défile d'une
         * trame à l'autre. Six lignes x 25 couvrent 150 des 192 entrées, donc
         * un arc-en-ciel presque complet du haut vers le bas du clavier.
         */
        const uint8_t hue = (uint8_t)((led_phase + (uint8_t)(regen_row * 25u)) %
                                      AULA_RGB_WHEEL_SIZE);

        fx_color(hue, rgb);
        aula_rgb_set(regen_row, regen_col, led_scale(rgb[0], gain), led_scale(rgb[1], gain),
                     led_scale(rgb[2], gain));
    } else if (user_settings.led_effect == AULA_FX_KEYWAVE) {
        /*
         * Vague sur la palette de 128 -- 0x82A5, désormais entièrement
         * transcrite : la palette, l'avance d'un cran par trame et par touche
         * (0xEDA2), ET le champ de phase par touche.
         *
         * Ce dernier était donné ici comme notre invention faute d'en trouver
         * l'écrivain. Il n'y en a pas : c'est un tableau CONSTANT que
         * l'initialiseur C dépose en XDATA 0x0E49 et que 0xACA3 transpose dans
         * le plan 0x0017. `aula_fx_keywave()` le porte tel quel.
         *
         * Chaque touche part de sa phase et avance d'un cran par trame, ce qui
         * revient exactement à `phase_de_la_touche + compteur de trames`. Aucun
         * état à tenir : la fonction est pure.
         */
        const uint8_t idx = (uint8_t)(aula_fx_keywave(regen_col, regen_row) + led_phase);

        if (user_settings.ul_effect >= AULA_FX_COLOR_WHEEL) {
            aula_fx_palette(idx, rgb); /* la seconde roue d'usine, 128 teintes */
        } else {
            aula_fx_color(user_settings.ul_effect, rgb);
        }
        aula_rgb_set(regen_row, regen_col, led_scale(rgb[0], gain), led_scale(rgb[1], gain),
                     led_scale(rgb[2], gain));
    } else if (user_settings.led_effect == AULA_FX_GAMING) {
        /*
         * Image statique de CODE 0xCAFC : Échap, A W S D et le pavé fléché.
         *
         * Le bleu codé en dur qui était ici reposait sur une lecture fausse de
         * la table -- trois plans de couleur dont seul le bleu serait non nul.
         * `0xCAFC` est en réalité un MASQUE d'un octet par touche, sans aucune
         * couleur (voir aula_fx.c). La couleur vient donc du mode de couleur,
         * comme pour tous les autres effets ; le bleu reste accessible, c'est
         * le mode 2.
         */
        if (aula_fx_gaming(regen_col) & (uint8_t)(1u << regen_row)) {
            fx_color(led_phase, rgb);
            aula_rgb_set(regen_row, regen_col, led_scale(rgb[0], gain), led_scale(rgb[1], gain),
                         led_scale(rgb[2], gain));
        } else {
            aula_rgb_set(regen_row, regen_col, 0, 0, 0);
        }
    } else if (user_settings.led_effect == AULA_FX_SHIMMER) {
        /*
         * Scintillement -- effet d'usine 6, moteur `0x60C9`, transcrit.
         *
         * Chaque touche porte sa propre phase, semée par `0xA791` depuis l'une
         * de deux tables selon le mode de couleur, et avance d'un pas par trame
         * via `0xEDA2(phase, 0xC0 ou 0x60)`. Comme toutes les touches avancent
         * du même pas, la phase courante vaut `semence + compteur de trames` :
         * aucun état à tenir, la même astuce que la vague.
         *
         * En arc-en-ciel la phase indexe la roue modulo 192 ; en couleur fixe
         * elle indexe la rampe de respiration modulo 96, et la couleur fixe est
         * mise à l'échelle par cette rampe. Le moteur d'usine fait exactement
         * ces deux choses, dans ces deux modules.
         *
         * `led_phase` avance de `led_speeds[vitesse]` et non de un : c'est le
         * choix de ce portage, déjà expliqué plus haut, et il est appliqué ici
         * comme à la vague.
         */
        if (user_settings.ul_effect >= AULA_FX_COLOR_WHEEL) {
            const uint16_t p = (uint16_t)aula_fx_shimmer_phase(regen_col, regen_row, 1) + led_phase;

            aula_rgb_wheel((uint8_t)(p % AULA_RGB_WHEEL_SIZE), rgb);
            aula_rgb_set(regen_row, regen_col, led_scale(rgb[0], gain), led_scale(rgb[1], gain),
                         led_scale(rgb[2], gain));
        } else {
            const uint16_t p = (uint16_t)aula_fx_shimmer_phase(regen_col, regen_row, 0) + led_phase;
            const uint8_t  k = aula_fx_breath((uint8_t)(p % AULA_FX_BREATH_SIZE));

            aula_fx_color(user_settings.ul_effect, rgb);
            aula_rgb_set(regen_row, regen_col, led_scale(led_scale(rgb[0], k), gain),
                         led_scale(led_scale(rgb[1], k), gain),
                         led_scale(led_scale(rgb[2], k), gain));
        }
    } else if (user_settings.led_effect == (uint8_t)FX_SOLID) {
        /*
         * Le blanc en dur était une invention : l'effet d'usine d'indice 3
         * (`0x9DA4`) est « une matrice d'une seule couleur, teinte calculée hors
         * boucle ». Cette couleur, c'est le choix de couleur -- la roue à la
         * phase courante en arc-en-ciel, la couleur fixe sinon. Le blanc reste
         * accessible : c'est le mode 6.
         */
        fx_color(led_phase, rgb);
        aula_rgb_set(regen_row, regen_col, led_scale(rgb[0], gain), led_scale(rgb[1], gain),
                     led_scale(rgb[2], gain));
    } else {
        /*
         * Géométrie de SMK, couleurs d'usine : l'index sur 0-255 est ramené aux
         * 192 entrées de la roue.
         *
         * La colonne passée n'est PAS la colonne électrique mais la colonne
         * SPATIALE, tirée de la table A d'usine (`CODE 0x2EED`). La géométrie
         * générée suppose une grille uniforme -- `axis_x[col] = col * 17` -- et
         * cette hypothèse est fausse sur la ligne 4, où la colonne électrique 12
         * se trouve entre les colonnes 0 et 1. Sans cette conversion, le
         * dégradé horizontal et l'onde radiale placent cette touche-là au
         * mauvais endroit.
         *
         * La LIGNE, elle, n'a besoin d'aucune conversion : la table B d'usine
         * (`CODE 0x2F6B`) est exactement l'index de ligne sur ses 126 positions.
         */
        const uint8_t idx =
            led_effect_index((led_effect_t)user_settings.led_effect, regen_row,
                             aula_fx_render_col(regen_col, regen_row), led_phase);

        fx_color((uint8_t)(((uint16_t)idx * AULA_RGB_WHEEL_SIZE) >> 8), rgb);
        aula_rgb_set(regen_row, regen_col, led_scale(rgb[0], gain), led_scale(rgb[1], gain),
                     led_scale(rgb[2], gain));
    }

    if (++regen_col >= LED_COLS) {
        regen_col = 0;
        if (++regen_row >= LED_ROWS) {
            regen_row = 0;
            /* La phase avance DANS la porte de trame, comme en usine : le rendu
             * d'usine est entièrement conditionné à `tic >= période`. */
            spark_decay_due = fx_frame_advance();
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

/*
 * Détection des frappes pour le moteur réactif : une colonne par sous-trame,
 * donc chaque colonne est examinée une fois par balayage LED -- exactement la
 * cadence à laquelle `matrix.c` la rafraîchit. Seuls les fronts comptent, une
 * touche maintenue ne réamorce pas.
 */
static void led_react_poll(void)
{
    const uint8_t now   = user_matrix_pressed(led_col);
    const uint8_t fresh = (uint8_t)(now & (uint8_t)~spark_prev[led_col]);

    spark_prev[led_col] = now;

    if (fresh == 0) {
        return;
    }
    for (uint8_t row = 0; row < LED_ROWS; row++) {
        if (fresh & (uint8_t)(1u << row)) {
            spark_hue[led_col][row] = led_phase;
            spark_val[led_col][row] = 255;
        }
    }
}

bool indicators_update_step(keyboard_state_t *keyboard, uint8_t current_step)
{
    (void)keyboard;      /* les indicateurs d'état ne sont pas encore portés */
    (void)current_step;  /* `tick.c` passe toujours 0 */

    if (user_settings.led_effect == AULA_FX_REACTIVE) {
        led_react_poll();
    }

    if (user_settings.led_effect < AULA_FX_OFF) {
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
    user_settings.ul_effect      = AULA_FX_COLOR_WHEEL; /* arc-en-ciel, comme en usine */
}

void indicators_validate_settings(void)
{
    if (user_settings.led_effect > AULA_FX_OFF) {
        user_settings.led_effect = FX_RADIAL;
    }
    if (user_settings.led_brightness >= LED_BRIGHTNESS_LEVELS) {
        user_settings.led_brightness = LED_BRIGHTNESS_DEFAULT;
    }
    if (user_settings.led_speed >= LED_SPEED_LEVELS) {
        user_settings.led_speed = LED_SPEED_DEFAULT;
    }
    if (user_settings.ul_effect >= AULA_FX_COLOR_MODES) {
        user_settings.ul_effect = AULA_FX_COLOR_WHEEL;
    }
}

void indicators_init(void)
{
    led_col   = 0;
    led_phase = 0;
    regen_row = 0;
    regen_col = 0;

    fx_reset();
    aula_rgb_clear();
}

void indicators_start(void)
{
    indicators_validate_settings();
}

/* ------------------------------------------------------- actions clavier */

void indicators_next_color(void)
{
    if (++user_settings.ul_effect >= AULA_FX_COLOR_MODES) {
        user_settings.ul_effect = 0;
    }
    settings_mark_dirty();
}

void indicators_next_effect(void)
{
    if (++user_settings.led_effect > AULA_FX_OFF) {
        user_settings.led_effect = 0;
    }
    aula_rgb_clear(); /* l'effet précédent laisserait ses pixels derrière lui */
    fx_reset();
    settings_mark_dirty();
}

void indicators_prev_effect(void)
{
    if (user_settings.led_effect == 0) {
        user_settings.led_effect = AULA_FX_OFF;
    } else {
        user_settings.led_effect--;
    }
    aula_rgb_clear();
    fx_reset();
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
