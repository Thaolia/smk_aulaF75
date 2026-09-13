#include "usbhw.h"
#include "usb.h"
#include "interrupts.h"
#include "report.h"
#include "delay.h"
#include <stdint.h>

#define EP_IN_DRAIN_TRIES 255

static void set_ep1_in_buffer(uint8_t *src, uint8_t len)
{
    if (len > EP1_BUF_SIZE) {
        return;
    }
    for (uint8_t i = 0; i < len; i++) {
        EP1_IN_BUF[i] = src[i];
    }
}

static void set_ep2_in_buffer(uint8_t *src, uint8_t len)
{
    if (len > EP2_BUF_SIZE) {
        return;
    }
    for (uint8_t i = 0; i < len; i++) {
        EP2_IN_BUF[i] = src[i];
    }
}

static void ep1_in_drain(void)
{
    uint8_t tries = 0;
    while (tries < EP_IN_DRAIN_TRIES && (EP1CON & _IEP1RDY)) {
        delay_us(40);
        tries++;
    }
}

static void ep2_in_drain(void)
{
    uint8_t tries = 0;
    while (tries < EP_IN_DRAIN_TRIES && (EP2CON & _IEP2RDY)) {
        delay_us(40);
        tries++;
    }
}

void usb_hw_init(void)
{
    USBADDR = 0;
    USBIE1  = (_OVERIE | _SETUPIE | _SOFIA | _RESMIE | _SUSPIE | _PBRSTIE);
    USBIE2  = (_OEP0IE | _IEP0IE);
    USBCON  = (_ENUSB | _SW1CON);

#ifdef USB_IRQ_HIGH_PRIORITY
    /*
     * USB au niveau 3, le plus haut, pour qu'il préempte l'ISR systick (rendu LED,
     * 2 kHz). Le firmware d'usine du GM610 fait exactement cela : sa routine 0x922C
     * active Timer2, l'épingle au niveau 0, puis monte l'USB seul au niveau 3.
     *
     * Ce levier suppose Timer2 au niveau 0 : c'est la valeur au reset, et rien dans
     * l'arbre n'écrit IPH0/IPL0 -- vérifié. Une carte qui voudrait relever Timer2
     * annulerait ce réglage en silence.
     *
     * ⚠️ L'imbrication d'ISR que cela autorise n'est PAS gratuite : le recouvrement
     * statique de SDCC ne la modélise pas. La carte qui pose cette macro DOIT aussi
     * déclarer son vecteur haute priorité au vérificateur (meson : usb_irq_priority),
     * sinon utils/check_interrupts.py laisse passer une collision USB <-> systick.
     */
    IPH1 |= _PUSBH;
    IPL1 |= _PUSBL;
#endif

    IEN1 |= _EUSB;
}

void usb_hw_deinit(void)
{
    USBADDR = 0;
    USBCON &= ~(_ENUSB | _SW1CON | _SW2CON); // drop the module-enable bits
    IEN1 &= ~_EUSB;                          // disable the USB interrupt
}

void usb_hw_ep1_in_send(uint8_t *src, uint8_t len)
{
    ep1_in_drain();
    set_ep1_in_buffer(src, len);
    SET_EP1_CNT(len);
    SET_EP1_IN_RDY;
}

void usb_hw_ep2_in_send(uint8_t *src, uint8_t len)
{
    ep2_in_drain();
    set_ep2_in_buffer(src, len);
    SET_EP2_CNT(len);
    SET_EP2_IN_RDY;
}

// IEPxRDY reads back the endpoint's busy state here, so completion needs no bookkeeping.
void usb_hw_ep1_in_complete(void) {}

void usb_hw_ep2_in_complete(void) {}

#if DEBUG == 1
bool usb_hw_ep2_in_free(void)
{
    return !(EP2CON & _IEP2RDY);
}

void usb_hw_console_send(const __xdata uint8_t *data, uint8_t len)
{
    EP2_IN_BUF[0] = REPORT_ID_CONSOLE;
    for (uint8_t i = 0; i < CONSOLE_REPORT_SIZE; i++) {
        EP2_IN_BUF[1 + i] = (i < len) ? data[i] : 0;
    }

    SET_EP2_CNT(1 + CONSOLE_REPORT_SIZE);
    SET_EP2_IN_RDY;
}
#endif

void usb_interrupt_handler(void) __interrupt(_INT_USB)
{
    // save/restore INSCON and FLASHCON: SDCC's prologue does not, and a USB IRQ over a main-loop __code access corrupts banking.
    uint8_t saved_inscon   = INSCON;
    uint8_t saved_flashcon = FLASHCON;

    usb_irq_dispatch();

    FLASHCON = saved_flashcon; // restore banking SFRs (see top)
    INSCON   = saved_inscon;
}
