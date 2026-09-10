#include "aula_rf.h"

#include "sh68f90.h"
#include "interrupts.h"
#include "delay.h"
#include "usb.h"
#include "usbhw.h"
#include "keyboard.h"
#include <string.h>

/*
 * Liaison EUART0 <-> BK3632 de l'AULA F75.
 *
 * Transcrit du firmware d'usine. Les adresses citées renvoient à
 * `docs/keyboards/aula-f75.md`, qui porte le raisonnement complet.
 *
 * ÉTABLI au désassemblage : le débit et les registres, les deux handshakes, le
 * format de trame et sa somme de contrôle, le jeu de commandes, et la machine à
 * états du sélecteur de transport (`fcn.000084E9`) — y compris le fait que la
 * commande 0x01 avec le slot 0 est ce qui fait passer le module en 2,4 GHz.
 *
 * INFÉRÉ, et signalé comme tel à chaque endroit : la disposition exacte des
 * octets de charge utile des trames de frappe (0x02 / 0x03), et le champ de la
 * trame d'état qui signale « connecté ».
 *
 * Rien n'a jamais été flashé sur l'appareil : ce code n'a pas été exécuté.
 */

/* ------------------------------------------------------------------ liaison */

/*
 * Débit : SBRTH/SBRTL/SFINE relevés en 0xB1CE.
 *
 *   SBRT   = 0x7FFB (SBRTH bit 7 = SBRTEN, donc la valeur est 0x7FFB)
 *   Baud   = Fsys / (16 * (32768 - SBRT) + SFINE)
 *          = Fsys / (16 * 5 + 12) = Fsys / 92
 *          = 24 MHz / 92 ~= 260 870 bauds
 *
 * On garde les littéraux du firmware d'usine plutôt qu'un calcul depuis un
 * débit nominal : c'est le module en face qui impose la valeur, et elle ne
 * correspond à aucun débit standard.
 */
#define RF_SBRTH 0xFF
#define RF_SBRTL 0xFB
#define RF_SFINE 0x0C
#define RF_SCON  0x50 /* mode 1, 8N1, réception activée */

_Static_assert(FREQ_SYS / 92 > 255000 && FREQ_SYS / 92 < 267000, "FREQ_SYS incompatible avec le diviseur d'usine (Fsys/92)");

/* Buffer d'émission : 32 o, la taille exacte de celui du firmware d'usine. */
#define RF_TX_MAX 32

/*
 * Réception : le firmware d'usine ne découpe PAS les trames dans son ISR.
 *
 * Son ISR (0xA544) empile les octets dans un tampon IDATA de 23 octets
 * (0x54-0x6A), jette tout ce qui dépasse, et pose un simple drapeau « octet
 * reçu » (0x24.4). Un étage différé (`fcn.000005EA`) recopie les 23 octets vers
 * XDATA 0x0120, et c'est `euart0_parse` qui dispatche sur l'octet 0 et vérifie
 * la somme À LA POSITION PROPRE AU TYPE :
 *
 *   0x02  trame d'état  10 octets, somme = 0x55 - Σ[0..8],  comparée à [9]
 *   0x08  conteneur     22 octets, somme = 0x55 - Σ[0..20], comparée à [21]
 *   0x03  annonce       AUCUNE somme vérifiée, longueur non établie
 *
 * Une première rédaction de ce fichier attendait 22 octets pour TOUTES les
 * trames et sommait les 21 premiers : le format de la seule 0x08. Une trame
 * d'état de dix octets restait donc en attente, les octets de la suivante
 * complétaient le tampon, la somme échouait toujours, et la liaison était
 * déclarée morte en permanence.
 */
#define RF_RX_MAX 23 /* le tampon d'usine, IDATA 0x54-0x6A */

#define RF_RX_LEN_STATUS 10
#define RF_RX_LEN_BULK   22

#define RF_RX_TYPE_STATUS   0x02
#define RF_RX_TYPE_ANNOUNCE 0x03
#define RF_RX_TYPE_BULK     0x08

#define RF_HDR      0x01 /* octet 0, constant dans les deux sens */
#define RF_SUM_SEED 0x55 /* somme = 0x55 - Σ, même constante aux deux sens */

