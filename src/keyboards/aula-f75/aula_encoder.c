#include "aula_encoder.h"
#include "sh68f90.h"
#include "host.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Décodeur en quadrature, transcrit de `fcn.00007928`.
 *
 *   état = (P0 >> 5) & 3            deux bits : P0.5 et P0.6
 *   hist = (état & 3) | (hist << 2) & 0x3C
 *   0x0B, 0x34 -> un sens          0x07, 0x38 -> l'autre
 *
 * Les deux broches sont déjà en entrée avec rappel sous la configuration
 * d'usine reprise telle quelle par `user_init.c` : `P0CR = 0x9C` laisse les
 * bits 5 et 6 à zéro, `P0PCR = 0xF8` les met à un. Rien à configurer ici.
 *
 * L'usine émet les usages Consumer `0xE9` et `0xEA` -- volume + et volume - --
 * par le tampon `0x09BD`. Un réglage persistant (offset 26 du bloc) bascule la
 * molette sur la luminosité du rétroéclairage ; la valeur du dump vaut 0, donc
 * volume. On suit le dump.
 */

#define ENC_VOL_UP   0x00E9
#define ENC_VOL_DOWN 0x00EA

/*
 * L'hôte sonde l'endpoint toutes les millisecondes (`bInterval = 1`). Un appui
 * plus court qu'un sondage serait écrasé par son propre relâchement et ne
 * serait JAMAIS vu -- c'est le piège de tout évènement synthétique, qui n'a pas
 * la durée naturelle d'un doigt sur une touche.
 *
 * Compté en sous-trames LED, soit ~420 µs pièce une fois les balayages de
 * matrice amortis : huit d'appui font ~3,4 ms, quatre de silence ~1,7 ms. Une
 * détente coûte donc ~5 ms, ce qui plafonne à deux cents détentes par seconde --
 * hors d'atteinte d'une molette réelle.
 */
#define ENC_GAP  4
#define ENC_HOLD (ENC_GAP + 8)

/*
 * BATTEMENT DE LA MOLETTE.
 *
 * Le motif est celui du coeur de la frappe automatique, mais sa CADENCE suit le
 * sens : monter accélère, descendre ralentit. C'est le seul retour possible sur
 * cette action -- l'hôte ne dit jamais au clavier où en est son volume, donc le
 * niveau affiché est RELATIF, pas le volume réel.
 *
 * Les huit cadences sont géométriques, de raison ~1,159 : l'oreille et l'oeil
 * perçoivent le tempo en rapport, pas en différence, donc des crans également
 * espacés en rapport se lisent comme également espacés.
 *
 * 256 = un pas de motif par bouclage de balayage (~37,9 ms). Le cycle complet
 * fait 24 pas, donc cycle_ms = 24 x 256 x 37,9 / cadence : la table couvre
 * 2,8 s au plus bas à 1,0 s au plus haut.
 */
#define ENC_BEAT_LEVELS 8
#define ENC_BEAT_START  3

/* ~1,5 s après la dernière détente, en sous-trames de ~420 µs. */
#define ENC_BEAT_HOLD 3570u

static const __code uint16_t enc_beat_rates[ENC_BEAT_LEVELS] = {
    83, 96, 111, 129, 150, 174, 201, 233,
};

/* Touchés uniquement depuis l'ISR -- `aula_encoder_sample()` les écrit, le rendu
 * LED les lit, et les deux vivent dans l'ISR Timer2. Aucune section critique à
 * poser, et aucune lecture 16 bits à cheval sur une écriture. */
static __xdata uint8_t  beat_lvl = ENC_BEAT_START;
static __xdata uint16_t beat_to;

static volatile __data uint8_t enc_hold;
static volatile __data int8_t  enc_pending;
static volatile __bit          enc_release_due;

/* En XDATA : ces deux-là ne sont touchés que par l'ISR, qui accède déjà à la
 * XRAM pour le rendu. La RAM interne n'a que dix-neuf octets de marge sur cette
 * carte, et ils valent mieux ailleurs. */
static __xdata uint8_t enc_state;
static __xdata uint8_t enc_hist;

void aula_encoder_sample(void)
{
    uint8_t now;

    if (beat_to != 0) {
        beat_to--;
    }

    if (enc_hold != 0) {
        enc_hold--;
        if (enc_hold == ENC_GAP) {
            enc_release_due = 1;
        }
    }

    now = (uint8_t)((P0 >> 5) & 0x03u);
    if (now == enc_state) {
        return;
    }
    enc_hist  = (uint8_t)((now & 0x03u) | (uint8_t)((enc_hist << 2) & 0x3Cu));
    enc_state = now;

    /*
     * Les détentes font la queue plutôt que d'être perdues quand la boucle est
     * plus lente que la molette. La borne évite qu'un balayage parasite ne
     * remplisse le compteur et ne fasse défiler le volume tout seul.
     */
    if (enc_hist == 0x0B || enc_hist == 0x34) {
        if (enc_pending < 8) enc_pending++;
        if (beat_lvl < (uint8_t)(ENC_BEAT_LEVELS - 1)) beat_lvl++;
        beat_to = ENC_BEAT_HOLD;
    } else if (enc_hist == 0x07 || enc_hist == 0x38) {
        if (enc_pending > -8) enc_pending--;
        if (beat_lvl != 0) beat_lvl--;
        beat_to = ENC_BEAT_HOLD;
    }
}

bool aula_encoder_beating(void)
{
    return beat_to != 0;
}

uint16_t aula_encoder_beat_rate(void)
{
    return enc_beat_rates[beat_lvl];
}

uint8_t aula_encoder_beat_level(void)
{
    return beat_lvl;
}

void aula_encoder_task(void)
{
    int8_t pending;

    if (enc_release_due) {
        enc_release_due = 0;
        host_consumer_send(0);
        return;
    }
    if (enc_hold != 0) {
        return;
    }

    /* `enc_pending` est incrémenté par l'ISR et décrémenté ici : la
     * lecture-modification-écriture doit être protégée, sinon une détente se
     * perd quand les deux tombent ensemble. */
    ET2     = 0;
    pending = enc_pending;
    if (pending > 0) {
        enc_pending--;
    } else if (pending < 0) {
        enc_pending++;
    }
    ET2 = 1;

    if (pending > 0) {
        host_consumer_send(ENC_VOL_UP);
        enc_hold = ENC_HOLD;
    } else if (pending < 0) {
        host_consumer_send(ENC_VOL_DOWN);
        enc_hold = ENC_HOLD;
    }
}
