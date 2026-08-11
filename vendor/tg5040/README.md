# tg5040 emulator runtime provenance

The AArch64 cores originate from the MinUI `v20251127-1` tg5040 runtime. GB and GBC share
Gambatte, GBA uses gpSP, and SFC uses Snes9x 2005 Plus.

- Release: https://github.com/shauninman/MinUI/releases/tag/v20251127-1
- Base archive SHA-256: `b1a5ffefb7599ea0f68bddd66ebc8158ee5bb371dfa1990c6fed07117b131632`
- Gambatte SHA-256: `82cb3a583ad04e508edf599f234fdb585fcf9f3204fc35521af5470c1c8a31cd`
- gpSP SHA-256: `e62ac39c0687d23143d6fa1bbc3770beb31f4cbe67a96644ab1d210b1ae7d022`
- Snes9x 2005 Plus SHA-256: `629a50c2047fdca9222760aa550341ef3e699cb18c8cc9e1fea6ba3a36d8d009`
- Architecture: ARM AArch64

`bin/minarch.elf` is built from `vendor/minarch` by this repository. The pak files, native
scaling configuration, console masks and LCD/CRT resources are all stored here so packaging
and installation do not read from an external MinUI checkout.
