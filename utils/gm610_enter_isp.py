#!/usr/bin/env python3
"""Fait passer le Newmen GM610 dans son bootloader d'usine. Rien d'autre.

    sudo python3 utils/gm610_enter_isp.py

Une seule commande est envoyée : `0x75`, l'entrée en ISP. Elle N'ÉCRIT PAS en
flash -- elle demande à l'application de poser le sésame (A=0x5A, B=0xA5) et de
sauter en 0xFF00. Aucun autre opcode n'existe dans ce fichier : ni l'effacement
(0x45), ni l'initialisation d'écriture (0x57), ni l'armement (0x55).

Volontairement séparé de `gm610_ispdump.py` : celui-ci ne lit aucun descripteur
de rapport et ne dépend d'aucune fonction d'analyse, donc il ne peut pas échouer
pour une raison sans rapport avec ce qu'on lui demande.

Le bootloader ressort seul de l'ISP par temporisation : s'il disparaît, ce n'est
pas un échec, il suffit de relancer.
"""

import sys
import time

APP = (0x12C9, 0x6001)
BOOT = (0x0603, 0x1020)

REPORT_ID_CMD = 0x05
CMD_ENTER_ISP = 0x75
USAGE_PAGE_ISP = 0xFF00


def main():
    try:
        import hid
    except ImportError:
        sys.exit("module « hid » manquant :  pip install hidapi  (ou python3-hid)")

    print("1. recherche de l'application %04x:%04x" % APP)
    collections = list(hid.enumerate(*APP))
    if not collections:
        sys.exit("   ✗ absente du bus. Énumère-la d'abord (gm610_linux_rescue.sh reset).")
    for d in collections:
        print("   itf=%s  page=0x%04X  usage=0x%02X  %s" % (
            d.get("interface_number"), d.get("usage_page", 0), d.get("usage", 0),
            d["path"].decode(errors="replace") if isinstance(d["path"], bytes) else d["path"]))

    # La collection vendeur FF00:01 porte le report 5. Repli : tout essayer.
    cibles = [d for d in collections if d.get("usage_page") == USAGE_PAGE_ISP]
    if not cibles:
        print("   ! aucune collection 0xFF00 annoncée — on essaiera toutes les collections")
        cibles = collections

    print("\n2. envoi de la commande d'entrée en ISP (0x%02x)" % CMD_ENTER_ISP)
    trame = bytes([REPORT_ID_CMD, CMD_ENTER_ISP, 0, 0, 0, 0])
    envoye = False
    for d in cibles:
        dev = hid.device()
        try:
            dev.open_path(d["path"])
        except OSError as e:
            print("   itf=%s : ouverture refusée (%s)" % (d.get("interface_number"), e))
            continue
        try:
            r = dev.send_feature_report(trame)
            print("   itf=%s : send_feature_report -> %s" % (d.get("interface_number"), r))
            if r is not None and r >= 0:
                envoye = True
        except OSError as e:
            # Le clavier disparaît parfois AVANT d'acquitter : c'est un succès déguisé.
            print("   itf=%s : %s  (le clavier a peut-être déjà basculé)" % (
                d.get("interface_number"), e))
            envoye = True
        finally:
            try:
                dev.close()
            except Exception:
                pass
        if envoye:
            break

    if not envoye:
        print("\n   ✗ aucune collection n'a accepté la commande.")
        print("     Sans root, hidraw refuse l'écriture : relancer avec sudo.")

    print("\n3. attente du bootloader %04x:%04x (15 s)" % BOOT)
    for i in range(30):
        ids = {(d["vendor_id"], d["product_id"]) for d in hid.enumerate()}
        if BOOT in ids:
            print("   ✅ BOOTLOADER PRÉSENT après %.1f s" % (i * 0.5))
            for d in hid.enumerate(*BOOT):
                print("      itf=%s  page=0x%04X  usage=0x%02X" % (
                    d.get("interface_number"), d.get("usage_page", 0), d.get("usage", 0)))
            return 0
        if i == 6 and APP not in ids:
            print("   ... l'application a disparu du bus : le basculement a eu lieu")
        time.sleep(0.5)

    print("   ✗ le bootloader n'est pas apparu.")
    print("     S'il énumère mal, `dmesg` le dira ; relancer gm610_linux_rescue.sh reset.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
