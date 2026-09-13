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
| Newmen GM610 | SH68F90 | ✅ | ✅ | 2.4G + BT | [Details](docs/keyboards/gm610.md) |

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
| Pavé fléché inversé — `AltGr` / `Menu` / `Ctrl droit` = `←` `↓` `→` sans `Fn` | ✅ |
| **Compensation AZERTY**, active par défaut, annoncée 5 s au démarrage | ✅ |
| Accent grave, Impr. écran, Origine / Fin, PgPréc / PgSuiv | ✅ |
| Console de debug HID | ✅ |
| Énumération **sous le nom d'usine**, au suffixe près | ✅ |
| **`Fn + B` → bootloader d'usine, sans USB** | ✅ |
| Radio `bk3632` — USB / BT ×3 / 2,4 GHz | ✅ activée par défaut |

Pas de touches multimédia : choix explicite du propriétaire, pas un manque.

### La couche `Fn`

| | |
| --- | --- |
| `` ` `` · `F1`–`F12` · `Suppr` | rangée des chiffres |
| `Tab` | bascule USB ↔ sans-fil *(maintenir ~3 s)* |
| `Q` `W` `E` | Bluetooth 1 / 2 / 3 *(maintenir ~3 s)* |
| `G` | 2,4 GHz *(maintenir ~3 s)* |
| `B` | **bootloader d'usine** *(maintenir ~3 s)* |
| `Ctrl gauche` | bascule la compensation AZERTY |
| `U` | Impr. écran |
| `J` · `M` | Origine · Fin |
| `Menu` | `↑` — et `Fn` + `Maj` + `↑`/`↓` = PgPréc / PgSuiv |
| `AltGr` · `Ctrl droit` | rendent leur fonction d'origine |
| `[` `]` · `;` `'` | vitesse − / + · luminosité − / + |
| `\` | effet suivant |
| `D` | diagnostic RGB sur la console |

### Budget

```
Flash    28 Ko / 60 Ko     47 %      (sans la radio : 22 Ko, 37 %)
XRAM    1,2 Ko /  4 Ko     30 %
Pile     150 o  /  222 o   68 %      pire cas mesuré, radio + priorité USB
```

### La priorité d'interruption USB — le point technique de la branche

L'hôte échouait sur `GET_DESCRIPTOR` (`-71` / `-32`) dès que le superviseur de liaison
tournait. Ce README a longtemps accusé les fenêtres `__critical` de `bb_spi.c` : **c'était
faux.** `bb_spi_burst()` ne prend le verrou que sur `lock = true`, et le seul appelant qui
le demande est le chemin de réception, sur quatre octets ; les dix sites d'émission n'en
prennent aucun. Le correctif recommandé désignait quelque chose qui n'existe pratiquement
pas.

La vraie piste venait du firmware d'usine. Sa routine `0x922C` active Timer2, **l'épingle au
niveau 0**, puis monte l'**USB seul au niveau 3** — alors que SMK laissait toutes les
interruptions au niveau 0, si bien que l'ISR USB ne pouvait pas préempter l'ISR systick
(rendu LED, 2 kHz). Or `USBIE1` contient `_SOFIA` : l'USB tire sur chaque SOF, toutes les
1 ms.

C'est l'option par carte `usb_irq_priority: 'high'`, qui pose **ensemble** la macro et le
drapeau du vérificateur — les séparer rendrait l'imbrication d'ISR invisible au contrôle.
Coût : **+6 octets**, et les autres cartes restent identiques octet pour octet.

Éprouvé : **300 s, 0 perte d'énumération**, à travers une montée de liaison Bluetooth, un
maintien `Fn + B` et une bascule `Fn + Tab` vers l'USB. Pile mesurée 98 → 122 → 150 o
(sur 222) selon priorité et radio.

⚠️ Ce que ce relevé ne prouve pas : deux choses ont changé à la fois (la garde de
rationnement v2, jamais éprouvée, **et** la priorité) et le propriétaire a choisi de garder
les deux sans les départager ; l'état le plus défavorable — liaison non appairée, superviseur
qui cherche — n'a pas été tenu longtemps.

### Ce que ce portage a corrigé dans le code partagé

| | |
| --- | --- |
| `utils/check_interrupts.py` | **l'interruption 0 n'était pas vérifiée.** La table s'ouvre sur le reset, dont le `ljmp` fait 3 octets sans bourrage ; l'indexer par `offset // 8` donnait au reset et à l'interruption 0 la même clé, l'interruption écrasait le reset, puis le filtre la supprimait. Sur le gm610 c'est le systick : **39 fonctions**, rendu LED et matrice, jamais confrontées à la boucle principale |
| *idem* | son contrôle de recouvrement est **vide de sens sous `--stack-auto`** — locaux sur la pile, `OSEG` vide, 0 créneau sur les 8 cartes. La ligne de succès l'affiche désormais au lieu de laisser lire une garantie |
| *idem* | nouvelle classe de collision ISR ↔ ISR pour l'imbrication (`--high-priority-vector`) |
| `src/smk/usb.c` | les trois chaînes `STRING` étaient codées en dur, les mêmes pour toutes les cartes. Ce sont des macros à valeur par défaut, redéfinissables dans le `kbdef.h` d'une carte |
| `src/platform/*/stack.c` | le pic de pile n'était imprimé que sur un nouveau maximum, donc **avant qu'un hôte puisse s'attacher** — et perdu dans un tampon console de 128 o. Il est désormais répété périodiquement |
| `src/platform/sh68f90/sh68f90.h` | tables de bits `IPH0` / `IPL0` / `IPH1` / `IPL1`, absentes |

