#include "aula_layout.h"
#include "aula_status.h"
#include "sh68f90.h"
#include "report.h"
#include "keycodes.h"
#include <stdint.h>

/*
 * La plage traduite : de `KC_A` à `KC_SLASH`, les seuls keycodes qui produisent
 * un caractère. Tout le reste passe brut -- flèches, F1-F12, pavé de
 * navigation, modificateurs, et tous les keycodes de la couche `_FN`, qui sont
 * soit au-dessus de `0x38`, soit au-delà de `SAFE_RANGE`.
 */
#define AZ_FIRST KC_A
#define AZ_LAST  KC_SLASH
#define AZ_N     (AZ_LAST - AZ_FIRST + 1)

#define AZ_SH  0x01u /* injecter Maj */
#define AZ_AG  0x02u /* injecter AltGr */
#define AZ_ACU 0x04u /* accent aigu : touche morte tenue par le firmware */
#define AZ_DIA 0x08u /* tréma       : touche morte tenue par le firmware */
#define AZ_DEAD (AZ_ACU | AZ_DIA)
#define AZ_RAW 0x80u /* ne rien injecter NI masquer -- voie des raccourcis */

/* Les deux Maj. Le masque les retire de `real_mods` : sur un hôte AZERTY le `!`
 * est SANS Maj alors que le clavier le porte en Maj+1, et sans masque la touche
 * sortirait `1`. */
#define AZ_SHIFT_MODS ((uint8_t)MODS_SHIFT_MASK)

/* Tout modificateur AUTRE que Maj : Ctrl, Alt, AltGr, GUI, des deux côtés. */
#define AZ_OTHER_MODS ((uint8_t)~AZ_SHIFT_MODS)

typedef struct {
    uint8_t kc;
    uint8_t fl;
} az_ent_t;

/*
 * Table de traduction vers l'AZERTY français (France), Windows.
 *
 * Indexée par `keycode US - KC_A`, puis par l'état de Maj. Le commentaire de
 * chaque ligne donne ce que PORTE la touche du clavier, sans Maj puis avec :
 * c'est le caractère voulu, pas le keycode émis.
 *
 * Les lettres sont presque une identité -- seules `a<->q`, `z<->w` et `m`
 * bougent. Ce qui coûte, c'est la rangée des chiffres, où l'AZERTY place les
 * symboles SANS Maj : il faut donc injecter Maj pour obtenir un chiffre.
 */
