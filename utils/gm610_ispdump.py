#!/usr/bin/env python3
"""Dump de la flash du Newmen GM610 par ISP HID — LECTURE SEULE.

Protocole SinoWealth, tel qu'établi de deux sources indépendantes : le reverse de
l'updater OEM (docs/GM610.md) et les sources de `sinowisp` (carlossless/sinowisp,
`src/isp_device.rs`). Les deux concordent sur chaque constante.

    status   énumère seulement — n'envoie STRICTEMENT rien
    probe    entre en ISP si besoin, lit UNE page, la compare à notre image, s'arrête
    dump     lit la zone applicative puis le bootloader, sauve, compare
    reboot   fait ressortir le clavier de l'ISP

⚠️ Sous WSL2 il n'y a pas d'USB natif : lancer avec le Python **Windows**.

    python3.exe "$(wslpath -w tools/gm610_ispdump.py)" status
    python3.exe "$(wslpath -w tools/gm610_ispdump.py)" probe

Sous **Linux**, le même script tourne tel quel, en root (accès hidraw) :

    sudo python3 tools/gm610_ispdump.py status
    sudo python3 tools/gm610_ispdump.py probe

GARANTIE DE NON-ÉCRITURE. Seuls trois opcodes sont autorisés : entrée en ISP, init
de lecture, et reboot. `send_command()` refuse tout le reste, donc ni l'effacement ni
l'initialisation d'écriture ne peuvent partir d'ici, y compris par erreur de frappe.

`sinowisp read` ne conviendrait PAS pour cet usage : son `read_cycle` appelle
`enable_firmware()` d'abord, ce qui écrit un opcode LJMP à `firmware_size-5`. C'est
l'origine du « une lecture écrit un octet » documenté sur l'AULA F75. Cet outil-ci
n'envoie jamais cette commande.
"""

import argparse
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gm610_identify import parse_report_descriptor  # noqa: E402

# Le Python Windows lancé depuis WSL2 n'hérite pas de PYTHONIOENCODING (il faudrait
# le déclarer dans WSLENV) : sans ça, les accents de cette sortie sortent cassés.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

# Le clavier change d'identité en entrant en ISP : le bootloader SinoWealth énumère
# sous 0603:1020. Cette paire est codée en dur dans le détecteur de l'updater OEM
# (FUN_004084A0) et a été confirmée sur l'appareil.
APP_IDS = (0x12C9, 0x6001)
BOOT_IDS = (0x0603, 0x1020)

REPORT_ID_CMD = 0x05
REPORT_ID_XFER = 0x06
CMD_LEN = 6                      # report ID + opcode + adresse 16 bits + 2 réservés
PAGE_SIZE = 2048
XFER_LEN = PAGE_SIZE + 2         # report ID + écho d'opcode + données

CMD_ENTER_ISP = 0x75
CMD_INIT_READ = 0x52
CMD_REBOOT = 0x5A
XFER_READ_PAGE = 0x72

# Liste blanche : la garantie de non-écriture de cet outil.
CMD_ENABLE_FIRMWARE = 0x55

ALLOWED_COMMANDS = {CMD_ENTER_ISP, CMD_INIT_READ, CMD_REBOOT}
# Seule commande du protocole qui touche la flash. Volontairement hors de
# ALLOWED_COMMANDS : elle n'est atteignable que par `send_command(..., allow_write=True)`,
# donc par un unique site d'appel, greppable, dans `cmd_enable`.
WRITE_COMMANDS = {CMD_ENABLE_FIRMWARE}

FIRMWARE_SIZE = 61440            # PLATFORM_SH68F90 : 65536 - 4096
BOOTLOADER_SIZE = 4096
BOOTLOADER_ADDR = FIRMWARE_SIZE

ASSETS = pathlib.Path(__file__).resolve().parent.parent / "assets" / "gm610"
REFERENCE = ASSETS / "gm610_firmware.bin"

SETTLE = 2.0                     # même délai que sinowisp après reboot/erase
REENUM_TIMEOUT = 15.0


class IspError(RuntimeError):
    pass