Les deux premiers sortent d'un **contrôle négatif** : une fonction rendue délibérément
atteignable depuis deux ISR pour voir le vérificateur échouer. Il a continué de passer.

### Identité USB

Le clavier énumère sous son nom d'origine, à un suffixe près — `12C9:6001`,
`SINO WEALTH` / **`Newmen Bluetooth Keyboard SMK`**, numéro de série `0001`. Les chaînes
d'usine viennent des descripteurs `STRING` de l'image extraite ; le VID:PID était déjà celui
d'usine, c'est ce qui permet à l'updater OEM de reconnaître la carte. Le suffixe est le seul
écart volontaire, faute de quoi rien ne distinguerait ce firmware de l'original.

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

Si après un flash l'hôte ne voit que `0603:1020` — le bootloader — **débrancher et
rebrancher** suffit. Ne pas confondre avec un échec d'armement : `0xEFFB` vaut `FF` dans
**tous** les `.hex` SMK, y compris ceux des cartes qui démarrent ; c'est le flasheur qui
arme, jamais l'image.

### Ce qui reste ouvert

- **Le Bluetooth tient, mais le mécanisme n'est pas expliqué** — voir les réserves
  ci-dessus, et [docs/keyboards/gm610.md](docs/keyboards/gm610.md).
- **Le `Maj` gauche** qui ne déclenchait pas l'accord PgPréc / PgSuiv alors que le droit le
  faisait. Contourné en acceptant les deux, **pas expliqué**.
- **La souris.** Le matériel sait le faire — le firmware d'usine déclare une collection
  souris (report `0x0D`, 5 boutons, X/Y 16 bits, molette) et la radio a l'opcode `0x05`.
  SMK a les keycodes mais aucun chemin de rapport.
- Six cases vides de la rangée basse, et le report vendeur 12 (macros) : jamais testés.
- Le tampon console de 128 octets fait perdre du diagnostic de démarrage sur **toutes** les
  cartes, pas seulement ici.

### Une mise en garde méthodologique

Ce portage a été fait presque entièrement par reverse, et **le reverse s'est trompé à
répétition** — chaque erreur est consignée là où elle porte, dans le document de la carte ou
dans le commentaire du code concerné. Les plus coûteuses :

- conclure à un éclairage par zones sans mesurer ;
- identifier un canal de couleur sur une **couleur composée**, au lieu d'allumer un seul
  canal à la fois ;
- interdire l'unique voie de flash Linux sur la base d'une contradiction qui n'existait pas ;
- annoncer « revenir à l'usine = relancer l'exe OEM », alors que sa charge a la table de
  touches à zéro ;
- **inventer le mécanisme d'un symptôme pourtant bien mesuré**, et l'écrire comme « le vrai
  correctif » dans trois documents ;
- livrer une vérification qui **passait à vide**, jusqu'à ce qu'un contrôle négatif la mette
  en défaut.

Le désassemblage établit ce que le code *peut* faire. Il ne dit jamais ce qui *tourne*. Et un
contrôle qui passe ne dit pas s'il a regardé quelque chose.

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
