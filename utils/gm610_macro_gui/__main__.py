"""Point d'entrée : python3 -m gm610_macro_gui"""

try:
    from .app import main
except ImportError:  # lancement direct depuis le dossier du paquet
    from app import main

if __name__ == "__main__":
    main()