/* Commandes, relevées une par une sur leurs douze émetteurs. */
#define RF_CMD_LINK     0x01 /* slot + drapeau d'appairage */
#define RF_CMD_REPORT_L 0x02 /* trame longue : 27 o de charge */
#define RF_CMD_REPORT_S 0x03 /* trame courte : 10 o de charge */
#define RF_CMD_STATUS   0x06 /* requête d'état -> réponse 02 06 ... */
#define RF_CMD_SETTINGS 0x0B /* un paramètre ; émis en quittant le sans-fil */
#define RF_CMD_WIRED    0x0E /* passage en filaire */

/*
 * Non émises ici, mais relevées sur le firmware d'usine : 0x04 (un paramètre,
 * envoyé périodiquement avec la valeur 3), 0x08 (conteneur à sous-commandes :
 * batterie, réglages, relecture de la flash), 0x09 (nom Bluetooth, 32 o), 0x0C
 * (deux paramètres, émis depuis la routine de veille) et 0x0D (pourcentage de
 * batterie). Elles demandent des données que ce portage n'a pas : SMK ne lit
 * pas la tension de batterie sur ce clavier, et aucun nom Bluetooth n'est
 * configurable ici.
 */

/* Longueurs totales, somme de contrôle comprise. */
#define RF_LEN_SHORT     6
#define RF_LEN_REPORT_L  30
#define RF_LEN_REPORT_S  13

/*
 * Drapeau de la commande 0x01. Le firmware d'usine émet 0 à l'entrée dans un
 * mode (`fcn.000084E9`) et 1 depuis le tic lent quand le lien doit être
 * réaffirmé (`fcn.0000870C`, sur le bit 0x2C.5 que pose le raccourci
 * d'appairage). D'où la lecture « 1 = relancer l'appairage » — cohérente, mais
 * non prouvée : seule la différence des deux sites l'établit.
 */
#define RF_LINK_SELECT  0
#define RF_LINK_PAIRING 1

static __xdata uint8_t          tx_buf[RF_TX_MAX];
static volatile __data uint8_t  tx_len;
static volatile __data uint8_t  tx_idx;
static volatile __bit           tx_busy;

static __xdata uint8_t         rx_buf[RF_RX_MAX];
static volatile __data uint8_t rx_idx;
static volatile __bit          rx_pending;

/*
 * Seuls tx_len/tx_idx/rx_idx restent en DATA : l'ISR les touche à chaque octet.
 * Le reste part en XDATA — la RAM interne du 8051 est la ressource rare, et à
 * 260 kbauds un octet dure ~38 us, mille fois le coût d'un accès XDATA.
 */
static __xdata rf_link_t link_state;
static __xdata uint8_t   bt_slot;
static __bit             link_connected;

/*
 * Commande 0x01 en attente. Le firmware d'usine l'émet une seule fois, au
 * moment du changement de transport, et la perd si `P4.7` est bas à cet
 * instant-là (son prologue d'émission refuse alors la trame sans rien
 * réessayer). On garde l'intention à la place, et `rf_task()` la rejoue dès que
 * le module se déclare prêt : c'est le seul endroit où ce portage s'écarte du
 * firmware d'usine pour corriger un trou, et il est sans effet quand le module
 * répond tout de suite.
 */
static __bit           link_tx_pending;
static __xdata uint8_t link_tx_flag;

/*
 * Sonde de présence, transcrite de `fcn.00003901` : la commande 0x06 part tous
 * les cent passages et, au bout de TROIS sondes sans réponse, le firmware
 * d'usine relâche la ligne d'émission (`P0CR &= 0xFB ; P0.2 = 1`) et retombe le
 * bit 0x2C.1 — son drapeau « liaison vivante ».
 *
 * Son compteur tourne sur la cadence de `euart0_parse`, donc sur le trafic
 * reçu ; ici il tourne sur les passages de `rf_task()`, dont la cadence n'est
 * pas calibrée. Le seuil est donc pris large, pour la même raison que
 * l'anti-rebond du sélecteur.
 */
#define RF_PROBE_PERIOD 2000
#define RF_PROBE_MISSES 3
static __xdata uint16_t probe_ticks;
static __xdata uint8_t  probe_misses;
static __bit            probe_answered;