static const __code az_ent_t az_map[AZ_N][2] = {
    /* a    A    */ {{0x14, 0}, {0x14, AZ_SH}},
    /* b    B    */ {{0x05, 0}, {0x05, AZ_SH}},
    /* c    C    */ {{0x06, 0}, {0x06, AZ_SH}},
    /* d    D    */ {{0x07, 0}, {0x07, AZ_SH}},
    /* e    E    */ {{0x08, 0}, {0x08, AZ_SH}},
    /* f    F    */ {{0x09, 0}, {0x09, AZ_SH}},
    /* g    G    */ {{0x0A, 0}, {0x0A, AZ_SH}},
    /* h    H    */ {{0x0B, 0}, {0x0B, AZ_SH}},
    /* i    I    */ {{0x0C, 0}, {0x0C, AZ_SH}},
    /* j    J    */ {{0x0D, 0}, {0x0D, AZ_SH}},
    /* k    K    */ {{0x0E, 0}, {0x0E, AZ_SH}},
    /* l    L    */ {{0x0F, 0}, {0x0F, AZ_SH}},
    /* m    M    */ {{0x33, 0}, {0x33, AZ_SH}},
    /* n    N    */ {{0x11, 0}, {0x11, AZ_SH}},
    /* o    O    */ {{0x12, 0}, {0x12, AZ_SH}},
    /* p    P    */ {{0x13, 0}, {0x13, AZ_SH}},
    /* q    Q    */ {{0x04, 0}, {0x04, AZ_SH}},
    /* r    R    */ {{0x15, 0}, {0x15, AZ_SH}},
    /* s    S    */ {{0x16, 0}, {0x16, AZ_SH}},
    /* t    T    */ {{0x17, 0}, {0x17, AZ_SH}},
    /* u    U    */ {{0x18, 0}, {0x18, AZ_SH}},
    /* v    V    */ {{0x19, 0}, {0x19, AZ_SH}},
    /* w    W    */ {{0x1D, 0}, {0x1D, AZ_SH}},
    /* x    X    */ {{0x1B, 0}, {0x1B, AZ_SH}},
    /* y    Y    */ {{0x1C, 0}, {0x1C, AZ_SH}},
    /* z    Z    */ {{0x1A, 0}, {0x1A, AZ_SH}},
    /* 1    !    */ {{0x1E, AZ_SH}, {0x38, 0}},
    /* 2    @    */ {{0x1F, AZ_SH}, {0x27, AZ_AG}},
    /* 3    #    */ {{0x20, AZ_SH}, {0x20, AZ_AG}},
    /* 4    $    */ {{0x21, AZ_SH}, {0x30, 0}},
    /* 5    %    */ {{0x22, AZ_SH}, {0x34, AZ_SH}},
    /* 6    ^    */ {{0x23, AZ_SH}, {0x2F, 0}},
    /* 7    &    */ {{0x24, AZ_SH}, {0x1E, 0}},
    /* 8    *    */ {{0x25, AZ_SH}, {0x31, 0}},
    /* 9    (    */ {{0x26, AZ_SH}, {0x22, 0}},
    /* 0    )    */ {{0x27, AZ_SH}, {0x2D, 0}},
    /* Entr Entr */ {{0x28, 0}, {0x28, 0}},
    /* Ech  Ech  */ {{0x29, 0}, {0x29, 0}},
    /* RArr RArr */ {{0x2A, 0}, {0x2A, 0}},
    /* Tab  Tab  */ {{0x2B, 0}, {0x2B, 0}},
    /* Esp  Esp  */ {{0x2C, 0}, {0x2C, 0}},
    /* -    _    */ {{0x23, 0}, {0x25, 0}},
    /* =    +    */ {{0x2E, 0}, {0x2E, AZ_SH}},
    /* [    {    */ {{0x22, AZ_AG}, {0x21, AZ_AG}},
    /* ]    }    */ {{0x2D, AZ_AG}, {0x2E, AZ_AG}},
    /* \    |    */ {{0x25, AZ_AG}, {0x23, AZ_AG}},
    /* ---  ---  */ {{0x32, 0}, {0x32, 0}},
    /* ;    :    */ {{0x36, 0}, {0x37, 0}},
    /* '    "    */ {{0x21, AZ_ACU}, {0x20, AZ_DIA}},
    /* `    ~    */ {{0x24, AZ_AG}, {0x1F, AZ_AG}},
    /* ,    <    */ {{0x10, 0}, {0x64, 0}},
    /* .    >    */ {{0x36, AZ_SH}, {0x64, AZ_SH}},
    /* /    ?    */ {{0x37, AZ_SH}, {0x10, AZ_SH}},
};

/*
 * Base de temps : la sous-trame LED, ~420 µs une fois les balayages de matrice
 * amortis. La même que `aula_encoder.c` et `aula_macro.c`, pour la même raison
 * -- il n'existe aucun compteur de millisecondes libre sur ce firmware.
 */
#define AZ_MS(ms) ((uint16_t)(((uint32_t)(ms) * 1000ul) / 420ul))

/* L'hôte sonde toutes les millisecondes (`bInterval = 1`) : un appui suivi de
 * son relâchement dans le même passage ne serait JAMAIS vu. Huit sous-trames
 * font ~3,4 ms, soit trois sondages au minimum. Même valeur qu'`ENC_HOLD`. */
#define AZ_HOLD 8
#define AZ_GAP  4

/*
 * Garde-fou des touches mortes. US International n'a pas de délai, mais un `'`
 * oublié mangerait la frappe suivante -- des heures plus tard. Au bout de trois
 * secondes, le symbole seul part.
 */
