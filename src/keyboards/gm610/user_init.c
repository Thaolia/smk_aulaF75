#include "kbdef.h"
#include "user_init.h"
#include "pwm.h"
#include "gpio.h"

/*
 * Période PWM. Le firmware d'usine charge 3000 (0x0BB8) dans les paires B et
 * ne se sert que de DUTY2 ; SMK utilise 0x0100 = 256, ce qui fait correspondre
 * un octet de tampon à un rapport cyclique sans division. On garde 256.
 */
#define PWM_PERD 0x0100

#define PWM_PERDH_INIT ((uint8_t)(PWM_PERD >> 8))
#define PWM_PERDL_INIT ((uint8_t)(PWM_PERD))

void user_gpio_init(void);
void user_pwm_init(void);

void user_init(void)
{
    user_gpio_init();
    user_pwm_init();
}

void user_gpio_init(void)
{
    // Les colonnes tirent la matrice ET alimentent les LED : courant relevé.
    DRVCON = DRVCON_UNLOCK_P2;
    P2DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_UNLOCK_P3;
    P3DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_UNLOCK_P5;
    P5DRV  = GPIO_DRIVE_25MA;

    DRVCON = DRVCON_LOCK;

    /*
     * ⚠️ P5 porte trois choses : colonnes 0-2 (bits 0-2, sortie), lignes 3-4
     * (bits 3-4, entrée), puits bleu de la rangée 2 (bit 7, sortie).
     * GPIO_DIR_WRITE écrit TOUT le registre : les bits absents restent en
     * entrée, ce qui est exactement voulu pour les lignes.
     *
     * Le firmware d'usine pose la même valeur : P5CR = 0x87.
     */
    GPIO_DIR_WRITE(5, (uint8_t)(KB_C_P5_MASK | RGB_P5_MASK));
    GPIO_DIR_WRITE(3, KB_C_P3_MASK);
    GPIO_DIR_WRITE(2, KB_C_P2_MASK);
    GPIO_DIR_WRITE(0, RGB_P0_MASK);
    GPIO_DIR_WRITE(4, RGB_P4_MASK);
    GPIO_DIR_WRITE(6, RGB_P6_MASK);

    GPIO_PULLUP_WRITE(7, KB_R_P7_MASK);
    GPIO_PULLUP_WRITE(5, KB_R_P5_MASK);

#ifdef RF_ENABLED
    /*
     * `bb_spi.c` pilote ces lignes en drain ouvert : il bascule PxCR à chaque
     * front et compte sur un niveau haut au repos. On verrouille donc le haut
     * et on laisse PxCR à son va-et-vient.
     *
     * ⚠️ P7.4 (CS) et P4.7 (SCK) s'ajoutent aux directions déjà écrites plus
     * haut ; GPIO_DIR_WRITE(4, ...) a mis P4CR à RGB_P4_MASK, donc P4.7 y est
     * absent -- c'est voulu, bb_spi.c le met en sortie lui-même au moment du
     * front.
     */
    GPIO_HIGH(7, RF_BB_SPI_CS_P7_4);
    GPIO_HIGH(4, RF_BB_SPI_SCK_P4_7);
    GPIO_HIGH(0, (uint8_t)(RF_BB_SPI_MOSI_P0_7 | RF_BB_SPI_MOT_P0_5));
    GPIO_PULLUP_ON(0, (uint8_t)(RF_BB_SPI_MISO_P0_6 | RF_BB_SPI_MOSI_P0_7 | RF_BB_SPI_MOT_P0_5));
    GPIO_PULLUP_ON(4, (uint8_t)(RF_BB_SPI_ACK_P4_2 | RF_BB_SPI_SCK_P4_7));
    GPIO_PULLUP_ON(7, RF_BB_SPI_CS_P7_4);
#endif

    // Colonnes au repos : hautes. Puits au repos : bas (aucune LED ne conduit).
    GPIO_HIGH(5, KB_C_P5_MASK);
    GPIO_HIGH(3, KB_C_P3_MASK);
    GPIO_HIGH(2, KB_C_P2_MASK);
    GPIO_LOW(0, RGB_P0_MASK);
    GPIO_LOW(4, RGB_P4_MASK);
    GPIO_LOW(5, RGB_P5_MASK);
    GPIO_LOW(6, RGB_P6_MASK);
}

void user_pwm_init(void)
{
    // Groupes réellement utilisés : 0 (P3), 1 (P2), 4 (P5). Le groupe 2 (P1)
    // porte les colonnes 15-20, absentes de ce clavier.
    PWM0PERDH = PWM_PERDH_INIT;
    PWM0PERDL = PWM_PERDL_INIT;
    PWM1PERDH = PWM_PERDH_INIT;
    PWM1PERDL = PWM_PERDL_INIT;
    PWM4PERDH = PWM_PERDH_INIT;
    PWM4PERDL = PWM_PERDL_INIT;

    SET_PWM_DUTY(LED_PWM_C0, 0, 0);
    SET_PWM_DUTY(LED_PWM_C1, 0, 0);
    SET_PWM_DUTY(LED_PWM_C2, 0, 0);
    SET_PWM_DUTY(LED_PWM_C3, 0, 0);
    SET_PWM_DUTY(LED_PWM_C4, 0, 0);
    SET_PWM_DUTY(LED_PWM_C5, 0, 0);
    SET_PWM_DUTY(LED_PWM_C6, 0, 0);
    SET_PWM_DUTY(LED_PWM_C7, 0, 0);
    SET_PWM_DUTY(LED_PWM_C8, 0, 0);
    SET_PWM_DUTY(LED_PWM_C9, 0, 0);
    SET_PWM_DUTY(LED_PWM_C10, 0, 0);
    SET_PWM_DUTY(LED_PWM_C11, 0, 0);
    SET_PWM_DUTY(LED_PWM_C12, 0, 0);
    SET_PWM_DUTY(LED_PWM_C13, 0, 0);
}
