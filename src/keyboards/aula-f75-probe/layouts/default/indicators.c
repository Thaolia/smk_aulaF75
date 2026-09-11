#include "kbdef.h"
#include "indicators.h"
#include "settings.h"

/* Aucun rétroéclairage : rien n'est allumé, rien n'est piloté. */
void indicators_init(void) {}
void indicators_start(void) {}
void indicators_render(void) {}
void indicators_pre_update(void) {}
void indicators_post_update(void) {}
void indicators_pwm_enable(void) {}
void indicators_pwm_disable(void) {}
void indicators_apply_defaults(void) {}
void indicators_validate_settings(void) {}

bool indicators_update_step(keyboard_state_t *keyboard, uint8_t current_step)
{
    (void)keyboard;
    (void)current_step;
    return true; /* trame « terminée » : c'est ce retour qui nourrit sleep_note_frame */
}