/*
 * Compteurs d'anti-rebond du sélecteur, un par position (0x08BD/0x02DF/0x0960).
 *
 * Le firmware d'usine exige dix passages consécutifs de son tic lent. SMK n'a
 * pas de base de temps équivalente ici — `rf_task()` est appelée depuis la
 * boucle principale, dont la cadence n'est pas calibrée. Le seuil est donc
 * exprimé en PASSAGES DE BOUCLE, pas en millisecondes, et pris large : le rôle
 * du compteur est d'absorber le rebond du contact, et un glissement de mode ne
 * demande pas de réactivité.
 */
#define RF_DEBOUNCE 250
static __xdata uint16_t deb_wired;
static __xdata uint16_t deb_24g;
static __xdata uint16_t deb_bt;

/* --------------------------------------------------------------------- ISR */

/*
 * Vecteur 0x6B (_INT_EUART0 = 13). L'ISR ne fait que pousser l'octet suivant et
 * ramasser ce qui arrive : elle n'appelle aucune fonction partagée avec la
 * boucle principale, pour que le recouvrement de variables locales de SDCC
 * reste sain (c'est ce que vérifie la cible `check_interrupts`).
 */
void rf_euart0_interrupt_handler(void) __interrupt(_INT_EUART0)
{
    if (TI) {
        TI = 0;
        if (tx_idx < tx_len) {
            SBUF = tx_buf[tx_idx++];
        } else {
            /*
             * Fin de rafale : P0.2 repasse en entrée et est relâché haut
             * (transcrit de l'ISR d'usine, 0xA580).
             */
            P0CR &= (uint8_t)~0x04;
            P0_2    = 1;
            tx_busy = 0;
        }
    }

    if (RI) {
        RI = 0;
        /*
         * Empiler et signaler, rien d'autre — comme l'ISR d'usine, qui jette
         * silencieusement ce qui dépasse son tampon. Le découpage appartient à
         * `rf_rx_consume()`, seul endroit qui connaisse le format des types.
         */
        const uint8_t byte = SBUF;
        if (rx_idx < RF_RX_MAX) {
            rx_buf[rx_idx++] = byte;
        }
        rx_pending = 1;
    }
}

/* ----------------------------------------------------------------- émission */

/*
 * Prologue commun aux douze émetteurs d'usine : ne rien envoyer si une rafale
 * est déjà en cours (bit 0x2C.1) ou si le module n'est pas prêt (P4.7 bas).
 */
static bool rf_can_send(void)
{
    return !tx_busy && P4_7;
}

/*
 * `len` est la longueur TOTALE, somme de contrôle comprise. Les octets 0 à
 * len-2 doivent déjà être posés dans tx_buf.
 */
static bool rf_send_frame(uint8_t len)
{
    uint8_t sum = RF_SUM_SEED;

    if (!rf_can_send() || len < 2 || len > RF_TX_MAX) {
        return false;
    }

    for (uint8_t i = 0; i < (uint8_t)(len - 1); i++) {
        sum -= tx_buf[i];
    }
    tx_buf[len - 1] = sum;

    tx_len = len;
    tx_idx = 1; /* l'octet 0 part tout de suite, l'ISR enchaîne */
    tx_busy = 1;

    /*
     * Requête d'émission : P0.2 est tiré bas AVANT de le basculer en sortie,
     * puis re-tiré bas — l'ordre exact du firmware d'usine (0xAB13), qui évite
     * un front haut parasite au moment du changement de direction.
     */
    P0_2 = 0;
    P0CR |= 0x04;
    P0_2 = 0;

    SBUF = tx_buf[0];
    return true;
}

static void rf_frame_begin(uint8_t cmd)
{
    memset(tx_buf, 0, RF_TX_MAX);
    tx_buf[0] = RF_HDR;
    tx_buf[1] = cmd;
}

static bool rf_send_short(uint8_t cmd, uint8_t p0, uint8_t p1)
{
    if (!rf_can_send()) {
        return false;
    }
    rf_frame_begin(cmd);
    tx_buf[2] = p0;
    tx_buf[3] = p1;
    return rf_send_frame(RF_LEN_SHORT);
}

/*
 * Commande 0x01 : le sélecteur de radio. C'est ELLE qui met le module en
 * 2,4 GHz — `fcn.0000ED89`, appelée via `fcn.0000EF5A`.
 *
 *   01 01 <slot> <drapeau> 00 <somme>
 *
 * Le slot 0 est le dongle 2,4 GHz, 1 à 3 les trois emplacements Bluetooth.
 * Établi par `fcn.0000870C`, qui choisit `slot = (transport == 2) ? bt_slot : 0`
 * — donc le seul slot possible en mode 1 est 0.
 */
