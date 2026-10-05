"""Format du blob macro et protocole HID du GM610.

MIROIR EXACT de src/smk/macro_store.h : toute évolution d'un côté doit être
répercutée de l'autre. Ce module est pur (aucune dépendance à hidapi à l'import) :
`hid` n'est importé que dans send().

Modèle de données :
  - un step est un tuple :
        ('tap',   keycode:int, mods:int)   # frappe (keycode basique + bits mods)
        ('delay', ms:int)                  # pause
  - une table est un dict { key_index:int -> list[step] }
    où key_index = row * MATRIX_COLS + col (position physique sur la grille 5x14).
"""

from __future__ import annotations

# ─── Constantes miroir de src/smk/macro_store.h ────────────────────────────────
MACRO_BLOB_SIZE = 240
MACRO_MAX_KEYS = 16
MACRO_STORE_VERSION = 0x01

BLOB_VERSION_OFF = 0
BLOB_HICOLOR_OFF = 1
BLOB_COUNT_OFF = 4
BLOB_DIR_OFF = 5

STEP_DELAY_TAG = 0x00
DELAY_UNIT_MS = 10

FEATURE_REPORT_SIZE = 7  # octets de données (hors octet d'ID), = 1 paquet EP0

OP_BEGIN = 0x01
OP_DATA = 0x02
OP_COMMIT = 0x03
OP_READ_SEEK = 0x04

# ─── Constantes USB / matrice ──────────────────────────────────────────────────
REPORT_ID_MACRO = 8
VID = 0x12C9
PID = 0x6001

# Usage de la collection HID macro (miroir de HID_RI_USAGE(8, 0x02) dans usb.c).
# La collection ISP partage la même usage_page 0xff00 (usage 0x01) : il faut viser
# 0x02, sinon Windows accepte l'écriture sur l'ISP sans jamais la délivrer au macro.
MACRO_USAGE_PAGE = 0xFF00
MACRO_USAGE = 0x02

MATRIX_ROWS = 5
MATRIX_COLS = 14

DEFAULT_HI_COLOR = (0, 255, 255)  # cyan, défaut firmware


def key_index(row: int, col: int) -> int:
    return row * MATRIX_COLS + col


def _encode_step(step) -> bytes:
    kind = step[0]
    if kind == "tap":
        kc = int(step[1]) & 0xFF
        mods = int(step[2]) & 0xFF
        if kc == 0:
            raise ValueError("keycode 0 (KC_NO) interdit : c'est le tag de délai")
        return bytes((kc, mods))
    if kind == "delay":
        units = max(1, min(255, round(int(step[1]) / DELAY_UNIT_MS)))
        return bytes((STEP_DELAY_TAG, units))
    raise ValueError("step inconnu : %r" % (step,))


def encode(table: dict, hi_color=DEFAULT_HI_COLOR) -> bytes:
    """Sérialise la table en blob (<= MACRO_BLOB_SIZE octets). Lève ValueError si
    la table déborde du budget ou viole une contrainte."""
    items = sorted((ki, steps) for ki, steps in table.items() if steps)
    if len(items) > MACRO_MAX_KEYS:
        raise ValueError("trop de touches-macro : %d > %d" % (len(items), MACRO_MAX_KEYS))

    out = bytearray()
    out.append(MACRO_STORE_VERSION)
    out += bytes((hi_color[0] & 0xFF, hi_color[1] & 0xFF, hi_color[2] & 0xFF))
    out.append(len(items))

    dir_bytes = bytearray()
    step_bytes = bytearray()
    for ki, steps in items:
        if not (0 <= ki < MATRIX_ROWS * MATRIX_COLS):
            raise ValueError("key_index hors grille : %d" % ki)
        if len(steps) > 255:
            raise ValueError("macro trop longue : %d steps" % len(steps))
        dir_bytes += bytes((ki, len(steps)))
        for st in steps:
            step_bytes += _encode_step(st)

    out += dir_bytes
    out += step_bytes
    if len(out) > MACRO_BLOB_SIZE:
        raise ValueError(
            "blob %d octets > budget %d : réduire les macros" % (len(out), MACRO_BLOB_SIZE)
        )
    return bytes(out)


def decode(blob: bytes):
    """Inverse d'encode(). Rend (hi_color, table). Un blob invalide rend une table
    vide et la couleur par défaut."""
    if len(blob) < BLOB_DIR_OFF or blob[BLOB_VERSION_OFF] != MACRO_STORE_VERSION:
        return (DEFAULT_HI_COLOR, {})

    hi = (blob[BLOB_HICOLOR_OFF], blob[BLOB_HICOLOR_OFF + 1], blob[BLOB_HICOLOR_OFF + 2])
    count = blob[BLOB_COUNT_OFF]
    off = BLOB_DIR_OFF + count * 2
    table = {}
    for i in range(count):
        ki = blob[BLOB_DIR_OFF + i * 2]
        n = blob[BLOB_DIR_OFF + i * 2 + 1]
        steps = []
        for _ in range(n):
            b0, b1 = blob[off], blob[off + 1]
            off += 2
            if b0 == STEP_DELAY_TAG:
                steps.append(("delay", b1 * DELAY_UNIT_MS))
            else:
                steps.append(("tap", b0, b1))
        table[ki] = steps
    return (hi, table)


