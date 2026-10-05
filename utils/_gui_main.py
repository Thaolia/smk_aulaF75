"""Point d'entrée pour la compilation Nuitka (import absolu du paquet).

Le dossier utils/ (celui de ce fichier) est sur sys.path[0], donc gm610_macro_gui
est importable. Fichier de build temporaire — non destiné à être conservé."""

from gm610_macro_gui.app import main

if __name__ == "__main__":
    main()