def import_hid():
    try:
        import hid
    except ImportError:
        sys.exit("Le module « hid » (hidapi) manque : python3.exe -m pip install hidapi\n"
                 "À installer sur le Python WINDOWS — depuis WSL2 l'USB n'est pas visible.")
    return hid


def descriptor_from_sysfs(path):
    """Le descripteur de rapport, lu dans sysfs — repli pour Linux.

    Les bindings hidapi n'exposent pas tous `get_report_descriptor()`. Sous Linux
    le noyau le publie de toute façon, et le lire là ne demande pas d'ouvrir la
    collection — donc ça marche aussi quand un autre processus la tient.
    """
    try:
        nom = pathlib.Path(path.decode() if isinstance(path, bytes) else path).name
    except Exception:
        return None
    if not nom.startswith("hidraw"):
        return None
    fichier = pathlib.Path("/sys/class/hidraw") / nom / "device/report_descriptor"
    try:
        return fichier.read_bytes()
    except OSError:
        return None


def find_channels(hid):
    """Localise les canaux commande et données par la TAILLE de leur feature report.

    C'est le critère qu'emploie l'updater OEM lui-même (FUN_004084A0) : il compare
    `FeatureReportByteLength` à 6 et à 0x802. Ni le numéro d'interface ni le VID/PID
    ne conviennent : les deux changent entre le mode applicatif et le bootloader.
    """
    cmd_path = xfer_path = None
    devices = [d for d in hid.enumerate()
               if (d["vendor_id"], d["product_id"]) in (APP_IDS, BOOT_IDS)]
    for dev in devices:
        desc = None
        handle = hid.device()
        try:
            handle.open_path(dev["path"])
        except OSError:
            desc = descriptor_from_sysfs(dev["path"])  # Linux : pas besoin d'ouvrir
            if desc is None:
                continue                  # collection ouverte en exclusif par Windows
        if desc is None:
            try:
                desc = bytes(handle.get_report_descriptor())
            except (OSError, AttributeError):
                # Binding hidapi sans get_report_descriptor : Linux l'expose en sysfs.
                desc = descriptor_from_sysfs(dev["path"])
            finally:
                handle.close()
        if not desc:
            continue
        reports, _ = parse_report_descriptor(desc)
        for rid, sizes in reports.items():
            if rid == REPORT_ID_CMD and sizes["feature"] + 1 == CMD_LEN:
                cmd_path = dev["path"]
            elif rid == REPORT_ID_XFER and sizes["feature"] + 1 == XFER_LEN:
                xfer_path = dev["path"]
    return cmd_path, xfer_path


def send_command(handle, opcode, addr=0, allow_write=False):
    if opcode in WRITE_COMMANDS:
        if not allow_write:
            raise IspError(f"opcode {opcode:#04x} écrit en flash — appel refusé sans "
                           "allow_write=True")
    elif opcode not in ALLOWED_COMMANDS:
        raise IspError(f"opcode {opcode:#04x} refusé — cet outil est en lecture seule")
    frame = bytes([REPORT_ID_CMD, opcode, addr & 0xFF, (addr >> 8) & 0xFF, 0, 0])
    assert len(frame) == CMD_LEN
    if handle.send_feature_report(frame) < 0:
        raise IspError(f"send_feature_report a échoué pour l'opcode {opcode:#04x}")


def read_page(handle):
    buf = handle.get_feature_report(REPORT_ID_XFER, XFER_LEN)
    if len(buf) < XFER_LEN:
        raise IspError(f"page courte : {len(buf)} o reçus sur {XFER_LEN}")
    if buf[1] != XFER_READ_PAGE:
        raise IspError(f"écho d'opcode inattendu {buf[1]:#04x} (attendu {XFER_READ_PAGE:#04x}) "
                       "— l'appareil n'est pas en mode lecture")
    return bytes(buf[2:XFER_LEN])


