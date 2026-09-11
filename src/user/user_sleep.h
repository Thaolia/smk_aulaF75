#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    USER_SLEEP_NONE = 0,
    USER_SLEEP_RF   = 1,
    USER_SLEEP_USB  = 2,
} user_sleep_mode_t;

user_sleep_mode_t user_sleep_supported(void);
void              user_sleep_prepare(void);
void              user_sleep_wake(void);

/*
 * La carte est-elle prête à s'endormir MAINTENANT ?
 *
 * `sleep_task()` coupe le tick et le PWM avant d'appeler `user_sleep_prepare()`,
 * donc plus rien ne s'affiche à partir de là : une carte qui veut signaler
 * l'endormissement -- un voyant qui s'éteint en fondu, par exemple -- doit le
 * faire AVANT. Rendre `false` retarde la veille d'un passage de boucle ; le
 * moteur d'affichage tourne toujours pendant ce temps.
 *
 * `user_sleep_cancel()` est appelé à chaque passage où la veille n'est PAS due.
 * C'est ce qui permet à une carte de défaire proprement ce qu'elle a commencé
 * quand l'utilisateur interrompt l'endormissement en cours de route.
 *
 * Une carte sans besoin particulier rend `true` et ne fait rien.
 */
bool user_sleep_ready(void);
void user_sleep_cancel(void);
