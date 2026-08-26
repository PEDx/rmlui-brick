# rmlui-brick

**English** · [简体中文](README.zh-CN.md)

A handheld-style game library and emulator frontend built specifically for the TrimUI Brick with **RmlUi, SDL2, and MinArch**. The interface targets the device's native `1024×768` display and handles system selection, ROM discovery, emulator launching, session restoration, and platform-specific screen rendering.

rmlui-brick does not replace the stock `MainUI` or modify the boot process. Its launcher gives the frontend and emulator exclusive access to the display, audio device, and controller in turn. After a game exits normally, the frontend reopens at the previously selected system and ROM.

## Interface

<table>
  <tr>
    <td width="50%" align="center">
      <img src="docs/images/home-current.png" alt="The six-system home screen running on a TrimUI Brick">
    </td>
    <td width="50%" align="center">
      <img src="docs/images/library-current.png" alt="The game library running on a TrimUI Brick">
    </td>
  </tr>
  <tr>
    <td align="center"><sub>Six-system home screen with an infinite carousel</sub></td>
    <td align="center"><sub>Cover carousel generated from ROMs on the SD card</sub></td>
  </tr>
</table>

The home carousel loops continuously in either direction. The active system is enlarged in the center while neighboring systems slide naturally at the edges, including during the wrap from the last item to the first. The library scans the SD card at runtime and presents discovered games as a cover carousel. Use the D-pad to navigate, `A` to enter or launch, and `B` to go back. The status bar shows the current platform, game count, battery level, wireless state, and time.

### On-device rendering

<table>
  <tr>
    <td width="50%" align="center">
      <img src="docs/images/gba-gameplay-current.png" alt="GBA gameplay rendered through MinArch on a TrimUI Brick">
    </td>
    <td width="50%" align="center">
      <img src="docs/images/gbc-gameplay-current.png" alt="GBC gameplay rendered through MinArch on a TrimUI Brick">
    </td>
  </tr>
  <tr>
    <td align="center"><sub>Game Boy Advance · 4×4 LCD aperture</sub></td>
    <td align="center"><sub>Game Boy Color · 5×5 LCD aperture</sub></td>
  </tr>
</table>

GBA output uses a 4× integer scale from the native `240×160` grid to a `960×640` viewport. GB and GBC output uses a 5× integer scale from `160×144` to `800×720`. In both cases, a fragment shader creates an LCD aperture aligned to the source-pixel grid before the device texture and console overlay are composited. These images are captures of the current device framebuffer, not desktop previews or post-processed mockups.

## Highlights

- Native `1024×768` fullscreen RmlUi interface using the device's OpenGL ES 2 renderer.
- Six-system carousel with a centered active state and seamless wraparound.
- Runtime ROM discovery with filename-based cover and metadata matching.
- Clean display, audio, and controller handoff between the frontend and MinArch.
- Restoration of the selected system, directory, and game after the emulator exits.
- Device-font support for Chinese text without redistributing the font.
- AXP2202 battery reporting and a live clock on the home screen.
- Dirty rendering while idle, avoiding repeated presentation of identical frames.
- Vendored MinArch sources, cores, paks, and rendering assets; no adjacent MinUI checkout is required to build.

The companion desktop application, `rom-manager`, can manage local and device ROMs, metadata, covers, and save files, including one-click ROM installation and removal.

## Supported systems

| Platform | Directory | ROM formats | Libretro core | Screen processing |
| --- | --- | --- | --- | --- |
| Game Boy | `GB` | `.gb` `.zip` | Gambatte | 5× integer scale, 5×5 LCD aperture |
| Game Boy Color | `GBC` | `.gbc` `.zip` | Gambatte | 5× integer scale, 5×5 LCD aperture |
| Game Boy Advance | `GBA` | `.gba` `.zip` | gpSP | 4× integer scale, 4×4 LCD aperture |
| Super Nintendo / SFC | `SFC` | `.sfc` `.smc` `.zip` | Snes9x 2005 Plus | Sharp, CRT, and Composite presets |
| Sega Genesis / Mega Drive | `MD` | `.md` `.gen` `.bin` `.smd` `.zip` | PicoDrive | Sharp, CRT, and Composite presets |
| Sega Game Gear | `GG` | `.gg` `.zip` | PicoDrive | 6×5 LCD aperture, 4:3 LCD presentation |

