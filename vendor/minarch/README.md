# Vendored MinArch source

This directory contains the complete source closure used to build the tg5040
`minarch.elf` shipped by this project. The build does not read from a sibling
MinUI checkout.

Upstream source:

- Repository: <https://github.com/shauninman/MinUI>
- Base revision: `dbf8943` (`v20251127-1`)
- Imported from local integration revision: `14e6ff6`
- Libretro API header: Copyright 2010-2024 The RetroArch team, MIT license
  text embedded at the top of `include/libretro.h`

Vendored layout:

- `src/minarch.c`: MinArch frontend
- `src/common/`: shared MinUI platform/API/scaler support used by MinArch
- `src/platform/tg5040/`: Brick/tg5040 renderer, including the GLES2 GBA
  aperture shader and console-mask compositor
- `include/`: the two external API headers required by this build

`scripts/build-minarch-brick.sh` is the canonical build entry point. Brick
changes should be made here, not in a separate MinUI checkout.