#define AZ_DEAD_MAX AZ_MS(3000)

/* File d'émission. Puissance de deux : l'avance se fait au masque. */
#define AZ_Q 4

/*
 * Suivi des touches traduites tenues. Il ne sert pas qu'à compter : le
 * relâchement doit retirer du rapport le keycode RÉELLEMENT émis. Sans lui,
 * lâcher Maj avant la touche (`Maj+1` donne `!` = `KC_SLASH`, puis le
 * relâchement verrait l'état sans Maj et retirerait `KC_1`) laisserait une
 * touche collée.
 */
#define AZ_T 8

static __bit az_on;

static __xdata uint8_t az_t_us[AZ_T]; /* keycode US tenu, 0 = libre */
static __xdata uint8_t az_t_kc[AZ_T]; /* keycode réellement émis */
static __xdata uint8_t az_down;
static __xdata uint8_t az_hold_fl = AZ_RAW;

static __xdata uint8_t az_q_kc[AZ_Q];
static __xdata uint8_t az_q_fl[AZ_Q];
static __xdata uint8_t az_q_head;
static __xdata uint8_t az_q_tail;
static __xdata uint8_t az_play; /* 0 repos, 1 appui en cours, 2 silence */
static __xdata uint8_t az_cur;

/* Décrémentés par l'ISR, armés par la boucle principale. En XDATA : cette carte
 * n'a que dix-sept octets de marge en RAM interne, et l'ISR accède déjà à la
 * XRAM en permanence pour le rendu. */
static volatile __xdata uint16_t az_wait;
static volatile __xdata uint16_t az_dead_to;

/* L'ISR lève ces bits plutôt que de laisser la boucle lire les compteurs : un
 * test de bit est atomique sur 8051, une lecture 16 bits ne l'est pas -- et un
 * mot lu à cheval sur une décrémentation peut valoir zéro à tort. */
static volatile __bit az_due;
static volatile __bit az_dead_due;

/* La touche morte en attente : 0, AZ_ACU ou AZ_DIA -- les valeurs de la table
 * elle-même, il n'y a donc rien à traduire entre les deux. */
static __xdata uint8_t az_dead;

static void az_mods_apply(uint8_t fl)
{
    uint8_t w;

    if (fl & AZ_RAW) {
        set_mods_mask(0xFF);
        set_weak_mods(0);
        return;
    }

    set_mods_mask((uint8_t)~AZ_SHIFT_MODS);

    w = 0;
    if (fl & AZ_SH) w |= (uint8_t)MOD_BIT(KC_LEFT_SHIFT);
    if (fl & AZ_AG) w |= (uint8_t)MOD_BIT(KC_RIGHT_ALT);
    set_weak_mods(w);
}

/* L'injection au repos est celle du DERNIER appui, rendue quand la dernière
 * touche traduite est relâchée. */
static void az_mods_rest(void)
{
    az_mods_apply(az_down != 0 ? az_hold_fl : (uint8_t)AZ_RAW);
}

static void az_arm(uint16_t n)
{
    az_due = 0;
    ET2    = 0;
    az_wait = n;
    ET2    = 1;
}

static void az_push(uint8_t kc, uint8_t fl)
{
    const uint8_t next = (uint8_t)((az_q_tail + 1u) & (AZ_Q - 1u));

    /* File pleine : on perd le caractère plutôt que d'écraser celui qui joue.
     * Il faudrait taper plus de deux cents caractères par seconde pour y
     * arriver -- la file ne sert qu'aux résolutions de touche morte. */
    if (next == az_q_head) {
        return;
    }
    az_q_kc[az_q_tail] = kc;
    az_q_fl[az_q_tail] = (uint8_t)(fl & (AZ_SH | AZ_AG));
    az_q_tail          = next;
}