def read_range(hid, cmd_path, xfer_path, addr, length, label):
    cmd = hid.device(); cmd.open_path(cmd_path)
    xfer = hid.device(); xfer.open_path(xfer_path)
    try:
        send_command(cmd, CMD_INIT_READ, addr)
        out = bytearray()
        first = None
        pages = length // PAGE_SIZE
        for n in range(pages):
            page = read_page(xfer)
            # Sans `enable_firmware`, le bootloader sert une page fixe et n'avance pas :
            # on le détecte à la deuxième plutôt que de lire trente fois la même chose.
            if n == 0:
                first = page
            elif n == 1 and page == first:
                print()
                raise IspError(
                    "page 2 identique à la page 1 : le pointeur de lecture n'avance pas.\n"
                    "  Le bootloader refuse de servir la flash tant que `enable_firmware`\n"
                    "  (0x55) n'a pas été envoyé. Lancer d'abord le palier 2a :\n"
                    "      gm610_ispdump.py enable --confirm")
            out += page
            print(f"\r  {label} : page {n + 1}/{pages} "
                  f"({addr + (n + 1) * PAGE_SIZE:#07x})", end="", flush=True)
        print()
        return bytes(out)
    finally:
        cmd.close(); xfer.close()


def enter_isp(hid, cmd_path):
    """Fait sauter le firmware applicatif dans son bootloader ISP.

    Le firmware d'usine teste `octet[0] == 0x05 && octet[1] == 0x75` puis exécute
    `isp_jump` (0x93C3 : CLR IE.7 / MOV B,#0xA5 / MOV A,#0x5A / LJMP 0xFF00). Cette
    séquence ne touche pas à la flash — elle coupe les interruptions et saute.
    """
    handle = hid.device(); handle.open_path(cmd_path)
    try:
        send_command(handle, CMD_ENTER_ISP)
    except (OSError, IspError):
        # Le saut est immédiat : l'appareil se détache avant la fin de la transaction
        # USB, et hidapi signale l'échec tantôt par une exception tantôt par un retour
        # négatif. Les deux sont attendus ICI, et seulement ici -- c'est la seule
        # commande dont on sait qu'elle fait disparaître l'appareil.
        pass
    finally:
        handle.close()


def wait_for_isp(hid):
    deadline = time.time() + REENUM_TIMEOUT
    while time.time() < deadline:
        time.sleep(0.5)
        cmd_path, xfer_path = find_channels(hid)
        if cmd_path and xfer_path:
            return cmd_path, xfer_path
    raise IspError(f"aucun canal de données après {REENUM_TIMEOUT:.0f} s — "
                   "le clavier n'a pas réénuméré en bootloader")


def ensure_isp(hid):
    cmd_path, xfer_path = find_channels(hid)
    if not cmd_path:
        raise IspError(f"clavier introuvable ({APP_IDS[0]:#06x}:{APP_IDS[1]:#06x} "
                       f"ou {BOOT_IDS[0]:#06x}:{BOOT_IDS[1]:#06x}) — "
                       "branché en USB-C, en mode FILAIRE ?")
    if xfer_path:
        print("  déjà en mode bootloader (canal de données présent) — rien à envoyer")
        return cmd_path, xfer_path
    print(f"  mode applicatif → envoi de l'entrée en ISP ({REPORT_ID_CMD:#04x} "
          f"{CMD_ENTER_ISP:#04x})")
    enter_isp(hid, cmd_path)
    return wait_for_isp(hid)


def compare(data, offset, label):
    """Compare à l'image d'usine déchiffrée — c'est l'oracle de cet outil."""
    if not REFERENCE.is_file():
        print(f"  (référence {REFERENCE.name} absente, comparaison impossible)")
        return None
    ref = REFERENCE.read_bytes()[offset:offset + len(data)]
    if data == ref:
        print(f"  ✅ {label} IDENTIQUE à gm610_firmware.bin — lecture correcte, flash intacte")
        return True
    diff = sum(1 for a, b in zip(data, ref) if a != b)
    print(f"  ⚠️  {label} DIFFÈRE de la référence : {diff}/{len(data)} octets")
    if set(data) <= {0xFF}:
        print("     tout à 0xFF — l'appareil refuse probablement la lecture sans")
        print("     `enable_firmware`. NE PAS l'envoyer sans décision explicite : c'est")
        print("     la seule commande du protocole qui écrirait en flash.")
    else:
        print("     contenu structuré mais différent — piste : un `isp_transform`,")
        print("     réversible hors ligne. Conserver le brut pour analyse.")
    return False


