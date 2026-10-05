"""Interface Tkinter de configuration des macros du GM610.

Dessine la grille 60 % du clavier ; un clic sur une touche ouvre l'éditeur de sa
macro (frappes + délais). La couleur de surbrillance et les macros sont poussées
au clavier par HID, et sauvegardables en JSON (copie qui fait foi côté hôte)."""

from __future__ import annotations

import json
import tkinter as tk
from tkinter import colorchooser, filedialog, messagebox, simpledialog, ttk

try:
    from . import proto, keycodes
except ImportError:  # lancement direct depuis le dossier du paquet
    import proto
    import keycodes

_USAGE_TO_NAME = {v: k for k, v in keycodes.KEYS.items()}
# Ordre d'affichage des bits de modificateurs dans un libellé de frappe.
_MOD_ORDER = ["LCtrl", "LShift", "LAlt", "LGui", "RCtrl", "RShift", "RAlt", "RGui"]


def format_step(step) -> str:
    if step[0] == "delay":
        return "⏱ délai %d ms" % step[1]
    kc, mods = step[1], step[2]
    parts = [name for name in _MOD_ORDER if mods & keycodes.MODS[name]]
    parts.append(_USAGE_TO_NAME.get(kc, "0x%02X" % kc))
    return "⌨ " + "+".join(parts)


def _rgb_to_hex(rgb) -> str:
    return "#%02x%02x%02x" % (rgb[0] & 0xFF, rgb[1] & 0xFF, rgb[2] & 0xFF)


class StepEditor(tk.Toplevel):
    """Éditeur de la séquence d'une touche. Modifie `steps` sur place via OK."""

    def __init__(self, master, label, steps, layout="us"):
        super().__init__(master)
        self.title("Macro — touche %s" % label)
        self.resizable(False, False)
        self.result = None
        self._steps = list(steps)
        self.layout_name = layout

        self.listbox = tk.Listbox(self, width=42, height=10, activestyle="dotbox")
        self.listbox.grid(row=0, column=0, columnspan=6, padx=8, pady=8)
        self._refresh()

        btns = [
            ("Ajouter frappe", self._add_tap),
            ("Ajouter texte", self._add_text),
            ("Ajouter délai", self._add_delay),
            ("Monter", self._up),
            ("Descendre", self._down),
            ("Supprimer", self._remove),
        ]
        for i, (txt, cmd) in enumerate(btns):
            ttk.Button(self, text=txt, command=cmd).grid(row=1, column=i, padx=2, pady=2)

        ttk.Button(self, text="Tout effacer", command=self._clear).grid(
            row=2, column=0, columnspan=2, padx=4, pady=6, sticky="we"
        )
        ttk.Button(self, text="Annuler", command=self._cancel).grid(
            row=2, column=3, padx=4, pady=6, sticky="we"
        )
        ttk.Button(self, text="OK", command=self._ok).grid(
            row=2, column=4, columnspan=2, padx=4, pady=6, sticky="we"
        )

        self.transient(master)
        self.grab_set()

    def _refresh(self):
        self.listbox.delete(0, tk.END)
        for st in self._steps:
            self.listbox.insert(tk.END, format_step(st))

    def _sel(self):
        s = self.listbox.curselection()
        return s[0] if s else None

    def _add_tap(self):
        dlg = TapDialog(self)
        if dlg.result is not None:
            idx = self._sel()
            at = len(self._steps) if idx is None else idx + 1
            self._steps.insert(at, dlg.result)
            self._refresh()

    def _add_text(self):
        txt = simpledialog.askstring("Ajouter du texte", "Texte (interprété en US) :", parent=self)
        if txt:
            self._steps.extend(keycodes.text_to_steps(txt, self.layout_name))
            self._refresh()

    def _add_delay(self):
        ms = simpledialog.askinteger(
            "Ajouter un délai", "Durée (ms, 10..2550) :", parent=self, minvalue=10, maxvalue=2550
        )
        if ms:
            idx = self._sel()
            at = len(self._steps) if idx is None else idx + 1
            self._steps.insert(at, ("delay", ms))
            self._refresh()

    def _up(self):
        i = self._sel()
        if i and i > 0:
            self._steps[i - 1], self._steps[i] = self._steps[i], self._steps[i - 1]
            self._refresh()
            self.listbox.selection_set(i - 1)

    def _down(self):
        i = self._sel()
        if i is not None and i < len(self._steps) - 1:
            self._steps[i + 1], self._steps[i] = self._steps[i], self._steps[i + 1]
            self._refresh()
            self.listbox.selection_set(i + 1)

    def _remove(self):
        i = self._sel()
        if i is not None:
            del self._steps[i]
            self._refresh()

    def _clear(self):
        self._steps = []
        self._refresh()

    def _ok(self):
        self.result = self._steps
        self.destroy()

    def _cancel(self):
        self.result = None
        self.destroy()


