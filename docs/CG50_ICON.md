# CG50 icon contract

Utilities-opt uses the native Prizm/CG50 add-in icon layout. The icon is not
an application-store icon and must not be replaced with a square PNG or an
alpha-composited asset.

The visual reference is Casio's [fx-CG50 Function keys and icons Quick Start
Guide](https://edu.casio.com/content/dam/casio/global/edu-casio-com/support/pdf/graphing-models/fx-cg50/00_CASIO_QuickStartGuide_fx-CG50_Function%20keys%20and%20icons.pdf).
The artwork follows the verified project baseline: a white outer canvas for
the unselected state, a blue rounded selection backplate for the selected
state, a compact high-saturation color tile, hard pixel edges, and a centered
white utility symbol. The gear-and-slider motif identifies Utilities without
putting text into the bitmap; the operating system supplies the add-in name
below the icon.

## Source assets

- `assets/icons/selected.png`: the icon shown while Utilities is highlighted.
- `assets/icons/unselected.png`: the icon shown in the normal menu state.
- Both files are 92 by 64 pixels, RGB-8 (24-bit RGB), with no alpha channel or
  palette.
- The artwork uses hard pixel edges and a high-contrast blue palette so it
  remains legible on the CG50's 384 by 216 icon menu. The selected state also
  carries the official blue/cyan rounded backplate across the icon cell, with
  white outer corners; it must not remain white behind the title strip.
- The rounded tile is approximately x=5..86 and y=5..42. The unselected
  lower y=43..63 strip remains white; the selected state follows the official
  samples and fills that area with the blue selection backplate.

The selected and unselected files contain the same gear/slider object. The
selected state adds the blue selection backplate and keeps the subject and
tile geometry fixed, matching the two-state behavior of the official add-ins.

## G3A packing

Makefile passes the two PNG assets to mkg3a as uns and sel. The generated file
is checked by tools/validate-g3a.ps1:

- 0x7000-byte add-in header;
- unselected RGB565 icon at 0x1000;
- selected RGB565 icon at 0x4000;
- executable section beginning at 0x7000;
- header checksum equal to the trailing checksum.

The validator runs in GitHub Actions after the cross-compile. It also checks
the source PNG dimensions, RGB color depth, and absence of alpha/palette data.