def checksum(blob: bytes) -> int:
    return sum(blob) & 0xFF


def frames(blob: bytes):
    """Découpe le blob en trames HID de 8 octets : [id, op, arg, p0..p4].

    Le firmware reconstitue à l'offset (index_de_trame * 5), donc les trames DATA
    sont indexées 0,1,2,… sans trou."""
    out = [[REPORT_ID_MACRO, OP_BEGIN, len(blob) & 0xFF, (len(blob) >> 8) & 0xFF, 0, 0, 0, 0]]
    idx = 0
    pos = 0
    while pos < len(blob):
        chunk = list(blob[pos : pos + 5])
        chunk += [0] * (5 - len(chunk))
        out.append([REPORT_ID_MACRO, OP_DATA, idx] + chunk)
        idx += 1
        pos += 5
    out.append([REPORT_ID_MACRO, OP_COMMIT, checksum(blob), 0, 0, 0, 0, 0])
    return out


def _candidate_paths(hid, vid=VID, pid=PID):
    """Chemins HID à essayer. LA COLLECTION MACRO (0xff00 / usage 0x02) D'ABORD :
    l'ISP partage l'usage_page 0xff00 et une écriture report id 8 dessus « réussit »
    sous Windows sans rien délivrer au firmware."""
    devs = list(hid.enumerate(vid, pid))
    if not devs:
        raise RuntimeError(
            "Aucune interface HID %04x:%04x. Clavier branché en USB-C, mode FILAIRE ?\n"
            "Fermer aussi le logiciel constructeur / OpenRGB (contention HID)." % (vid, pid)
        )

    def rank(d):
        up, us = d.get("usage_page", 0), d.get("usage", 0)
        if up == MACRO_USAGE_PAGE and us == MACRO_USAGE:
            return 0  # la collection macro : à essayer en premier
        if up == MACRO_USAGE_PAGE:
            return 1  # autre collection vendeur (ISP)
        return 2

    return [d["path"] for d in sorted(devs, key=rank)]


def send(table: dict, hi_color=DEFAULT_HI_COLOR, vid=VID, pid=PID) -> int:
    """Pousse la table au clavier par SET_REPORT(FEATURE, id=8). Rend le nombre
    d'octets utiles poussés. Lève en cas d'échec sur toutes les interfaces.

    Sous WSL2 : lancer avec le Python Windows (hidapi), l'USB n'y est pas visible."""
    try:
        import hid
    except ImportError as exc:  # pragma: no cover - dépend de l'environnement
        raise RuntimeError(
            "Module « hid » (hidapi) manquant : python3.exe -m pip install hidapi"
        ) from exc

    blob = encode(table, hi_color)
    pkts = frames(blob)

    last_err = None
    for path in _candidate_paths(hid, vid, pid):
        handle = hid.device()
        try:
            handle.open_path(path)
        except OSError as exc:
            last_err = exc
            continue
        try:
            for pkt in pkts:
                handle.send_feature_report(bytes(pkt))
            return len(blob)
        except (OSError, ValueError) as exc:
            # Interface sans le report ID 8 (Windows la refuse) : on essaie la suivante.
            last_err = exc
        finally:
            handle.close()

    raise RuntimeError("Aucune interface n'a accepté la config macro (%s)" % last_err)


def read(vid=VID, pid=PID):
    """Lit (dump) le blob stocké dans le clavier et le décode. Rend (hi_color, table).

    Place le curseur à 0 (SET READ_SEEK) puis enchaîne des GET_REPORT de 7 octets
    (Windows borne un GET à la taille déclarée du report)."""
    try:
        import hid
    except ImportError as exc:  # pragma: no cover
        raise RuntimeError(
            "Module « hid » (hidapi) manquant : python3.exe -m pip install hidapi"
        ) from exc

    nchunks = (MACRO_BLOB_SIZE + 6) // 7
    last_err = None
    for path in _candidate_paths(hid, vid, pid):
        handle = hid.device()
        try:
            handle.open_path(path)
        except OSError as exc:
            last_err = exc
            continue
        try:
            handle.send_feature_report(bytes([REPORT_ID_MACRO, OP_READ_SEEK, 0, 0, 0, 0, 0, 0]))
            blob = bytearray()
            for _ in range(nchunks):
                rep = handle.get_feature_report(REPORT_ID_MACRO, 8)
                if rep and len(rep) >= 8:
                    blob += bytes(rep[1:8])  # [id, 7 octets]
                elif rep and len(rep) == 7:
                    blob += bytes(rep[0:7])  # pas d'id en tête
                else:
                    raise RuntimeError("réponse GET inattendue (%r)" % (rep,))
            return decode(bytes(blob[:MACRO_BLOB_SIZE]))
        except (OSError, ValueError) as exc:
            last_err = exc
        finally:
            handle.close()
    raise RuntimeError("Lecture de la config échouée (%s)" % last_err)
