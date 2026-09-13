#pragma once

#include "sh68f90.h"
#include "keycodes.h"

/*
 * Newmen GM610 -- carte SONDE de mise en route.
 *
 * Son seul but est de prouver le trajet aller-retour vers le bootloader ISP
 * d'usine sur le vrai matériel : démarrer, énumérer en USB, accepter la requête
 * de contrôle qui appelle `isp_jump()`, et revenir au bootloader.
 *
 * ⚠️ ELLE NE CONFIGURE NI NE PILOTE AUCUN GPIO. Pas de matrice, pas de PWM, pas
 * de liaison radio, pas de veille. Toutes les broches restent dans leur état de
 * reset -- en entrée. C'est délibéré : le seul moyen d'abîmer ce clavier par le
 * logiciel est de piloter des broches, et cette carte n'en pilote aucune.
 *
 * Cela compte doublement ici : sur le GM610, P5 porte À LA FOIS des colonnes de
 * matrice (bits 0-2) et des lignes (bits 3-4). Une sonde qui piloterait P5 sans
 * précaution mettrait en conflit une sortie et une entrée.
 *
 * ── Le bootloader d'usine reste en place ────────────────────────────────────
 *
 * `0xF000`-`0xFFFF` n'est jamais écrit : la plateforme sh68f90 de SMK réserve
 * déjà cette zone (`bootloader_addr`), et `nvm.c` garde ses réglages sous
 * `FLASH_MARKER_ADDR` (0xEE00) par assertion statique. Le secteur 0xEE00-0xEFFF
 * -- qui contient l'octet d'armement `0xEFFB` lu par le bootloader d'usine en
 * 0xF017 -- n'est donc écrit par personne.
 *
 * Récupération : tant que `CODE[0xEFFB] != 0x02`, le bootloader reste en ISP
 * indéfiniment et le clavier est récupérable sans condition. Il n'existe aucune
 * entrée matérielle : la récupération passe par l'USB, en 0603:1020.
 *
 * ── Flasher comme une mise à jour ───────────────────────────────────────────
 *
 * L'updater officiel (« GM610 firmware.exe ») accepte un fichier Intel HEX
 * externe et fait lui-même le chiffrement, l'effacement, l'écriture ET
 * l'armement. Donner le .hex produit ici revient donc exactement à appliquer
 * une mise à jour d'usine. Contraintes du parseur, lues dans l'exe (0x00405740) :
 *
 *   - tampon de 0xF000 octets, prérempli à 0x00  (pas 0xFF)
 *   - toute adresse > 0xEFFF est refusée : « 對應目標文件超過60K！ »
 *   - adresses 16 bits uniquement, fin sur l'enregistrement `:00000001FF`
 *
 * Voir docs/GM610_PORTAGE_SMK.md.
 */

/*
 * 5 lignes physiques x 14 colonnes.
 *
 * Le firmware d'usine indexe ses touches sur 6 lignes, mais sa « ligne 0 » est
 * la couche Fn, pas une ligne câblée : sa routine de lecture (0x8309) ne pose
 * jamais le bit 0. Le GM610 est un 60 % à cinq rangées.
 *
 * Rien ne lit ces dimensions sur cette carte ; elles servent à dimensionner la
 * géométrie LED générée et les tampons de matrice.
 */
#define MATRIX_ROWS 5
#define MATRIX_COLS 14