static bool rf_send_link(rf_slot_t slot, uint8_t flag)
{
    return rf_send_short(RF_CMD_LINK, (uint8_t)slot, flag);
}

static rf_slot_t rf_link_slot(void)
{
    return (link_state == RF_LINK_BT) ? (rf_slot_t)bt_slot : RF_SLOT_24G;
}

static void rf_queue_link(uint8_t flag)
{
    link_tx_pending = 1;
    link_tx_flag    = flag;
}

static bool rf_send_status_probe(void)
{
    return rf_send_short(RF_CMD_STATUS, 0, 0);
}

/* ------------------------------------------------------- trames de frappe */

/*
 * Les deux trames de frappe sortent de la même file circulaire dans le firmware
 * d'usine (six emplacements de 28 octets en `0x0C57`) ; l'octet 0 de
 * l'emplacement EST l'octet de commande, 2 ou 3, et décide de la longueur.
 * Ici la file de SMK suffit : on émet directement.
 *
 * INFÉRÉ : la disposition des octets de charge. Ce qui est établi, c'est la
 * taille (27 et 10 octets utiles) et le fait que la file est alimentée par les
 * MÊMES drapeaux que la chaîne USB — donc que le contenu est celui des rapports
 * HID. Le placement à l'offset 0 est le choix le plus naturel, pas une lecture.
 */
static bool rf_send_payload_long(const __xdata uint8_t *data, uint8_t len)
{
    if (!rf_can_send() || len > (RF_LEN_REPORT_L - 3)) {
        return false;
    }
    rf_frame_begin(RF_CMD_REPORT_L);
    for (uint8_t i = 0; i < len; i++) {
        tx_buf[2 + i] = data[i];
    }
    return rf_send_frame(RF_LEN_REPORT_L);
}

static bool rf_send_payload_short(const __xdata uint8_t *data, uint8_t len)
{
    if (!rf_can_send() || len > (RF_LEN_REPORT_S - 3)) {
        return false;
    }
    rf_frame_begin(RF_CMD_REPORT_S);
    for (uint8_t i = 0; i < len; i++) {
        tx_buf[2 + i] = data[i];
    }
    return rf_send_frame(RF_LEN_REPORT_S);
}

void rf_send_report(__xdata report_keyboard_t *report)
{
    (void)rf_send_payload_long(report->raw, KEYBOARD_REPORT_SIZE);
}

void rf_send_nkro(__xdata report_nkro_t *report)
{
    (void)rf_send_payload_long(report->raw, NKRO_REPORT_SIZE);
}

void rf_send_extra(__xdata report_extra_t *report)
{
    (void)rf_send_payload_short(report->raw, EXTRA_REPORT_SIZE);
}

/* ---------------------------------------------------------------- réception */

/* Somme d'usine : 0x55 - Σ des `len` premiers octets, comparée à l'octet `len`. */
static bool rf_rx_checksum_ok(uint8_t len)
{
    uint8_t sum = RF_SUM_SEED;

    for (uint8_t i = 0; i < len; i++) {
        sum -= rx_buf[i];
    }
    return sum == rx_buf[len];
}

/* Vide le tampon, l'IRQ masquée : sans ça un octet arrivé entre le test et la
 * remise à zéro se retrouverait attribué à la trame suivante. */
static void rf_rx_drop(void)
{
    const bool armed = (IEN1 & _ES0) != 0;

    IEN1 &= (uint8_t)~_ES0;
    rx_idx     = 0;
    rx_pending = 0;
    if (armed) {
        IEN1 |= _ES0;
    }
}