class TapDialog(tk.Toplevel):
    """Choix d'une frappe : une touche + des modificateurs."""

    def __init__(self, master):
        super().__init__(master)
        self.title("Frappe")
        self.resizable(False, False)
        self.result = None

        ttk.Label(self, text="Touche :").grid(row=0, column=0, padx=6, pady=6, sticky="e")
        self.key = ttk.Combobox(self, values=sorted(keycodes.KEYS), state="readonly", width=16)
        self.key.set("A")
        self.key.grid(row=0, column=1, columnspan=3, padx=6, pady=6, sticky="w")

        self.mod_vars = {}
        for i, name in enumerate(_MOD_ORDER):
            var = tk.BooleanVar()
            self.mod_vars[name] = var
            ttk.Checkbutton(self, text=name, variable=var).grid(
                row=1 + i // 4, column=i % 4, padx=4, pady=2, sticky="w"
            )

        ttk.Button(self, text="Annuler", command=self.destroy).grid(row=4, column=0, columnspan=2, pady=8)
        ttk.Button(self, text="OK", command=self._ok).grid(row=4, column=2, columnspan=2, pady=8)

        self.transient(master)
        self.grab_set()
        self.wait_window(self)

    def _ok(self):
        mods = 0
        for name, var in self.mod_vars.items():
            if var.get():
                mods |= keycodes.MODS[name]
        self.result = ("tap", keycodes.KEYS[self.key.get()], mods)
        self.destroy()


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("GM610 — configurateur de macros")
        self.resizable(False, False)

        self.table = {}  # key_index -> [steps]
        self.hi_color = list(proto.DEFAULT_HI_COLOR)

        bar = ttk.Frame(self)
        bar.grid(row=0, column=0, sticky="we", padx=8, pady=6)
        self.swatch = tk.Label(bar, text="  ", bg=_rgb_to_hex(self.hi_color), relief="sunken", width=3)
        self.swatch.pack(side="left")
        ttk.Button(bar, text="Couleur surbrillance", command=self._pick_color).pack(side="left", padx=4)
        ttk.Button(bar, text="Charger…", command=self._load).pack(side="left", padx=4)
        ttk.Button(bar, text="Enregistrer…", command=self._save).pack(side="left", padx=4)
        ttk.Button(bar, text="Appliquer au clavier", command=self._apply).pack(side="left", padx=4)
        ttk.Button(bar, text="Lire du clavier", command=self._read_device).pack(side="left", padx=4)
        ttk.Label(bar, text="Texte:").pack(side="left", padx=(10, 2))
        self.layout = tk.StringVar(value="azerty")
        ttk.Combobox(bar, textvariable=self.layout, values=["azerty", "us"],
                     state="readonly", width=7).pack(side="left")
        self.budget = ttk.Label(bar, text="")
        self.budget.pack(side="right")

        grid = ttk.Frame(self)
        grid.grid(row=1, column=0, padx=8, pady=8)
        self.buttons = {}  # (row,col) -> Button
        for r in range(keycodes.MATRIX_ROWS):
            for c in range(keycodes.MATRIX_COLS):
                label = keycodes.cell_label(r, c)
                if label is None:
                    continue
                state = "normal" if keycodes.cell_assignable(r, c) else "disabled"
                b = tk.Button(
                    grid, text=label, width=4, height=2, state=state,
                    command=(lambda rr=r, cc=c: self._edit_key(rr, cc)),
                )
                b.grid(row=r, column=c, padx=1, pady=1, sticky="nsew")
                self.buttons[(r, c)] = b

        ttk.Label(
            self,
            text="Les touches en couleur portent une macro. Fn + Caps active le mode macro sur le clavier.",
        ).grid(row=2, column=0, padx=8, pady=(0, 8), sticky="w")

        self._refresh_all()

    # ─── modèle ───────────────────────────────────────────────────────────────
    def _edit_key(self, row, col):
        ki = proto.key_index(row, col)
        label = keycodes.cell_label(row, col)
        ed = StepEditor(self, label, self.table.get(ki, []), self.layout.get())
        self.wait_window(ed)
        if ed.result is None:
            return
        if ed.result:
            self.table[ki] = ed.result
        else:
            self.table.pop(ki, None)
        self._refresh_key(row, col)
        self._refresh_budget()

    def _refresh_all(self):
        for (r, c) in self.buttons:
            self._refresh_key(r, c)
        self.swatch.config(bg=_rgb_to_hex(self.hi_color))
        self._refresh_budget()

    def _refresh_key(self, row, col):
        b = self.buttons[(row, col)]
        if b["state"] == "disabled":
            return
        has = proto.key_index(row, col) in self.table
        b.config(bg=(_rgb_to_hex(self.hi_color) if has else self.cget("bg")))

    def _refresh_budget(self):
        try:
            n = len(proto.encode(self.table, self.hi_color))
            self.budget.config(text="%d / %d o" % (n, proto.MACRO_BLOB_SIZE), foreground="black")
        except ValueError as exc:
            self.budget.config(text="DÉBORDE : %s" % exc, foreground="red")

    # ─── actions ───────────────────────────────────────────────────────────────
    def _pick_color(self):
        rgb, _ = colorchooser.askcolor(color=_rgb_to_hex(self.hi_color), title="Couleur de surbrillance")
        if rgb:
            self.hi_color = [int(x) for x in rgb]
            self._refresh_all()

    def _apply(self):
        try:
            n = proto.send(self.table, self.hi_color)
        except Exception as exc:  # noqa: BLE001 - message utilisateur
            messagebox.showerror("Échec", str(exc))
            return
        messagebox.showinfo("OK", "Config poussée (%d octets). Fn + Caps pour tester." % n)

    def _read_device(self):
        try:
            hi, table = proto.read()
        except Exception as exc:  # noqa: BLE001 - message utilisateur
            messagebox.showerror("Échec", str(exc))
            return
        self.hi_color = list(hi)
        self.table = table
        self._refresh_all()
        messagebox.showinfo("OK", "Config lue depuis le clavier : %d touche(s)." % len(table))

    def _save(self):
        path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")])
        if not path:
            return
        doc = {
            "hi_color": self.hi_color,
            "macros": {str(ki): steps for ki, steps in self.table.items()},
        }
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(doc, fh, indent=2, ensure_ascii=False)

    def _load(self):
        path = filedialog.askopenfilename(filetypes=[("JSON", "*.json")])
        if not path:
            return
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
        self.hi_color = [int(x) for x in doc.get("hi_color", proto.DEFAULT_HI_COLOR)]
        self.table = {
            int(ki): [tuple(s) for s in steps] for ki, steps in doc.get("macros", {}).items()
        }
        self._refresh_all()


def main():
    App().mainloop()


if __name__ == "__main__":
    main()