Every platform follows the same ROM, cover, metadata, save, and launch conventions. The game list is never hard-coded into the frontend.

## Screen simulation

![Diagram of the GB/GBC, GBA, Game Gear, and SFC/MD screen pipelines](docs/images/screen-simulation-guide.svg)

The diagram enlarges the output-fragment cell associated with each source pixel and summarizes the native resolution, fixed viewport, and render order for every pipeline.

- **GBA:** `240×160` is scaled 4× into `(32,64,960,640)`. The shader derives a 4×4 aperture from each fragment's position within the viewport.
- **GB/GBC:** `160×144` is scaled 5× into `(112,24,800,720)` and processed by a dedicated 5×5 aperture shader. `gb-gbc-lcd-5x.png` is only an SDL renderer fallback if GLES2 initialization fails.
- **Game Gear:** the core's `160×144` output is mapped to `(32,24,960,720)`. Each source pixel becomes a 6×5 output cell to reproduce the original LCD's non-square-pixel, roughly 4:3 presentation.
- **SFC/MD:** a shared CRT pipeline provides `sharp`, `crt`, `composite`, and `off` modes.

See [Emulator integration](docs/EMULATOR_INTEGRATION.md) for viewport, overscan, fallback, and preset details.

## Storage layout

The launcher expects the following structure on the SD card:

```text
/mnt/SDCARD/
├── Roms/
│   ├── GB/
│   ├── GBC/
│   ├── GBA/
│   ├── SFC/
│   ├── MD/
│   └── GG/
├── Bios/{GB,GBC,GBA,SFC,MD,GG}/
├── Saves/{GB,GBC,GBA,SFC,MD,GG}/
└── .system/tg5040/
    ├── bin/minarch.elf
    ├── cores/
    ├── paks/Emus/
    └── res/
```

Cover art and metadata live next to each system's ROMs:

```text
Roms/GBA/
├── Example.gba
├── .media/Example.png
└── .meta/Example.json
```

- The ROM filename is the default display title.
- Cover art uses `.media/<ROM basename>.png`.
- Metadata uses `.meta/<ROM basename>.json`.
- Missing cover art or metadata does not prevent discovery or launching.
- ROM Manager installs files using the same convention, so newly installed games appear after the library refreshes.

## Runtime model

```text
stock runtrimui.sh
  └── rmlui-brick launcher
      ├── RmlUi frontend
      │   └── writes {system, ROM path} to a launch request, then exits
      ├── MinArch + matching Libretro core
      │   └── exits normally through the MinArch menu
      └── RmlUi frontend restarts and restores the previous selection
```

The frontend and MinArch never hold an SDL window at the same time. Before launching, the wrapper also checks that the ROM is inside `/mnt/SDCARD/Roms` and that its extension matches the requested system.

> Do not terminate `minarch.elf` remotely while a game is running. MinArch owns the display, audio, input, and power state; exit through its in-game menu. For the same reason, the launcher rejects remote `stop` requests while it is in emulator mode.

## Build and preview

The development machine needs CMake, Zig, and SDL2_image:

```sh
brew install cmake zig sdl2_image
```

### macOS preview

```sh
./scripts/build-macos.sh
FONT=/tmp/trimui-regular.ttf ./scripts/run-macos.sh
```

### TrimUI Brick package

```sh
./scripts/build-brick.sh
./scripts/package-brick.sh
```

`build-brick.sh` builds both the ARM64 frontend and the vendored MinArch runtime. It downloads pinned RmlUi and TG5040 SDK dependencies into the ignored `.deps/` directory. The resulting self-contained package is written to:

```text
build/package/rmlui-prototype/
├── trimui-rmlui-prototype
├── launch.sh
├── assets/
└── runtime/tg5040/
```

