# Logo and icon

This page says which logo and icon files exist, where to use each one and what not to change.
Author of the marks: grabowski98. Licence: **CC BY 4.0** (see [Licence](#licence)).

![Project logo: a dot-matrix display](../../assets/brand/logo-transparent.svg)

## Marks

- **Main logo:** a dot-matrix display that shows the project name in three parts: "ESP", "Can"
  and "Box". The part "Can" is inverted: a dark block with letters in the glass colour.
- **Icon:** a connector housing with 2 × 4 pins and a latch (2 × 8 at large sizes). It is a
  separate mark for small sizes: favicon, avatar, header of the device web panel.

The logo and the icon are two marks. Do not join them into one horizontal lockup.

## Colours

| Name | HEX | Use |
|---|---|---|
| Glass (backlight) | `#B2C246` | display background, letters inside the "Can" block, icon holes |
| Dot | `#283020` | lit dots, "Can" block, icon housing |
| Unlit dot | `#A6B640` | background dot grid |
| Bezel | `#151617` | module frame |
| Bezel rim | `#45494E` | thin frame ring |
| Light background | `#F4F3EE` | light pages and graphics |
| Dark background | `#0E0F10` | dark graphics |

- The contrast of a dot against the glass is 7.0 : 1.
- The letters inside the "Can" block use **exactly** the glass colour. There is no third shade.
- The glass colour on a white page has a contrast of only 2.0 : 1. Do not use it for text on a
  light background; the documentation site uses a darker olive for links (`docs/assets/brand/site.css`).

## Geometry

- **Grid:** one grid of square dots. A dot is 0.867 of the pitch, so the gap is 13 %. Everything
  sits on the grid.
- **Letters:** 5 × 7 dots, in the style of the character set of common character LCD modules.
  1 dot between letters, 3 dots between the three parts of the name.
- **"Can" block:** 1 dot of margin around its letters.
- **Window:** 61 × 13 dots. Margins are 2 dots; left equals right and top equals bottom.
- **Bezel:** 2 pitches on every side.
- **Icon:** a 16 × 16 grid (2 × 4 pins) and a 32 × 32 grid (2 × 8 pins), mirror-symmetric about
  the vertical axis, with equal margins.

## Clear space and minimum sizes

- **Clear space:** 7 pitches (the height of a capital letter) around the bezel. `logo.svg` and
  `logo-dark.svg` include it.
- **Logo on screen:** at least **160 px** wide. Smaller sizes were not checked. Prefer the SVG
  and a width that gives a whole dot pitch, so every dot is drawn the same:
  `logo-transparent.svg` is 65.87 pitches wide, so 527 px gives an 8 px pitch and 329 px a 5 px
  pitch. The READMEs and this site use these widths (`docs/assets/brand/site.css`).
- **Icon on screen:** the 2 × 4 version from 16 px. Scale it only by whole multiples of 16
  (16, 32, 48, 64 ...). The PNG files of other sizes (180, 192) hold the icon at the largest whole
  scale, centred.
- **Web panel header:** `panel-header-icon.svg` has its own 24 × 24 grid, so it is sharp at 24 px
  and 48 px. The panel draws it in the text colour, with the pin holes cut out
  (`firmware/web/index.html`).
- **Silkscreen on a board:** the logo from **10 mm** wide, the 2 × 4 icon from 4 mm, the 2 × 8
  icon from 8 mm. The table of feature sizes is in `hardware/brand/README.md`.

## Don'ts

- Do not recolour the marks and do not add a third shade inside the "Can" block.
- Do not stretch or rotate the logo. Do not add shadows, glows or gradients.
- Do not move letters off the grid and do not change the spacing.
- Do not scale the icon by a factor that is not a whole multiple of its grid: it blurs.
- Do not place the colour logo on a background about as bright as the glass. Use `logo-dark` or
  a one-colour version there.

## Files

Everything is in `docs/assets/brand/`, except the silkscreen files in `hardware/brand/`.

| File | Use |
|---|---|
| `logo.svg`, `logo-600.png`, `logo-1200.png` | logo on the light background, with clear space |
| `logo-dark.svg`, `logo-dark-600.png`, `logo-dark-1200.png` | logo on the dark background |
| `logo-transparent.svg`, `logo-transparent-600.png`, `logo-transparent-1200.png` | the display alone, no background |
| `logo-mono.svg`, `logo-mono-white.svg` | one colour: engraving, stickers, print |
| `icon.svg`, `icon-dark.svg`, `icon-mono.svg`, `icon-2x8.svg`, `icon-2x8-dark.svg` | icon |
| `icon-32.png` ... `icon-512.png`, `icon-dark-32.png` ... `icon-dark-512.png`, `favicon.ico` | icon renders and the favicon (16, 32 and 48 px) |
| `panel-header-icon.svg` | 24 px icon for the header of the device web panel |
| `readme-banner-light.png`, `readme-banner-dark.png` | raster banner 1600 × 400 with a filled background, for places that cannot show an SVG; the READMEs show `logo-animated.svg`, with `logo-transparent.svg` as the fallback |
| `github-social-preview.png`, `github-avatar.png` | repository preview 1280 × 640 and avatar 500 × 500 |
| `espcanbox-intro.gif` | intro animation 1280 × 360, 2.85 s: the name is typed letter by letter, then the "Can" block lights up |
| `logo-animated.svg`, `logo-animated.gif`, `logo-animated-dark.gif` | README header: the name is typed on the display, then the static logo holds; 10 s loop, still logo under "reduce motion". The GIF files (1054 px, on `#ffffff` and `#0d1117`) are for viewers that do not animate SVG. Generated, do not edit |
| `site.css` | colours of the documentation site, taken from the table above |
| `hardware/brand/pcb-*.svg`, `hardware/brand/pcb-*.png` | silkscreen, one colour; PNG files are 1 bit at 1200 dpi |

The SVG files are the masters: paths only, no fonts, no filters. The script that draws them is
not in the repository yet.

The animated logo is the exception: `tools/make_logo_animation.py` builds it from the dots of
`logo-transparent.svg`, so redraw the static logo first and then run
`python tools/make_logo_animation.py --gif`. `python -I tools/make_logo_animation.py --check`
tells whether the animated SVG still matches the static logo and `project.json`.

The project name is part of the drawings: the dots of the logo, the banners, the preview, the
animation, the `<title>` of the SVG files and the file name `espcanbox-intro.gif`.
`tools/set_project_name.py` does not rewrite them and its `--check` does not look at them. After
a change of the project name, replace these files by hand (the animated logo: run the script above).

Screenshots of the web panel are not brand files: they are made by a script, see [Screenshots](screenshots.md).

## Licence

The logo, the icon and every file listed above are by grabowski98 and are licensed under
CC BY 4.0, like the documentation (`LICENSE-docs`). You may use and adapt them when you name the
author. The name of the project and the marks do not mean that this project is affiliated with any
car or radio maker.
