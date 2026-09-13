# GM610 — reprendre le debug depuis Linux

Document autonome : tout ce qu'il faut savoir tient ici, sans avoir à relire
l'historique. Objectif unique : **obtenir une fenêtre de flash**.

---

## Où on en est

Le clavier **fonctionne** — LED, touches, Bluetooth. Ce qui échoue est
l'**énumération USB** : l'hôte n'arrive pas à lire ses descripteurs de façon fiable.

| | |
| --- | --- |
| Firmware dessus | SMK, carte `gm610`, avec la radio active |
| Windows | ✗ « échec de demande de descripteur de périphérique », puis « ...de configuration » |
| Linux | ✅ **finit par énumérer**, après plusieurs réénumérations forcées |
| `dmesg` | `error -71` (EPROTO) et `-32` (EPIPE). **Jamais** `over-current` |
| Bootloader d'usine | **intact** — jamais une écriture au-dessus de `0xEFFF` |

**Ce que `-71`/`-32` élimine** : ce n'est pas un problème d'alimentation. C'est EP0 qui
répond mal ou se met en `STALL`, **par intermittence** — d'où la réussite après plusieurs
tentatives.

⚠️ **Le clavier a une batterie.** Il fonctionne parfaitement sans câble : « il répond donc
il est branché » est faux. Vérifier le bus, pas le clavier.

⚠️ **Il n'a AUCUNE entrée ISP matérielle.** `OP_ISP = 1` dans ses code options, et les
4 096 octets du bootloader ne lisent pas un seul GPIO. Pas de sauvetage par pads.
La seule voie vers le bootloader est la commande `0x75` envoyée par USB.

---

## Préparation

```sh
sudo apt install python3-usb python3-hid usbutils   # ou : sudo pip install pyusb hidapi
git clone git@github.com:Thaolia/smk_aulaF75.git && cd smk_aulaF75
git checkout gm610
```

Pour éviter `sudo` partout, une règle udev :

```sh
sudo tee /etc/udev/rules.d/99-gm610.rules >/dev/null <<'RULE'
SUBSYSTEM=="usb",    ATTRS{idVendor}=="12c9", ATTRS{idProduct}=="6001", MODE="0666"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="12c9", ATTRS{idProduct}=="6001", MODE="0666"
SUBSYSTEM=="usb",    ATTRS{idVendor}=="0603", ATTRS{idProduct}=="1020", MODE="0666"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0603", ATTRS{idProduct}=="1020", MODE="0666"
RULE
sudo udevadm control --reload-rules && sudo udevadm trigger
```

---

## La marche à suivre

### 0. Le clavier est-il sur le bus ?

```sh
lsusb | grep -Ei '12c9:6001|0603:1020'
```

Rien ? → étape 1. `12c9:6001` ? → étape 2. `0603:1020` ? → **on est déjà dans le
bootloader**, aller directement à « Une fois en ISP ».

### 1. Forcer l'énumération

Dans un terminal, la surveillance :

```sh
sudo dmesg -w
```

Dans un autre :

```sh
sudo ./utils/gm610_linux_rescue.sh quirks   # DELAY_INIT + DELAY_CTRL_MSG
sudo ./utils/gm610_linux_rescue.sh reset    # réénumérations en boucle
```

`quirks` insère des délais autour des transferts de contrôle — fait exactement pour un
appareil trop lent sur EP0. Les deux paramètres sont modifiables à chaud, sans redémarrer.

### 2. Basculer dans le bootloader

Deux voies pour la **même** requête. Essayer la seconde si la première échoue.

```sh
sudo python3 utils/gm610_enter_isp.py       # par hidapi
sudo python3 utils/gm610_enter_isp_usb.py   # par libusb, court-circuite la couche HID
```

La trame envoyée est `05 75 00 00 00 00`, et c'est **exactement** ce que le firmware
teste (`src/smk/usb.c`, `usb_ep0_out_irq`) :

```c
if (EP0_OUT_BUF[0] == 0x05 && EP0_OUT_BUF[1] == 0x75) usb_isp_requested = 1;
```

Aucun de ces deux scripts ne contient d'opcode d'écriture flash.

---

## Lire la sortie — l'arbre de décision

| Ce que tu vois | Ce que ça veut dire | Quoi faire |
| --- | --- | --- |
| `✅ BOOTLOADER PRÉSENT` | **c'est gagné** | voir « Une fois en ISP » |
| `l'application a disparu du bus` puis échec | la bascule a eu lieu, mais le bootloader énumère mal | `reset` pendant qu'il y est ; c'est une piste différente et importante |
| `Access denied` / `Operation not permitted` | permissions | `sudo`, ou la règle udev |
| `Resource busy` | le pilote noyau tient l'interface | la version `_usb.py` le détache seule ; sinon `sudo modprobe -r usbhid` (attention : clavier interne) |
| `Pipe error` sur **toutes** les interfaces | le firmware a refusé la requête | relever l'interface qui porte la page `0xFF00` (`lsusb -v`) |
| `module « usb » manquant` | dépendance absente **pour le python de root** | `sudo pip install pyusb` |
| étape 1 : rien trouvé | le clavier n'est pas énuméré | retour à l'étape 1 |

---

## Une fois en ISP (`0603:1020`)

Le bootloader a **sa propre pile USB**, bien plus simple, et n'allume aucune LED. S'il
énumère proprement là où l'application échoue, c'est la fenêtre.

```sh
lsusb -v -d 0603:1020 2>/dev/null | head -40
```

⚠️ **Il ressort seul de l'ISP par temporisation.** S'il disparaît, ce n'est pas un échec :
relancer la bascule.

### Flasher

✅ **`sinowisp write` fonctionne**, vérifié sur l'appareil :

```sh
sinowisp write -p sh68f90 build/gm610_default_smk.hex
```

Le `.hex` de SMK **tel quel**, sans conversion — le bootloader permute les cinq octets
lui-même.

⛔ Une version de ce guide l'interdisait. `firmware_size − 5 = 0xEFFB` est l'octet
d'armement même que teste le bootloader : il n'y avait aucun conflit de convention.

---

## Ce qu'il faut rapporter

Trois choses, brutes — pas résumées :

1. **La sortie complète** du script d'entrée en ISP, les trois étapes.
2. **`dmesg`** au moment du branchement (`sudo dmesg -w` dans un terminal à part).
3. **`lsusb -v -d 12c9:6001`**, ou à défaut `lsusb`.

Le détail par interface vaut plus qu'un « ça ne marche pas » : `Pipe error` sur
l'interface 1 et `Access denied` sur l'interface 0 ne mènent pas au même correctif.

---

## Le firmware à flasher, quand la fenêtre s'ouvrira

Construit depuis cette branche (`meson setup build && ninja -C build gm610_default_smk.hex`),
il contient trois choses que le firmware actuellement sur le clavier **n'a pas** :

- **`Fn + B` maintenu ~3 s → `isp_jump()`.** La porte de secours qui manquait : elle
  éteint le RGB puis saute dans le bootloader, **sans USB**. Écrite, jamais vérifiée sur
  l'appareil — puisque c'est le flash qu'on n'arrive pas à faire.
- **Radio désactivée** dans `meson.build` : le Bluetooth marchait, mais `bb_spi.c`
  transfère sous `__critical` et casse les transferts EP0 multi-paquets.
- **Luminosité bridée** tant que `usb_is_configured()` est faux.

Le retour à l'usine complet reste possible : le firmware d'origine avec sa table de
touches vit dans le projet parent (`assets/gm610/gm610_isp_firmware.bin`), et
`tools/gm610_package.py --bin` en refait un installeur.
