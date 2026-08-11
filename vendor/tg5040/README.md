# SNES runtime provenance

`cores/snes9x2005_plus_libretro.so` is the AArch64 tg5040 core extracted from the official
MinUI `v20251127-1` base release:

- Release: https://github.com/shauninman/MinUI/releases/tag/v20251127-1
- Base archive SHA-256: `b1a5ffefb7599ea0f68bddd66ebc8158ee5bb371dfa1990c6fed07117b131632`
- Core SHA-256: `629a50c2047fdca9222760aa550341ef3e699cb18c8cc9e1fea6ba3a36d8d009`
- Core architecture: ARM AArch64

The SFC pak files mirror MinUI's tg5040 launcher, with Brick-native scaling configured so
the custom 3× CRT texture and console mask align with a 256×224 SNES frame.
