#!/usr/bin/env bash
# Sauvetage du GM610 depuis Linux, quand l'hôte échoue à lire ses descripteurs.
#
# À LANCER SUR UNE VRAIE MACHINE LINUX (ou une clé live) -- pas sous WSL2 :
# WSL2 n'a pas d'USB natif, et usbipd ne peut pas rattacher un appareil que
# Windows lui-même ne parvient pas à énumérer.
#
#   sudo ./utils/gm610_linux_rescue.sh diag     # écoute et explique l'échec
#   sudo ./utils/gm610_linux_rescue.sh quirks   # pose les délais, puis rebrancher
#   sudo ./utils/gm610_linux_rescue.sh reset    # force des réénumérations en boucle
#
# Aucune écriture sur l'appareil : ce script ne fait qu'agir sur l'hôte.
set -uo pipefail

VID=12c9
PID=6001

besoin_root() { [ "$(id -u)" -eq 0 ] || { echo "à lancer avec sudo" >&2; exit 1; }; }

case "${1:-diag}" in

diag)
    echo "== état actuel =="
    lsusb | grep -i "$VID:$PID" || echo "   (pas d'appareil $VID:$PID énuméré)"
    echo
    echo "== branche le clavier MAINTENANT ; Ctrl-C pour arrêter =="
    echo "   à lire : 'error -71/-32', 'over-current', 'Cannot enable',"
    echo "            'device descriptor read', 'can't read configurations'"
    echo
    dmesg -W 2>/dev/null || dmesg -w
    ;;

quirks)
    besoin_root
    echo "== avant =="
    cat /sys/module/usbcore/parameters/quirks 2>/dev/null
    cat /sys/module/usbcore/parameters/old_scheme_first 2>/dev/null
    # g = DELAY_INIT, n = DELAY_CTRL_MSG, p = SHORT_SET_ADDRESS_REQ_TIMEOUT
    echo "$VID:$PID:gnp" > /sys/module/usbcore/parameters/quirks
    echo Y > /sys/module/usbcore/parameters/old_scheme_first
    echo "== après =="
    cat /sys/module/usbcore/parameters/quirks
    cat /sys/module/usbcore/parameters/old_scheme_first
    echo
    echo "Débranche puis rebranche le clavier, et relance '$0 diag' en parallèle."
    echo "Si ça ne suffit pas, ajoute au démarrage du noyau :"
    echo "   usbcore.quirks=$VID:$PID:gnp usbcore.old_scheme_first=1 usbcore.autosuspend=-1"
    ;;

reset)
    besoin_root
    # Une énumération ratée laisse un port en erreur : on le redemande en boucle.
    # Chaque tentative est indépendante -- sur un appareil marginal, l'une passe.
    echo "== réénumérations forcées ; Ctrl-C pour arrêter =="
    for i in $(seq 1 40); do
        if lsusb | grep -qi "$VID:$PID"; then
            echo "[$i] ✅ ÉNUMÉRÉ :"
            lsusb | grep -i "$VID:$PID"
            lsusb -d "$VID:$PID" -v 2>/dev/null | head -30
            exit 0
        fi
        for p in /sys/bus/usb/drivers/usb/*-*; do
            b=$(basename "$p")
            case "$b" in *:*) continue;; esac
            echo "$b" > /sys/bus/usb/drivers/usb/unbind 2>/dev/null
            sleep 0.3
            echo "$b" > /sys/bus/usb/drivers/usb/bind   2>/dev/null
        done
        echo "[$i] pas encore..."
        sleep 2
    done
    echo "40 tentatives sans succès."
    ;;

*)
    echo "usage: $0 {diag|quirks|reset}" >&2
    exit 1
    ;;
esac
