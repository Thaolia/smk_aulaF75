"""Table nom ↔ usage HID, bits de modificateurs, et disposition de base du GM610.

Les usages sont les codes HID standard (= la couche de base du firmware SMK,
src/smk/keycodes.h). Les macros émettent ces codes en POSITIONNEL US : la
conversion vers la disposition de l'hôte n'est PAS faite (le mode AZERTY du
clavier est contourné pendant la lecture d'une macro)."""

# Bits du masque de modificateurs (octet mods du rapport clavier HID).
MODS = {
    "LCtrl": 0x01,
    "LShift": 0x02,
    "LAlt": 0x04,
    "LGui": 0x08,
    "RCtrl": 0x10,
    "RShift": 0x20,
    "RAlt": 0x40,
    "RGui": 0x80,
}

# Touches proposées dans l'éditeur de steps (nom -> usage HID). Sous-ensemble utile.
KEYS = {}


def _add_range(prefix, start_usage, items):
    for i, name in enumerate(items):
        KEYS[name] = start_usage + i


_add_range("", 0x04, list("ABCDEFGHIJKLMNOPQRSTUVWXYZ"))
# Chiffres rangée du haut : 1..9 puis 0.
for _i, _n in enumerate("123456789"):
    KEYS[_n] = 0x1E + _i
KEYS["0"] = 0x27

KEYS.update(
    {
        "Enter": 0x28,
        "Esc": 0x29,
        "Backspace": 0x2A,
        "Tab": 0x2B,
        "Space": 0x2C,
        "-": 0x2D,
        "=": 0x2E,
        "[": 0x2F,
        "]": 0x30,
        "\\": 0x31,
        ";": 0x33,
        "'": 0x34,
        "`": 0x35,
        ",": 0x36,
        ".": 0x37,
        "/": 0x38,
        "CapsLock": 0x39,
    }
)
for _i in range(12):
    KEYS["F%d" % (_i + 1)] = 0x3A + _i
KEYS.update(
    {
        "PrintScreen": 0x46,
        "Insert": 0x49,
        "Home": 0x4A,
        "PageUp": 0x4B,
        "Delete": 0x4C,
        "End": 0x4D,
        "PageDown": 0x4E,
        "Right": 0x4F,
        "Left": 0x50,
        "Down": 0x51,
        "Up": 0x52,
    }
)

_SHIFT = MODS["LShift"]

# ─── Conversion texte US -> steps 'tap' ────────────────────────────────────────
_SHIFTED_US = {
    "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7",
    "*": "8", "(": "9", ")": "0", "_": "-", "+": "=", "{": "[", "}": "]",
    "|": "\\", ":": ";", '"': "'", "~": "`", "<": ",", ">": ".", "?": "/",
}

# ─── Correspondance AZERTY FR : caractère -> (usage HID, mods) ──────────────────
# Pour qu'un hôte réglé en AZERTY FR tape le bon caractère, on émet l'usage HID dont
# l'AZERTY donne ce caractère (p.ex. US-Q = 0x14 donne « a »). Les macros contournant
# la compensation AZERTY du clavier, c'est ici que la traduction se fait.
AZERTY_FR = {
    "a": (0x14, 0), "b": (0x05, 0), "c": (0x06, 0), "d": (0x07, 0), "e": (0x08, 0),
    "f": (0x09, 0), "g": (0x0A, 0), "h": (0x0B, 0), "i": (0x0C, 0), "j": (0x0D, 0),
    "k": (0x0E, 0), "l": (0x0F, 0), "m": (0x33, 0), "n": (0x11, 0), "o": (0x12, 0),
    "p": (0x13, 0), "q": (0x04, 0), "r": (0x15, 0), "s": (0x16, 0), "t": (0x17, 0),
    "u": (0x18, 0), "v": (0x19, 0), "w": (0x1D, 0), "x": (0x1B, 0), "y": (0x1C, 0),
    "z": (0x1A, 0),
    "1": (0x1E, _SHIFT), "2": (0x1F, _SHIFT), "3": (0x20, _SHIFT), "4": (0x21, _SHIFT),
    "5": (0x22, _SHIFT), "6": (0x23, _SHIFT), "7": (0x24, _SHIFT), "8": (0x25, _SHIFT),
    "9": (0x26, _SHIFT), "0": (0x27, _SHIFT),
    "&": (0x1E, 0), "é": (0x1F, 0), '"': (0x20, 0), "'": (0x21, 0), "(": (0x22, 0),
    "-": (0x23, 0), "è": (0x24, 0), "_": (0x25, 0), "ç": (0x26, 0), "à": (0x27, 0),
    ")": (0x2D, 0), "=": (0x2E, 0), "ù": (0x34, 0),
    ",": (0x10, 0), "?": (0x10, _SHIFT), ";": (0x36, 0), ".": (0x36, _SHIFT),
    ":": (0x37, 0), "/": (0x37, _SHIFT), "!": (0x38, 0),
    " ": (0x2C, 0),
}


def text_to_steps(text, layout="us"):
    """Convertit une chaîne en steps ('tap', usage, mods). `layout` vaut 'us' ou
    'azerty' (FR). Les caractères non représentables sont ignorés silencieusement."""
    steps = []
    for ch in text:
        if ch == "\n":
            steps.append(("tap", KEYS["Enter"], 0))
            continue
        if ch == "\t":
            steps.append(("tap", KEYS["Tab"], 0))
            continue

        if layout == "azerty":
            low = ch.lower()
            if ch.isalpha() and ch.isupper() and low in AZERTY_FR:
                steps.append(("tap", AZERTY_FR[low][0], _SHIFT))
            elif ch in AZERTY_FR:
                usage, mods = AZERTY_FR[ch]
                steps.append(("tap", usage, mods))
            continue

        if ch == " ":
            steps.append(("tap", KEYS["Space"], 0))
        elif ch.isalpha() and ch.upper() in KEYS:
            steps.append(("tap", KEYS[ch.upper()], _SHIFT if ch.isupper() else 0))
        elif ch in KEYS:
            steps.append(("tap", KEYS[ch], 0))
        elif ch in _SHIFTED_US:
            steps.append(("tap", KEYS[_SHIFTED_US[ch]], _SHIFT))
    return steps


# ─── Disposition de base (_BL) du GM610, pour dessiner la grille ───────────────
# Chaque case : None (KC_NO, trou), ou (label, assignable). Fn (couche) non assignable.
_N = None
BASE_LAYOUT = [
    ["Esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Bksp"],
    ["Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "\\"],
    ["Caps", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "#", "Enter"],
    ["LShift", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "INT1", _N, "RShift"],
    ["LCtrl", "LGui", "LAlt", _N, _N, "Space", _N, _N, "←", "↓", _N, _N, "→", "Fn"],
]

MATRIX_ROWS = len(BASE_LAYOUT)
MATRIX_COLS = len(BASE_LAYOUT[0])

# Les positions non assignables : trous (gérés par None) et la touche Fn (couche).
FN_ROW, FN_COL = 4, 13


def cell_label(row, col):
    """Label d'affichage d'une case, ou None si trou (KC_NO)."""
    return BASE_LAYOUT[row][col]


def cell_assignable(row, col):
    """True si une macro peut être posée sur cette touche."""
    if BASE_LAYOUT[row][col] is None:
        return False
    return not (row == FN_ROW and col == FN_COL)