def cmd_status(hid, _args):
    cmd_path, xfer_path = find_channels(hid)
    ids = {(d["vendor_id"], d["product_id"]) for d in hid.enumerate()} & {APP_IDS, BOOT_IDS}
    print("clavier : " + (", ".join(f"{v:#06x}:{p:#06x}" for v, p in sorted(ids)) or "absent"))
    print(f"  canal commande ({CMD_LEN} o)   : {'présent' if cmd_path else 'ABSENT'}")
    print(f"  canal données  ({XFER_LEN} o) : {'présent' if xfer_path else 'absent'}")
    print(f"  état : {'BOOTLOADER (ISP)' if xfer_path else 'applicatif (normal)' if cmd_path else 'non détecté'}")
    if xfer_path:
        print("  ⚠️  le bootloader repart dans l'application après un délai d'inactivité")
    return 0


def cmd_probe(hid, _args):
    print("palier 1 — une page, aucune écriture")
    cmd_path, xfer_path = ensure_isp(hid)
    page = read_range(hid, cmd_path, xfer_path, 0, PAGE_SIZE, "page 0")
    print(f"  premiers octets : {page[:16].hex(' ')}")
    ok = compare(page, 0, "page 0")
    print("\nLe clavier reste en ISP. Enchaîner avec « dump », ou « reboot » pour ressortir.")
    return 0 if ok else 1


def cmd_enable(hid, args):
    """Palier 2a — arme la lecture. UNIQUE site d'appel écrivant en flash.

    `enable_firmware` (0x55) pose l'opcode `LJMP` (`0x02`) en `firmware_size-5`
    = `0xEFFB`, et seulement s'il n'y est pas déjà. Sur CE clavier l'octet vaut
    déjà `0x02`, établi sans rien écrire :

      - le bootloader dumpé (`gm610_bootloader_head.bin`) teste `CODE[0xEFFB]` en
        0xF017 (`MOV DPL/DPH` -> 0xEFFB, `MOVC`, `XRL A,#2`, `JZ 0xF053`) et ne
        démarre l'application QUE si l'octet vaut 0x02 ;
      - le clavier démarre son application.

    L'appel est donc attendu sans changement d'état. « Attendu » n'est pas
    « garanti » : la sous-commande exige --confirm, ne lit qu'UNE page, et s'arrête.
    """
    if not args.confirm:
        print("palier 2a — REFUSÉ : --confirm absent.")
        print(f"  Cette sous-commande envoie {CMD_ENABLE_FIRMWARE:#04x} "
              "(`enable_firmware`), la seule")
        print("  commande du protocole qui touche la flash : un octet, 0x02, en 0xEFFB.")
        print("  Analyse complète : docs/GM610.md, section « 0x55 est un no-op ».")
        return 2

    print("palier 2a — armement de la lecture (une écriture d'un octet, attendue sans effet)")
    cmd_path, xfer_path = ensure_isp(hid)

    cmd = hid.device(); cmd.open_path(cmd_path)
    try:
        print(f"  envoi de {REPORT_ID_CMD:#04x} {CMD_ENABLE_FIRMWARE:#04x} "
              "(enable_firmware)")
        send_command(cmd, CMD_ENABLE_FIRMWARE, allow_write=True)
    finally:
        cmd.close()

    # Une seule page : on vérifie que la lecture est débloquée, rien de plus.
    # Les 30 pages sont le palier suivant, et elles se demandent.
    page = read_range(hid, cmd_path, xfer_path, 0, PAGE_SIZE, "page 0")
    print(f"  premiers octets : {page[:16].hex(' ')}")
    ok = compare(page, 0, "page 0")
    if ok:
        print("\n  Lecture débloquée et conforme. Palier suivant : « dump ».")
    else:
        print("\n  Ne pas enchaîner sur « dump » : analyser l'écart d'abord.")
    print("Le clavier reste en ISP (il en ressort seul par timeout, ou via « reboot »).")
    return 0 if ok else 1


