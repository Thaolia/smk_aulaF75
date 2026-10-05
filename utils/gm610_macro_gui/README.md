# Configurateur de macros du GM610

Petite application Tkinter pour définir les macros du GM610 (firmware SMK) et les
pousser au clavier par HID. Les touches porteuses d'une macro s'éclairent d'une
couleur au clavier quand le **mode macro** est actif (**Fn + Caps**).

## Dépendances

- Python 3 avec Tkinter (inclus dans la distribution standard).
- `hidapi` **uniquement pour le bouton « Appliquer au clavier »** :
  `python3.exe -m pip install hidapi`.

## Lancement

Sous **WSL2, l'USB n'est pas visible** : il faut le **Python Windows**.

```bash
# depuis le dossier utils/ du dépôt
python3.exe -m gm610_macro_gui
# ou en donnant le chemin du paquet
python3.exe "$(wslpath -w gm610_macro_gui)"
```

Fermer d'abord le logiciel constructeur et OpenRGB : ils tiennent l'interface HID
et empêchent l'envoi.

## Utilisation

1. Cliquer une touche de la grille → éditeur de sa macro (frappes, texte, délais).
2. Régler la **couleur de surbrillance** si besoin.
3. **Enregistrer…** la configuration en JSON (sauvegarde locale pratique).
4. **Appliquer au clavier** : pousse tout par HID et persiste en flash.
5. **Lire du clavier** : relit la config réellement stockée dans le clavier et remplit
   la grille (utile pour repartir de l'existant ou vérifier ce qui est en place).
6. Au clavier : **Fn + Caps** active le mode macro ; une touche porteuse joue alors
   sa séquence (les autres tapent normalement). Fn + Caps de nouveau pour sortir.

## ⚠️ Positionnel US

La lecture d'une macro émet des **keycodes HID bruts** et **contourne la
compensation AZERTY** du clavier (`gm610_layout.c`). Les macros tapent donc en
**positionnel US** : une macro « @ » suppose la disposition de l'hôte, exactement
comme lorsque le mode AZERTY du clavier est désactivé. L'éditeur « Ajouter du
texte » interprète lui aussi la saisie en US.

## Gigue des pauses

Tout step « délai » ajouté dans une macro est **jitté de ±10 ms** par le firmware, et la
valeur est **re-tirée à chaque exécution** — pour que les timings ne soient pas d'une
régularité mécanique. Rien à configurer : c'est automatique sur toutes les pauses.

## Limites

- 16 touches porteuses au maximum, blob total ≤ 240 octets (le bandeau affiche le
  budget consommé). Au-delà, « Appliquer » est bloqué.

Le format du blob et le protocole HID sont définis une seule fois dans
`src/smk/macro_store.h` et reflétés dans `proto.py`.

## Binaire autonome Windows (Nuitka)

Un exécutable autonome (Python + Tkinter + hidapi embarqués, lançable au double-clic,
aucune installation requise) se compile avec Nuitka sur le **Python Windows** :

```bash
# depuis la racine du dépôt (utils/ doit être sur le sys.path du lanceur)
python3.exe -m nuitka --onefile --enable-plugin=tk-inter \
  --windows-console-mode=disable \
  --include-package=gm610_macro_gui --include-module=hid \
  --output-dir=build --output-filename=gm610_macro_gui.exe \
  utils/_gui_main.py
```

- `utils/_gui_main.py` est le point d'entrée (import absolu du paquet) — Nuitka ne
  gère pas correctement le `__main__` relatif d'un paquet compilé en dossier.
- `--include-module=hid` est **obligatoire** : `hid` est importé en lazy (dans
  `proto.send`), donc l'analyse statique de Nuitka ne le voit pas sans ça.
- Pour compresser le binaire (~22 Mo → ~8 Mo) : `python3.exe -m pip install zstandard`
  avant de compiler.

Le binaire produit est `build/gm610_macro_gui.exe`.