static bool az_track_add(uint8_t us, uint8_t kc)
{
    for (uint8_t i = 0; i < AZ_T; i++) {
        if (az_t_us[i] == 0) {
            az_t_us[i] = us;
            az_t_kc[i] = kc;
            az_down++;
            return true;
        }
    }
    return false;
}

static uint8_t az_track_del(uint8_t us)
{
    for (uint8_t i = 0; i < AZ_T; i++) {
        if (az_t_us[i] == us) {
            const uint8_t kc = az_t_kc[i];
            az_t_us[i] = 0;
            if (az_down != 0) az_down--;
            return kc;
        }
    }
    return 0;
}

static void az_reset(void)
{
    az_arm(0);
    ET2        = 0;
    az_dead_to = 0;
    ET2        = 1;
    az_dead_due = 0;
    az_dead     = 0;

    az_q_head = 0;
    az_q_tail = 0;
    az_play   = 0;

    for (uint8_t i = 0; i < AZ_T; i++) {
        az_t_us[i] = 0;
    }
    az_down    = 0;
    az_hold_fl = AZ_RAW;
    az_mods_apply(AZ_RAW);

    /* Une touche traduite encore tenue resterait dans le rapport avec le
     * keycode de l'ancienne disposition : on repart du rapport vide. */
    clear_keys();
    send_keyboard_report();
}

void aula_layout_toggle(void)
{
    az_on = !az_on;
    az_reset();

    aula_status_set_azerty(az_on ? true : false);
    aula_status_event(az_on ? (uint8_t)AULA_STATUS_AZERTY_ON
                            : (uint8_t)AULA_STATUS_AZERTY_OFF);
}

void aula_layout_euro(void)
{
    az_push(KC_E, AZ_AG);
}