def cmd_dump(hid, _args):
    print("palier 2 — zone applicative puis bootloader")
    cmd_path, xfer_path = ensure_isp(hid)

    # Point d'arrêt : on contrôle UNE page avant d'en lire trente. Ne jamais passer
    # outre parce qu'une fenêtre se referme -- le bootloader se récupère par timeout,
    # donc une nouvelle entrée en ISP ne coûte qu'une commande.
    head = read_range(hid, cmd_path, xfer_path, 0, PAGE_SIZE, "contrôle")
    verdict = compare(head, 0, "page 0")
    if verdict is None:
        raise IspError(f"référence {REFERENCE.name} absente : rien ne permettrait de "
                       "valider les 30 pages suivantes. Arrêt.")
    if verdict is False:
        raise IspError("la page 0 ne correspond pas à la référence — arrêt avant la "
                       "lecture complète. Analyser le diagnostic ci-dessus ; ne pas "
                       "relancer en espérant mieux.")

    app = read_range(hid, cmd_path, xfer_path, 0, FIRMWARE_SIZE, "applicatif")
    compare(app, 0, "zone applicative")
    (ASSETS / "gm610_isp_firmware.bin").write_bytes(app)

    boot = read_range(hid, cmd_path, xfer_path, BOOTLOADER_ADDR, BOOTLOADER_SIZE, "bootloader")
    (ASSETS / "gm610_isp_bootloader.bin").write_bytes(boot)
    (ASSETS / "gm610_isp_full.bin").write_bytes(app + boot)
    blank = sum(1 for b in boot if b == 0xFF)
    print(f"  bootloader : {len(boot)} o, {blank} à 0xFF, "
          f"premiers octets {boot[:16].hex(' ')}")
    print(f"\n  écrits dans {ASSETS}/ : gm610_isp_firmware.bin, "
          "gm610_isp_bootloader.bin, gm610_isp_full.bin")
    print("Le clavier reste en ISP — lancer « reboot » pour ressortir.")
    return 0


def cmd_reboot(hid, _args):
    cmd_path, xfer_path = find_channels(hid)
    if not cmd_path:
        return print("clavier introuvable") or 1
    if not xfer_path:
        return print("déjà en mode applicatif — rien à faire") or 0
    handle = hid.device(); handle.open_path(cmd_path)
    try:
        send_command(handle, CMD_REBOOT)
    except (OSError, IspError):
        # Même cas qu'à l'entrée en ISP : le reboot fait disparaître l'appareil avant
        # la fin de la transaction, et hidapi le signale tantôt par une exception,
        # tantôt par un retour négatif. Attendu ICI comme pour CMD_ENTER_ISP.
        pass
    finally:
        handle.close()
    print(f"reboot envoyé, attente de la réénumération…")
    deadline = time.time() + REENUM_TIMEOUT
    while time.time() < deadline:
        time.sleep(1.0)
        if find_channels(hid)[0]:
            break
    else:
        print("  ⚠️  le clavier n'est pas revenu — DÉBRANCHER et REBRANCHER : un cycle")
        print("     d'alimentation réinitialise le MCU. La flash n'a pas été modifiée.")
    return cmd_status(hid, None)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name, fn in (("status", cmd_status), ("probe", cmd_probe),
                     ("dump", cmd_dump), ("reboot", cmd_reboot)):
        sub.add_parser(name).set_defaults(fn=fn)
    p_enable = sub.add_parser("enable", help="palier 2a — arme la lecture (écrit un octet)")
    p_enable.add_argument("--confirm", action="store_true",
                          help="obligatoire : accuse réception de l'écriture en 0xEFFB")
    p_enable.set_defaults(fn=cmd_enable)
    args = ap.parse_args()
    try:
        return args.fn(import_hid(), args)
    except IspError as exc:
        print(f"\nERREUR : {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
