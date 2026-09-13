#!/usr/bin/env python3
"""Entrée en ISP du GM610 par transfert de contrôle BRUT — sans hidapi.

    sudo python3 utils/gm610_enter_isp_usb.py

À utiliser quand la voie HID échoue : `hid` absent sous sudo, hidraw qui refuse,
ou une pile HID qui se comporte mal. Ici on parle à libusb, donc on court-circuite
toute la couche HID de l'hôte.

La requête est celle qu'un SET_REPORT produit, à l'octet près :

    bmRequestType 0x21   hôte -> périphérique, classe, destinataire = interface
    bRequest      0x09   SET_REPORT
    wValue        0x0305 type FEATURE (3), report ID 5
    wIndex        <interface portant la collection vendeur>
    données       05 75 00 00 00 00

C'est très exactement ce que le firmware teste (src/smk/usb.c, usb_ep0_out_irq) :

    if (EP0_OUT_BUF[0] == 0x05 && EP0_OUT_BUF[1] == 0x75) usb_isp_requested = 1;

Aucun opcode d'écriture flash n'existe dans ce fichier.

Dépendance :  pip install pyusb   (et libusb présent, ce qui est le cas partout)
"""

import sys
import time

VID, PID = 0x12C9, 0x6001
BOOT = (0x0603, 0x1020)

SET_REPORT = 0x09
TYPE_FEATURE = 0x03
REPORT_ID = 0x05
CMD_ENTER_ISP = 0x75


def main():
    try:
        import usb.core
        import usb.util
    except ImportError:
        sys.exit("module « usb » manquant :  sudo pip install pyusb")

    print("1. recherche de %04x:%04x" % (VID, PID))
    dev = usb.core.find(idVendor=VID, idProduct=PID)
    if dev is None:
        sys.exit("   ✗ absent du bus.")
    print("   trouvé : bus %s adresse %s" % (dev.bus, dev.address))

    # Les interfaces qui portent une collection HID vendeur. On les essaie toutes :
    # une seule acceptera, les autres renverront une erreur sans rien casser.
    interfaces = []
    try:
        for cfg in dev:
            for itf in cfg:
                interfaces.append(itf.bInterfaceNumber)
    except Exception as e:
        print("   ! lecture de la configuration impossible (%s) ; on essaiera 0 et 1" % e)
    interfaces = sorted(set(interfaces)) or [0, 1]
    print("   interfaces : %s" % interfaces)

    trame = bytes([REPORT_ID, CMD_ENTER_ISP, 0, 0, 0, 0])
    wValue = (TYPE_FEATURE << 8) | REPORT_ID

    print("\n2. envoi de SET_REPORT(FEATURE, id=5) = %s" % trame.hex(" "))
    envoye = False
    for itf in interfaces:
        try:
            if dev.is_kernel_driver_active(itf):
                dev.detach_kernel_driver(itf)
                print("   itf=%d : pilote noyau détaché" % itf)
        except Exception:
            pass  # pas toujours nécessaire, jamais bloquant
        try:
            n = dev.ctrl_transfer(0x21, SET_REPORT, wValue, itf, trame, timeout=2000)
            print("   itf=%d : %d octets acceptés ✅" % (itf, n))
            envoye = True
            break
        except Exception as e:
            print("   itf=%d : %s" % (itf, e))

    if not envoye:
        print("\n   ✗ aucune interface n'a accepté. Sans root, libusb refuse : sudo.")

    print("\n3. attente du bootloader %04x:%04x (15 s)" % BOOT)
    for i in range(30):
        if usb.core.find(idVendor=BOOT[0], idProduct=BOOT[1]) is not None:
            print("   ✅ BOOTLOADER PRÉSENT après %.1f s" % (i * 0.5))
            return 0
        if i == 6 and usb.core.find(idVendor=VID, idProduct=PID) is None:
            print("   ... l'application a disparu du bus : le basculement a eu lieu")
        time.sleep(0.5)

    print("   ✗ le bootloader n'est pas apparu.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
