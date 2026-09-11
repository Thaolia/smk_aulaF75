#include "kbdef.h"
#include "user_sleep.h"

#ifdef SLEEP_ENABLE

/*
 * USER_SLEEP_NONE : `sleep_due()` rend alors `false` sans condition, donc
 * `power_enter_powerdown()` n'est jamais atteint. Une sonde qui s'endort sans
 * source de réveil armée resterait figée jusqu'au débranchement.
 */
user_sleep_mode_t user_sleep_supported(void)
{
    return USER_SLEEP_NONE;
}

void user_sleep_prepare(void) {}
void user_sleep_wake(void) {}


/* Rien à signaler avant l'endormissement sur cette carte : voir `user_sleep.h`. */
bool user_sleep_ready(void)
{
    return true;
}

void user_sleep_cancel(void)
{
}

#endif
