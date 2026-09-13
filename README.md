<div align="center">
  <img src="https://github.com/carlossless/smk/assets/498906/30535a69-47a5-4229-8e08-fe2a840d8355" alt="SMK" />
</div>

# SMK - Small (device) Mechanical Keyboard Firmware

[![Build](https://github.com/carlossless/smk/actions/workflows/build.yml/badge.svg)](https://github.com/carlossless/smk/actions/workflows/build.yml) [![](https://img.shields.io/badge/discord-SMK-blue)](https://discord.gg/SZFBDBuxrK)

## About This Fork

This fork adds a port for the **Epomaker × AULA F75** (classic model), reverse-engineered and built entirely from a stock firmware dump — see [docs/keyboards/aula-f75.md](docs/keyboards/aula-f75.md) for the full write-up (wireless protocol, RGB engine, encoder, macros).

It has only been tested on the maintainer's own unit. Hardware revisions may differ, so proceed at your own risk and keep a full stock firmware backup before flashing (see the warning below).

La branche `gm610` ajoute un second portage, le **Newmen GM610** — voir le récapitulatif
ci-dessous et [docs/keyboards/gm610.md](docs/keyboards/gm610.md).

Everything else in this README describes the upstream SMK project this fork is based on.

This is a keyboard firmware similar to [QMK](https://github.com/qmk/qmk_firmware), but targeting 8051-based devices like the SinoWealth SH68F90A (labeled as BYK916 or BYK901) and the SH68F881.

The S (Small) in SMK comes from this firmware using [SDCC](https://sdcc.sourceforge.net/) to build itself.

## ⚠️ WARNING ⚠️

This firmware is still highly experimental, so be cautious when trying to use it or extend it.

You can very easily end up with a bricked device if the written firmware can't jump back into ISP mode, so before testing or modifying it, it's best to have a full dump of your stock firmware and a programming tool (like an Arduino Nano + [sinodude-serial](https://github.com/carlossless/sinodude/tree/main/firmware) or SinoWealth SinoLink + ProWriter) that can write it back.

## Supported Devices

| Keyboard | MCU | ISP | USB | Wireless | Details |
| -------- | --- | --- | --- | -------- | ------- |
| [NuPhy Air60 v1](https://nuphy.com/products/air60) | SH68F90A / BYK916 | ✅ | ✅ | 2.4G (BT WIP) | [Details](docs/keyboards/nuphy-air60.md) |
| E-YOOSO Z11 | SH68F90A / BYK901 | ✅ | ✅ | N/A | [Details](docs/keyboards/eyooso-z11.md) |
| Genesis Thor 300 | SH68F881 / BYK801 | ✅ | ✅ | N/A | [Details](docs/keyboards/genesis-thor-300.md) |
| Epomaker × AULA F75 | SH68F90A / BYK916 | ✅ | ✅ | 2.4G + BT | [Details](docs/keyboards/aula-f75.md) |
| Newmen GM610 | SH68F90 | ✅ | ✅ | 2.4G + BT *(WIP)* | [Details](docs/keyboards/gm610.md) |

Platform notes: [SH68F90 / SH68F90A](docs/platforms/sh68f90.md), [SH68F881](docs/platforms/sh68f881.md).

## La branche `gm610` — Newmen GM610

60 % (61 touches), RGB par touche, tri-mode. **SinoWealth SH68F90** — la puce se nomme
elle-même : `PART = 68 f9 00 00 00`, relu dans son bloc d'information et sorti sur la
console de debug.

Porté **en conservant le bootloader ISP d'usine** : rien n'est jamais écrit au-dessus de
`0xEFFF`, et l'outil d'empaquetage refuse tout paquet qui y toucherait.

### D'où ça vient

Il n'existait aucune documentation de cette carte. Tout a été sorti d'un `.exe` d'updater
OEM, puis vérifié sur l'appareil :

| | |
| --- | --- |
| Conteneur `5A A5 "SINO"` | en-tête de 12 o, clé XTEA **en clair dans les 4 derniers octets** |
| Chiffrement | XTEA 32 tours, **delta trafiqué `0x9E3769B9`** au lieu de `0x9E3779B9` |
| Flash | 64 Ko relus sur la puce par ISP HID, bootloader compris |
| Brochage | matrice, RGB et radio reversés puis **mesurés un par un** |

Les lignes, les colonnes, les canaux PWM et les six broches radio **coïncident exactement**
avec la `nuphy-air60` : même design de référence, 21 colonnes au lieu de 16. Les code
options sont elles aussi identiques (`A4E063C00F000088`), donc rien à reprogrammer.

### Ce qui fonctionne

| | |
| --- | --- |
| Matrice 5 × 14 | ✅ |
| RGB par touche — **battement** (lub-dub) et **goutte d'eau sur le lac** | ✅ |
| Pavé fléché inversé — `AltGr`/`Menu`/`Ctrl droit` = `← ↓ →` sans `Fn` | ✅ |
| **Compensation AZERTY**, active par défaut, `Fn + Ctrl gauche` | ✅ |
| Accent grave, Impr. écran, Origine/Fin, PgPréc/PgSuiv | ✅ |
| Console de debug HID | ✅ |
| Énumération **sous le nom d'usine** — `SINO WEALTH` / `Newmen Bluetooth Keyboard SMK` | ✅ |
| **`Fn + B` → bootloader d'usine, sans USB** | ✅ |
| Radio `bk3632` — USB / BT ×3 / 2,4 GHz | ⚠️ fonctionne, **désactivée par défaut** |

```
Flash    22 Ko / 60 Ko     37 %      XRAM    1,2 Ko / 4 Ko     28 %
```

### ⛔ La porte de secours, et pourquoi elle est obligatoire

Cette carte n'a **aucune entrée ISP matérielle** : `OP_ISP = 1` dans ses code options, et les
4 096 octets du bootloader d'usine ne lisent pas un seul GPIO. La seule voie vers l'ISP
passait donc par l'USB — **inutile le jour où c'est justement l'USB qui échoue**, ce qui est
arrivé.

D'où `Fn + B`, maintenu ~3 s : le clavier passe **tout rouge** une demi-seconde, s'éteint,
et démarre dans son bootloader. Le décompte et le saut vivent dans l'**ISR systick**, pas
dans la boucle principale — celle-ci s'effondre précisément quand la radio cherche sa
liaison, c'est-à-dire dans l'état où cette porte doit servir.

**Règle à retenir pour toute carte sans entrée ISP matérielle : le firmware doit offrir au
moins une porte vers le bootloader qui ne dépende pas de l'USB.**

### Construire et flasher

```sh
meson setup build && ninja -C build gm610_default_smk.hex

# Linux -- ne depend ni de Windows ni de l'updater OEM
sinowisp write -p sh68f90 build/gm610_default_smk.hex
```

Le `.hex` **tel quel**, sans conversion : le bootloader permute les cinq octets lui-même.

Sous Linux, `utils/gm610_enter_isp.py` (hidapi) et `utils/gm610_enter_isp_usb.py` (libusb)
font basculer le clavier en ISP ; `utils/gm610_linux_rescue.sh` aide quand l'hôte n'énumère
plus. Aucun de ces outils ne contient d'opcode d'écriture flash.

### Ce qui reste ouvert

- **Le Bluetooth.** Il fonctionne — appairage, frappe, témoins d'état — mais `bb_spi.c`
  transfère sous `__critical` et affame l'interruption USB : l'hôte finit par échouer sur
  `GET_DESCRIPTOR`. Deux gardes successives n'ont pas suffi. Le vrai correctif est de
  raccourcir chaque fenêtre `__critical` en découpant les transferts, pas d'en réduire le
  nombre — et ça touche un fichier partagé par toutes les cartes.
- **Le `Maj` gauche** qui ne déclenchait pas l'accord PgPréc/PgSuiv alors que le droit le
  faisait. Contourné en acceptant les deux, **pas expliqué**.
- Six cases vides de la rangée basse, et le report vendeur 12 (macros) : jamais testés.

### Une mise en garde méthodologique

Ce portage a été fait presque entièrement par reverse, et **le reverse s'est trompé
quatorze fois** — chaque erreur est consignée dans le document de la carte. Les plus
coûteuses : conclure à un éclairage par zones sans mesurer, identifier un canal de couleur
sur une **couleur composée**, et interdire l'unique voie de flash Linux sur la base d'une
contradiction qui n'existait pas.

Le désassemblage établit ce que le code *peut* faire. Il ne dit jamais ce qui *tourne*.

## Developing

### Prerequisites

#### Nix

Currently, this project is primarily developed with the help of [Nix](https://nixos.org/) and Nix flakes. Please consider using Nix and the provided [flake](https://github.com/carlossless/smk/blob/master/flake.nix) to automatically set up a reproducible development environment.

With Nix installed and flakes enabled, use `nix develop` or [direnv](https://direnv.net/) to enter a shell with all prerequisites installed.

#### Manual

If setting up prerequisites without nix, you will need the following tools installed and available within your environment:

* [sdcc](https://sdcc.sourceforge.net/) >= 4.3.0
* [meson](https://mesonbuild.com/) >= 0.53
* [ninja](https://ninja-build.org/) >= 1.11.1
* [sinowisp](https://github.com/carlossless/sinowisp) latest version - required only for flashing
* [rust](https://www.rust-lang.org/) >= 1.85 - required only for `smk-console`

### Building & Flashing

Once all prerequisites are set up, you can build and flash firmware for a specific combination of keyboard and layout using the following commands:

```sh
meson setup build # configure meson build dir
meson compile -C build nuphy-air60_default_smk.hex # build firmware for nuphy-air60 with the default layout
meson compile -C build nuphy-air60_default_flash # write firmware to the device via sinowisp
```

### Debug Console

Debug builds ship their log output as HID reports. `smk-console` ([tools/smk-console](tools/smk-console)) prints them, and runs on Linux, macOS and Windows:

```sh
cargo run --release --manifest-path tools/smk-console/Cargo.toml               # defaults to 05ac:024f (nuphy-air60)
cargo run --release --manifest-path tools/smk-console/Cargo.toml -- 258a:002a  # eyooso-z11
cargo run --release --manifest-path tools/smk-console/Cargo.toml -- 258a:001f  # genesis-thor-300
```

It picks devices up and drops them as they are plugged and unplugged, so it can be left running across a reflash. On Linux the `/dev/hidraw*` node needs a udev rule, or `sudo`.

## Acknowledgements

* [libfx2](https://github.com/whitequark/libfx2)
* [LUFA](https://github.com/abcminiuser/lufa)
* [QMK](https://github.com/qmk/qmk_firmware)