bool aula_layout_intercept(uint16_t qcode, bool pressed)
{
    uint8_t idx, kc, fl, mods;

    if (!az_on) {
        return false;
    }
    if (qcode < AZ_FIRST || qcode > AZ_LAST) {
        return false;
    }
    idx = (uint8_t)(qcode - AZ_FIRST);

    if (!pressed) {
        /* Retirer le keycode RÉELLEMENT émis. S'il n'était pas suivi -- joué par
         * la file, ou appuyé avant l'allumage du mode -- retirer le brut : c'est
         * sans effet quand il n'est pas dans le rapport. */
        kc = az_track_del((uint8_t)qcode);
        del_key(kc != 0 ? kc : (uint8_t)qcode);
        az_mods_rest();
        send_keyboard_report();
        return true;
    }

    mods = get_mods();

    /*
     * La décision se prend sur `real_mods`, l'état PHYSIQUE, et avant toute
     * écriture : le mode injecte lui-même AltGr, que Windows voit comme
     * Ctrl+Alt, et une garde qui lirait le rapport se déclencherait sur sa
     * propre injection au coup suivant.
     */
    if (mods & AZ_OTHER_MODS) {
        /* Raccourci : traduire la position, n'injecter ni masquer rien.
         * `Ctrl+A` devient `Ctrl+KC_Q`, `Ctrl+1` reste `Ctrl+1`. */
        az_dead = 0;
        kc      = az_map[idx][0].kc;
        if (!az_track_add((uint8_t)qcode, kc)) {
            return false;
        }
        az_hold_fl = AZ_RAW;
        az_mods_apply(AZ_RAW);
        add_key(kc);
        send_keyboard_report();
        return true;
    }

    {
        const uint8_t st = (mods & AZ_SHIFT_MODS) ? 1u : 0u;
        kc = az_map[idx][st].kc;
        fl = az_map[idx][st].fl;

        if (az_dead != 0) {
            const uint8_t d = az_dead;

            az_dead     = 0;
            ET2         = 0;
            az_dead_to  = 0;
            ET2         = 1;
            az_dead_due = 0;

            if (d == AZ_DIA) {
                /*
                 * Le fr-FR a une VRAIE touche morte tréma -- Maj + `KC_LBRC` --
                 * et c'est l'hôte qui compose. Mais son tréma suivi d'un espace
                 * rend `¨`, PAS `"` : c'est la seule des quatre touches mortes de
                 * l'hôte dont le symbole seul diffère de celui d'US
                 * International, donc la seule qui ne peut pas être un simple
                 * passe-plat. Mesuré sur l'appareil.
                 */
                /* La borne basse est déjà garantie : la fonction rend la main
                 * plus haut pour tout `qcode` sous `AZ_FIRST`, qui vaut `KC_A`.
                 * La retester ferait supprimer le test par l'optimiseur, et
                 * `--Werror` en fait une erreur (SDCC 110). */
                if (qcode <= KC_Z) {
                    az_push(0x2Fu, AZ_SH); /* tréma mort : l'hôte compose */
                    az_push(kc, fl);
                } else if (qcode == KC_SPACE) {
                    az_push(0x20u, 0); /* `"` -- sans Maj sur AZERTY */
                } else {
                    az_push(0x20u, 0);
                    az_push(kc, fl);
                }
                return true;
            }

            /* Le fr-FR n'a PAS de touche morte aiguë : `é` et `ç` sont des
             * touches à part entière, et `á í ó ú ý` ne sont pas produisibles.
             * Pour tout le reste, le repli d'US International -- l'apostrophe
             * puis le caractère. */
            if (st == 0 && qcode == KC_E) {
                az_push(0x1Fu, 0); /* é */
            } else if (st == 0 && qcode == KC_C) {
                az_push(0x26u, 0); /* ç */
            } else if (qcode == KC_SPACE) {
                az_push(0x21u, 0); /* ' */
            } else {
                az_push(0x21u, 0);
                az_push(kc, fl);
            }
            return true;
        }

        if (fl & AZ_DEAD) {
            az_dead     = (uint8_t)(fl & AZ_DEAD);
            ET2         = 0;
            az_dead_to  = AZ_DEAD_MAX;
            ET2         = 1;
            az_dead_due = 0;
            return true; /* touche morte : rien n'est émis */
        }

        /* Tant que la file joue, passer par elle : un caractère naturel
         * passerait sinon devant un caractère synthétique encore en vol. */
        if (az_play != 0 || az_q_head != az_q_tail) {
            az_push(kc, fl);
            return true;
        }

        /* Voie naturelle : l'appui tient le keycode traduit, donc la répétition
         * automatique de l'hôte fonctionne. */
        if (!az_track_add((uint8_t)qcode, kc)) {
            return false;
        }
        az_hold_fl = fl;
        az_mods_apply(fl);
        add_key(kc);
        send_keyboard_report();
        return true;
    }
}

void aula_layout_tick(void)
{
    if (az_wait != 0) {
        az_wait--;
        if (az_wait == 0) {
            az_due = 1;
        }
    }
    if (az_dead_to != 0) {
        az_dead_to--;
        if (az_dead_to == 0) {
            az_dead_due = 1;
        }
    }
}

void aula_layout_task(void)
{
    if (az_dead_due) {
        az_dead_due = 0;
        if (az_dead != 0) {
            /* Le symbole seul : `'` pour l'aigu, `"` pour le tréma. */
            az_push(az_dead == AZ_DIA ? 0x20u : 0x21u, 0);
            az_dead = 0;
        }
    }

    if (az_play == 0) {
        if (az_q_head == az_q_tail) {
            return;
        }
        az_cur = az_q_kc[az_q_head];
        az_mods_apply(az_q_fl[az_q_head]);
        add_key(az_cur);
        send_keyboard_report();
        az_play = 1;
        az_arm(AZ_HOLD);
        return;
    }

    if (!az_due) {
        return;
    }
    az_due = 0;

    if (az_play == 1) {
        del_key(az_cur);
        send_keyboard_report();
        az_play = 2;
        az_arm(AZ_GAP);
        return;
    }

    az_play   = 0;
    az_q_head = (uint8_t)((az_q_head + 1u) & (AZ_Q - 1u));
    if (az_q_head == az_q_tail) {
        az_mods_rest();
    }
}