static void rf_rx_consume(void)
{
    if (!rx_pending) {
        return;
    }

    switch (rx_buf[0]) {
        case RF_RX_TYPE_STATUS:
            if (rx_idx < RF_RX_LEN_STATUS) {
                return; /* trame incomplète : on laisse le reste arriver */
            }
            /*
             * Les deux gardes du firmware d'usine, avant même la somme :
             * l'octet 1 vaut 6 (c'est la réponse à la commande 0x06) et
             * l'octet 2 est imposé à 0.
             */
            if (rx_buf[1] == RF_CMD_STATUS && rx_buf[2] == 0 &&
                rf_rx_checksum_ok(RF_RX_LEN_STATUS - 1)) {
                /*
                 * INFÉRÉ : « une trame d'état valide vient d'arriver » vaut
                 * preuve de lien. Le champ qui porte vraiment la connexion est
                 * l'octet 5, mais le firmware d'usine ne le lit que dans une
                 * condition composée avec l'octet 4 — le décoder demande de
                 * distinguer les transports, ce qui viendra avec la batterie.
                 */
                link_connected = 1;
                probe_answered = 1;
            }
            break;

        case RF_RX_TYPE_BULK:
            if (rx_idx < RF_RX_LEN_BULK) {
                return;
            }
            (void)rf_rx_checksum_ok(RF_RX_LEN_BULK - 1);
            /* Conteneur à sous-commandes : rien n'en dépend dans ce portage. */
            break;

        case RF_RX_TYPE_ANNOUNCE:
            /*
             * Annonce de connexion. Le firmware d'usine ne vérifie aucune somme
             * ici et sa longueur n'est pas établie ; on se contente du type, qui
             * prouve que le module parle.
             */
            probe_answered = 1;
            break;

        default:
            /* Type inconnu : on jette plutôt que de tenter un recalage. */
            break;
    }

    rf_rx_drop();
}

/* ------------------------------------------------------- machine à états */

static void rf_uart_init(void)
{
    SCON  = RF_SCON;
    SBRTH = RF_SBRTH;
    SBRTL = RF_SBRTL;
    SFINE = RF_SFINE;
    PCON |= _SSTAT; /* active les drapeaux d'erreur de réception */
    SADDR = 0x00;
    SADEN = 0x00;

    TI = 0;
    RI = 0;

    tx_busy  = 0;
    tx_idx   = 0;
    tx_len   = 0;
    rx_idx     = 0;
    rx_pending = 0;

    /* Handshake au repos : P0.2 en entrée, relâché. */
    P0CR &= (uint8_t)~0x04;
    P0_2 = 1;
}

/*
 * Attend la fin de la rafale en cours avant de couper l'IRQ : sans cela une
 * trame encore en vol laisserait tx_busy armé pour toujours et P0.2 bloqué bas
 * en sortie, ce qui pend la liaison. Borné, parce qu'un module absent ne
 * répondra jamais.
 */
static void rf_tx_drain(void)
{
    /* 32 octets à ~260 kbauds ~= 1,3 ms ; dix millisecondes sont larges. */
    for (uint8_t i = 0; tx_busy && i < 10; i++) {
        delay_ms(1);
    }
    if (tx_busy) {
        tx_busy = 0;
        P0CR &= (uint8_t)~0x04;
        P0_2 = 1;
    }
}

/*
 * Armement de la radio, transcrit du thunk d'usine 0xEF40 — quatre
 * instructions, appelé par `fcn.000084E9` en 0x8581 et 0x85D5 :
 *
 *   ef40  ORL  IEN1,#0x40      IEN1 |= _ES0      -- l'IRQ EUART0 s'arme
 *   ef43  ANL  USBCON,#0x7F    USBCON &= ~_ENUSB -- le module USB s'ÉTEINT
 *   ef46  SETB 0x3f            drapeau interne partagé, hors sujet ici
 *   ef48  MOV  R7,#0x14
 *   ef4a  LJMP 0xED03          ~20 ms
 *
 * La deuxième instruction manquait à une première rédaction de ce fichier : en
 * sans-fil le firmware d'usine COUPE le périphérique USB, il ne se contente pas
 * d'ignorer l'hôte.
 *
 * On appelle `usb_hw_deinit()` et NON `usb_deinit()`. Les deux retombent
 * `_ENUSB | _SW1CON | _SW2CON` et désarment l'interruption USB, mais
 * `usb_deinit()` remet en plus toute la machine à états logicielle à zéro — dont
 * `interface0_protocol`. Or `host_nkro_active()` (`src/smk/host.c:12`) exige
 * `USB_PROTOCOL_REPORT` : le remettre à zéro fait retomber le clavier en 6KRO
 * pour toute la durée du mode sans-fil, alors que `NKRO_REPORT_BITS` vaut 20
 * précisément « limited by wireless dongle hid descriptor » (`report.h:10`).
 *
 * Ne couper que le matériel est donc à la fois plus fidèle au firmware d'usine —
 * qui n'exécute qu'un `anl USBCON,#0x7F` — et la seule façon de garder le NKRO
 * sur la radio. L'état logiciel est reconstruit par `usb_init()` au retour au
 * filaire.
 */
