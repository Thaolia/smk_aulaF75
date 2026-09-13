#!/usr/bin/env python3
"""Identifie un clavier SinoWealth par son descripteur HID -- LECTURE SEULE.

Aucune écriture, aucune commande vendeur, aucune entrée en ISP : le script se contente
d'énumérer et de lire les descripteurs de rapport. Il est sans risque pour l'appareil.

Ce qu'on cherche : la signature ISP SinoWealth telle que relevée sur l'AULA F75 --
une collection vendeur portant un rapport **feature d'ID 5 sur 5 octets** (`75 08 95 05`),
sur une interface distincte du clavier. Sa présence est le meilleur indice que `sinowisp`
saura dialoguer avec l'appareil.

Sous WSL2 il n'y a pas d'USB natif : lancer avec le Python **Windows**.

    python3.exe "$(wslpath -w tools/gm610_identify.py)"
    python3.exe "$(wslpath -w tools/gm610_identify.py)" --vid 0x258a
"""

import argparse
import sys

# Le Python Windows lancé depuis WSL2 n'hérite pas de PYTHONIOENCODING (il faudrait
# le déclarer dans WSLENV) : sans ça, les accents de cette sortie sortent cassés.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

ISP_REPORT_ID = 5
ISP_PAYLOAD_LEN = 5

# Fabricants de MCU clavier rencontrés sur ce type de produit ; sert uniquement à annoter
# la sortie, jamais à filtrer.
KNOWN_VENDORS = {
    0x258A: "Sino Wealth Electronic Ltd.",
    0x05AC: "Apple (réutilisé par des claviers SinoWealth)",
    0x0C45: "SONiX / Microdia",
    0x320F: "Evision / Gaming KB",
    0x1EA7: "SHARKOON / générique",
    0x3554: "Compx",
    0x04D9: "Holtek",
}

MAIN_INPUT, MAIN_OUTPUT, MAIN_FEATURE = 8, 9, 11


def parse_report_descriptor(desc):
    """Renvoie {report_id: {'input': n_octets, 'output': n, 'feature': n}} et les usages.

    Parseur volontairement minimal : on n'a besoin que des tailles par report ID et des
    pages d'usage, pas d'une interprétation sémantique du descripteur.
    """
    reports = {}
    usages = []
    report_size = report_count = report_id = 0
    usage_page = None
    i = 0
    while i < len(desc):
        prefix = desc[i]
        i += 1
        if prefix == 0xFE:  # item long : jamais émis en pratique, on saute proprement
            if i >= len(desc):
                break
            size = desc[i]
            i += 2 + size
            continue
        size = {0: 0, 1: 1, 2: 2, 3: 4}[prefix & 0x03]
        itype = (prefix >> 2) & 0x03
        tag = prefix >> 4
        value = int.from_bytes(desc[i : i + size], "little") if size else 0
        i += size

        if itype == 1:  # global
            if tag == 0:
                usage_page = value
            elif tag == 7:
                report_size = value
            elif tag == 8:
                report_id = value
            elif tag == 9:
                report_count = value
        elif itype == 2 and tag == 0:  # local : usage
            usages.append((usage_page, value))
        elif itype == 0 and tag in (MAIN_INPUT, MAIN_OUTPUT, MAIN_FEATURE):
            kind = {MAIN_INPUT: "input", MAIN_OUTPUT: "output", MAIN_FEATURE: "feature"}[tag]
            entry = reports.setdefault(report_id, {"input": 0, "output": 0, "feature": 0})
            entry[kind] += report_size * report_count
    for entry in reports.values():
        for kind in entry:
            entry[kind] = (entry[kind] + 7) // 8
    return reports, usages


def describe(hid, dev, verbose):
    vid, pid = dev["vendor_id"], dev["product_id"]
    vendor = KNOWN_VENDORS.get(vid)
    print(f"\n{vid:#06x}:{pid:#06x}"
          f"   iface={dev['interface_number']}"
          f"   usage_page={dev['usage_page']:#06x} usage={dev['usage']:#04x}")
    print(f"  fabricant   {dev['manufacturer_string']!r}"
          + (f"   [{vendor}]" if vendor else ""))
    print(f"  produit     {dev['product_string']!r}")
    print(f"  serial      {dev['serial_number']!r}")
    if verbose:
        print(f"  chemin      {dev['path']!r}")

    handle = hid.device()
    try:
        handle.open_path(dev["path"])
    except OSError as exc:
        # Windows ouvre en exclusif les collections clavier/souris : c'est normal et sans
        # conséquence -- l'interface vendeur ISP, elle, reste accessible.
        print(f"  descripteur : inaccessible ({exc})")
        return None
    try:
        desc = bytes(handle.get_report_descriptor())
    except (OSError, AttributeError) as exc:
        print(f"  descripteur : illisible ({exc})")
        return None
    finally:
        handle.close()

    reports, usages = parse_report_descriptor(desc)
    print(f"  descripteur : {len(desc)} o")
    if verbose:
        print("    " + desc.hex(" "))
    if usages:
        uniq = sorted({u for u in usages if u[0] is not None})
        print("    usages : " + ", ".join(f"{p:#06x}/{u:#04x}" for p, u in uniq[:12]))
    for rid in sorted(reports):
        r = reports[rid]
        parts = [f"{k}={v} o" for k, v in r.items() if v]
        print(f"    report ID {rid:<3} " + "  ".join(parts))
    hit = reports.get(ISP_REPORT_ID, {}).get("feature") == ISP_PAYLOAD_LEN
    if hit:
        print(f"    >>> SIGNATURE ISP SinoWealth : feature ID {ISP_REPORT_ID} "
              f"de {ISP_PAYLOAD_LEN} o (+1 pour l'ID sur le fil)")
    return hit


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--vid", type=lambda s: int(s, 0), default=0)
    ap.add_argument("--pid", type=lambda s: int(s, 0), default=0)
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="descripteur brut et chemins de périphérique")
    args = ap.parse_args()

    try:
        import hid
    except ImportError:
        sys.exit("Le module « hid » (hidapi) manque : python3.exe -m pip install hidapi\n"
                 "À installer sur le Python WINDOWS -- depuis WSL2 l'USB n'est pas visible.")

    devices = list(hid.enumerate(args.vid, args.pid))
    if not devices:
        sys.exit("Aucune interface HID. Clavier branché en USB-C, en mode FILAIRE ?\n"
                 "Fermer aussi le pilote constructeur et OpenRGB (contention HID).")

    print(f"{len(devices)} interface(s) HID")
    isp = [d for d in devices if describe(hid, d, args.verbose)]

    print("\n=== bilan ===")
    if isp:
        for d in isp:
            print(f"  ISP SinoWealth sur {d['vendor_id']:#06x}:{d['product_id']:#06x} "
                  f"iface={d['interface_number']}")
        print("  -> candidat pour « sinowisp read ». NE PAS écrire avant d'avoir établi")
        print("     firmware_size par reverse de l'updater : une lecture écrit un octet.")
    else:
        print("  aucune interface ne porte la signature ISP (feature ID 5 / 5 o).")
        print("  Soit l'appareil n'est pas branché, soit son ISP se présente autrement")
        print("  -> trancher avec la séquence HID lue dans l'updater.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