## Device deployment and debugging

Install the emulator runtime, then start the frontend:

```sh
./scripts/install-emulator-runtime.sh
./scripts/run-brick.sh start
```

Useful device commands:

```sh
./scripts/run-brick.sh status
./scripts/run-brick.sh logs
./scripts/run-brick.sh stop
```

The scripts connect to `root@192.168.31.117` and deploy to `/mnt/UDISK/trimui-dev/rmlui-prototype/current` by default. Override either value when needed:

```sh
TRIMUI_DEVICE=root@192.168.1.20 \
TRIMUI_REMOTE_DIR=/mnt/UDISK/trimui-dev/rmlui-prototype/current \
./scripts/run-brick.sh start
```

The launcher uses the stock `/tmp/cmd_to_run.sh` mechanism to hand over the display. When the frontend exits, the stock `runtrimui.sh` restores `MainUI` automatically.

### Frontend options

```text
--seconds N         Exit automatically after N seconds
--screenshot FILE   Save a BMP after rendering has stabilized
--rom-root DIR      Use a custom ROM root
--request FILE      Set the UI-to-launcher request file
--state FILE        Save and restore the current system and ROM selection
--renderer NAME     Select an SDL renderer
--dp-ratio N        Set UI density in the range 0.5–4.0
--fullscreen        Run fullscreen
```

## Performance

While idle, the frontend stops presenting unchanged frames but continues polling the controller roughly every 16 ms, keeping input latency within one frame. Continuous rendering resumes only while an interaction is animating.

| Idle on device | Before optimization | Current implementation |
| --- | ---: | ---: |
| Total CPU usage | ~8.5% | ~0.39% |
| Single-core equivalent | ~34% | ~1.56% |
| Frontend presentation rate | ~60.2 FPS | 0 FPS |
| Measured CPU frequency | ~1008 MHz | as low as ~408 MHz |

These figures are measurements from the current development device, not a controlled battery-life benchmark with fixed brightness and wireless conditions.

## Repository layout

```text
.
├── assets/                    # RML, RCSS, system artwork, and UI assets
├── cmake/                     # Zig ARM64 cross-compilation configuration
├── docs/                      # Integration and RmlUi documentation
├── scripts/                   # Build, package, install, and device scripts
├── src/                       # RmlUi frontend and ROM catalog
└── vendor/
    ├── minarch/               # Project-maintained MinArch sources
    └── tg5040/                # Cores, paks, overlays, and target runtime
```

Notable implementation choices:

- Pinned RmlUi 6.2 commit `2230d1a6e8e0848ed87a5761e2a5160b2a175ba4`.
- C++14 with a Zig ARM64 toolchain.
- A custom SDL renderer built directly on `SDL_RenderGeometry`.
- Premultiplied alpha conversion before PNG upload.
- Cached RmlUi geometry to avoid repeated per-frame conversion and allocation.
- Dynamic linking against the device's SDL2, SDL2_image, FreeType, and glibc.
- Suppression of duplicate low-level joystick hat events when GameController events are available.
- Separate GB/GBC, GBA, Game Gear, and CRT fragment shaders under `vendor/minarch/src/platform/tg5040/shaders/`; MinArch compiles only the shader required for the active system.

## Documentation

- [Emulator integration, shaders, and safe exit](docs/EMULATOR_INTEGRATION.md)
- [RmlUi 6.2 RCSS guide](docs/RCSS_GUIDE.md)
- [RML elements and scrolling lists](docs/RML_ELEMENTS.md)

## Current limitations

- Favorites, recently played games, and play-time tracking are not yet integrated into the frontend.
- Volume, brightness, Wi-Fi, Bluetooth, and sleep recovery do not yet have a complete settings interface.
- GBA does not yet provide an mGBA fallback core.
- Automatic dimming, idle display-off behavior, and controlled battery-life testing remain to be completed.
- Development builds still include the RmlUi Debugger; it should be removed from release builds.
- The stock environment emits one harmless `setterm: not found` message during startup.