static void rf_radio_on(void)
{
    IEN1 |= _ES0;
    usb_hw_deinit();
    delay_ms(20);
}

static void rf_radio_off(void)
{
    rf_tx_drain();
    IEN1 &= (uint8_t)~_ES0;
}

/*
 * Entrée en 2,4 GHz, transcrite de `fcn.000084E9` (branche du compteur 0x02DF).
 * C'est la réponse à la question restée ouverte « qui met le transport à 1 » :
 * personne ne l'écrit directement, c'est cette branche du sélecteur qui le fait,
 * et elle prévient le module par la commande 0x01 avec le slot 0.
 */
static void rf_enter_24g(void)
{
    if (link_state == RF_LINK_WIRED) {
        delay_ms(20); /* 0xEF8D : un temps de garde avant de couper l'USB */
    }
    link_state     = RF_LINK_24G;
    link_connected = 0;
    rf_radio_on();
    rf_queue_link(RF_LINK_SELECT);
}

static void rf_enter_bt(void)
{
    if (link_state == RF_LINK_WIRED) {
        delay_ms(20);
    }
    link_state = RF_LINK_BT;
    if (bt_slot < RF_SLOT_BT1 || bt_slot > RF_SLOT_BT3) {
        bt_slot = RF_SLOT_BT1; /* borne d'usine : 1..3 */
    }
    link_connected = 0;
    rf_radio_on();
    rf_queue_link(RF_LINK_SELECT);
}

/*
 * Retour au filaire, transcrit de `fcn.000084E9` @ 0x8543 et de `fcn.0000EED1` :
 * on prévient le module (0x0E puis 0x0B avec 0), puis on coupe l'IRQ EUART0 et
 * on réinitialise l'USB.
 */
static void rf_enter_wired(void)
{
    delay_ms(20);
    (void)rf_send_short(RF_CMD_WIRED, 0, 0);
    delay_ms(10);
    (void)rf_send_short(RF_CMD_SETTINGS, 0, 0);
    delay_ms(10);

    link_state      = RF_LINK_WIRED;
    link_connected  = 0;
    link_tx_pending = 0;
    rf_radio_off();
    usb_init();
    /*
     * 200 ms bloquants, comme le firmware d'usine. `delay_us` donne un coup de
     * chien de garde à chaque microseconde, donc le WDT ne mord pas — mais le
     * balayage matrice et l'USB s'arrêtent pendant ce temps. C'est un
     * changement de mode, il n'arrive pas en frappe.
     */
    delay_ms(200);
}

/*
 * Sélecteur à glissière trois positions.
 *
 *   P7.4 = 0            -> 2,4 GHz
 *   P7.4 = 1, P4.5 = 1  -> filaire
 *   P7.4 = 1, P4.5 = 0  -> Bluetooth
 *
 * Les deux broches sortent de `jb 0xF8.4` et `jb 0xB0.5` en tête de
 * `fcn.000084E9` ; 0xF8 est P7 et 0xB0 est P4 d'après `sh68f90.h`. Elles
 * figuraient jusqu'ici parmi les entrées « rôle inconnu » du portage.
 */
static rf_link_t rf_read_selector(void)
{
    if (!P7_4) {
        return RF_LINK_24G;
    }
    return P4_5 ? RF_LINK_WIRED : RF_LINK_BT;
}

static void rf_sample_selector(void)
{
    if (P7_4) {
        if (P4_5) {
            deb_wired++;
            deb_24g = 0;
            deb_bt  = 0;
        } else {
            deb_bt    = deb_bt + 1;
            deb_wired = 0;
            deb_24g   = 0;
        }
    } else {
        deb_24g   = deb_24g + 1;
        deb_wired = 0;
        deb_bt    = 0;
    }

    if (deb_wired >= RF_DEBOUNCE) {
        deb_wired = 0;
        if (link_state != RF_LINK_WIRED) {
            rf_enter_wired();
        }
    } else if (deb_24g >= RF_DEBOUNCE) {
        deb_24g = 0;
        if (link_state != RF_LINK_24G) {
            rf_enter_24g();
        }
    } else if (deb_bt >= RF_DEBOUNCE) {
        deb_bt = 0;
        if (link_state != RF_LINK_BT) {
            rf_enter_bt();
        }
    }
}

