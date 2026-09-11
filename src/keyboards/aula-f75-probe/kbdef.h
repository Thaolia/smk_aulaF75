#pragma once

#include "sh68f90.h"
#include "keycodes.h"

/*
 * AULA F75 -- carte SONDE de mise en route.
 *
 * Son seul but est de prouver le trajet aller-retour vers le bootloader ISP sur
 * le vrai matériel : démarrer, énumérer en USB, accepter la requête de contrôle
 * qui appelle `isp_jump()`, et revenir au bootloader.
 *
 * ⚠️ ELLE NE CONFIGURE NI NE PILOTE AUCUN GPIO. Pas de matrice, pas de PWM, pas
 * d'EUART0, pas de veille. Toutes les broches restent dans leur état de reset --
 * en entrée. C'est délibéré : le seul moyen d'abîmer ce clavier par le logiciel
 * est de piloter des broches, et cette carte n'en pilote aucune.
 *
 * La géométrie reste celle du F75 pour que la géométrie LED générée et les
 * tampons de matrice aient les bonnes tailles ; rien ne les lit.
 */
#define MATRIX_ROWS 6
#define MATRIX_COLS 15