/* ------------------------------------------------------------------ public */

void rf_init(void)
{
    link_state      = RF_LINK_WIRED;
    bt_slot         = RF_SLOT_BT1;
    link_connected  = 0;
    link_tx_pending = 0;
    probe_ticks     = 0;
    probe_misses    = 0;
    probe_answered  = 0;
    deb_wired = deb_24g = deb_bt = 0;

    rf_uart_init();
    IEN1 &= (uint8_t)~_ES0; /* armée seulement quand un mode sans-fil est actif */

    /*
     * Le transport de départ est LU SUR LE SÉLECTEUR, pas supposé filaire.
     *
     * Le firmware d'usine fait autrement : il part de `g_transport = 0` et
     * laisse l'anti-rebond de `fcn.000084E9` converger. Ce portage ne reproduit
     * pas ce comportement, parce que pendant la convergence `kb_send_report()`
     * pousserait les frappes sur l'USB alors que la glissière dit « sans fil ».
     * Le sélecteur est la seule autorité sur le transport : autant l'interroger
     * tout de suite.
     *
     * L'ordre de `main()` le permet : `rf_init()` est appelée par `kb_init()`,
     * donc après `usb_init()` et avant `usb_wait_for_enumeration()`. Couper
     * l'USB ici ne bloque rien — cette attente rend la main au bout de 500 ms
     * (`ENUM_NO_HOST_MS`) quand aucun SETUP n'arrive.
     */
    switch (rf_read_selector()) {
        case RF_LINK_24G:
            rf_enter_24g();
            break;
        case RF_LINK_BT:
            rf_enter_bt();
            break;
        case RF_LINK_WIRED:
        default:
            break;
    }
}

/*
 * Sonde de présence : commande 0x06 périodique, trois échecs et la liaison est
 * déclarée morte. Transcrit de `fcn.00003901` ; voir RF_PROBE_PERIOD.
 */
static void rf_probe_task(void)
{
    if (++probe_ticks < RF_PROBE_PERIOD) {
        return;
    }
    probe_ticks = 0;

    if (probe_answered) {
        probe_misses = 0;
    } else if (probe_misses < RF_PROBE_MISSES) {
        probe_misses++;
    }

    if (probe_misses >= RF_PROBE_MISSES) {
        probe_misses   = 0;
        link_connected = 0;
        /* Le firmware d'usine relâche ici la ligne d'émission (0x3D5A). */
        rf_tx_drain();
        /*
         * Et on jette ce qui traîne en réception : une trame tronquée dont le
         * type annonce une longueur qui n'arrivera jamais bloquerait le tampon
         * indéfiniment, puisque `rf_rx_consume()` attend le compte.
         */
        rf_rx_drop();
    }

    probe_answered = 0;
    (void)rf_send_status_probe();
}

void rf_task(void)
{
    rf_rx_consume();
    rf_sample_selector();

    if (rf_is_wireless()) {
        if (link_tx_pending && rf_can_send()) {
            link_tx_pending = 0;
            (void)rf_send_link(rf_link_slot(), link_tx_flag);
        }
        rf_probe_task();
    }

    /*
     * `paired` n'est pas alimenté : aucun champ de la trame d'état n'a été
     * identifié comme portant l'appairage, et le déduire de `connected` serait
     * une invention. `keyboard.c` le laisse à zéro.
     */
    keyboard_state.rf_link   = (uint8_t)rf_link();
    keyboard_state.connected = rf_connected() ? 1 : 0;
}

rf_link_t rf_link(void)
{
    return link_state;
}

bool rf_is_wireless(void)
{
    return link_state != RF_LINK_WIRED;
}

bool rf_connected(void)
{
    return link_connected;
}

uint8_t rf_bt_slot(void)
{
    return bt_slot;
}

void rf_set_bt_slot(uint8_t slot)
{
    if (slot < RF_SLOT_BT1 || slot > RF_SLOT_BT3) {
        return;
    }
    bt_slot = slot;
    if (link_state == RF_LINK_BT) {
        link_connected = 0;
        rf_queue_link(RF_LINK_SELECT);
    }
}

void rf_request_pairing(void)
{
    if (rf_is_wireless()) {
        rf_queue_link(RF_LINK_PAIRING);
    }
}
