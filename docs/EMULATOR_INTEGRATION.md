# TrimUI Brick 模拟器接入方案

记录时间：2026-08-01。

## 实机现状

最初检查时设备没有安装可直接调用的模拟器：

- 找不到 `retroarch`、`mGBA`、`gpSP` 等可执行程序；
- 找不到 `*_libretro.so` core；
- `opkg` 没有安装模拟器包；
- SD 卡已有 `/mnt/SDCARD/Roms/GBA` 等目录，但当时 ROM 目录为空；
- SD 卡没有 MinUI 常用的 `/mnt/SDCARD/Emus` 目录。

当前已接入 MinUI `tg5040` 的 MinArch、Gambatte 和 gpSP：GB/GBC 使用 Gambatte，GBA 使用
gpSP。运行时不存在时，游戏库仍可浏览，但 launcher 会拒绝启动并返回桌面。

Brick 必须使用包含原生 1024×768 支持的较新 MinArch。实测 2024-12-13 版本仍按
1280×720 创建视频区域，会导致游戏画面右移、菜单横向拉伸；2025-11-27 版本已正常识别 Brick。

## 推荐方案

第一版建议复用 MinUI 在 `tg5040` 平台已经验证过的组合：

```text
minarch.elf + gpsp_libretro.so
```

gpSP 是 MinUI 给 GBA 使用的默认 core，目标偏向低功耗和流畅运行。需要更高兼容性时，可以把
`mgba_libretro.so` 作为第二个可选 core；MinUI 的 Extras 也使用
`minarch.elf + mgba_libretro.so`。

另一种选择是：

```text
retroarch -L /path/to/gpsp_libretro.so /path/to/game.gba
```

RetroArch 的配置能力更完整，但程序、配置和菜单更重。当前项目想做自己的前端，因此先用
MinArch 更贴近目标：RmlUi 负责选游戏，MinArch 只负责游戏内运行、存档和菜单。

## 进程模型

不要让 RmlUi 桌面和模拟器同时持有 SDL 窗口、OpenGL ES 上下文、音频设备和手柄。推荐使用
一个常驻的外层 launcher：

```text
launcher
  -> 启动 RmlUi 桌面
  -> 桌面选中 ROM 后写入 launch request 并退出
  -> launcher 启动 MinArch/core/ROM，并等待它退出
  -> launcher 重新启动 RmlUi，恢复上次系统、目录和选中项
```

这和 MinUI 的实现方式一致。MinUI 的 `tg5040` 启动器在循环中启动前端，读取
`/tmp/next` 的命令，运行模拟器，然后回到前端。

目前原型自身是由原厂 `/tmp/cmd_to_run.sh` 机制启动的。原厂脚本会在原型退出后删除这个文件，
所以不要让原型在运行期间覆盖同一个文件来接模拟器。项目 launcher 现已使用专用请求文件
`/tmp/rmlui-next`，并用 `/tmp/rmlui-state` 恢复上次机型、目录和选中项。

## 建议目录

模拟器程序和配置沿用 MinUI 的 SD 卡布局，方便升级或拔卡恢复：

```text
/mnt/SDCARD/
├── Roms/{GB,GBC,GBA}/
│   ├── <游戏文件>.<gb|gbc|gba|zip>
│   └── .media/<游戏文件名（不含扩展名）>.png
├── Bios/{GB,GBC,GBA}/
├── Saves/{GB,GBC,GBA}/
└── .system/tg5040/
    ├── bin/minarch.elf
    ├── cores/{gambatte,gpsp}_libretro.so
    └── paks/Emus/{GB,GBC,GBA}.pak/
        ├── launch.sh
        └── default.cfg
```

也可以把项目自带的运行时放到 `/mnt/UDISK/trimui-dev/`，但用户 ROM 和存档仍应放 SD 卡。

## 启动参数和环境

MinUI 的 Brick/tg5040 GBA 脚本本质上执行：

```sh
minarch.elf "$CORES_PATH/gpsp_libretro.so" "$ROM"
```

正式 launcher 还需要设置：

- `LD_LIBRARY_PATH`：项目运行库、`/usr/trimui/lib`；
- `HOME`：指向 SD 卡上的可写配置目录；
- BIOS、存档、日志目录；
- CPU governor/频率和退出后的恢复策略；
- Brick 的手柄映射、音量、电源键和休眠行为。

C++ 层把系统和 ROM 写为两行结构化请求。launcher 只接受 GB/GBC/GBA、校验 ROM 必须位于
`/mnt/SDCARD/Roms` 且扩展名匹配，并始终把路径作为单个引用参数传递给对应 `.pak/launch.sh`。

## 退出安全

MinArch 运行时同时管理 Brick 的显示、音频、输入和电源状态，不能用 `killall minarch.elf`
或外部 `SIGTERM` 作为正常退出方式。游戏必须从 MinArch 自身菜单退出。项目 launcher 会在
`desktop.mode` 为 `emulator` 时拒绝远程 `stop`，防止外部终止造成设备异常关机。

## 前端需要实现的接口

第一阶段接口已经完成：

1. 扫描 GB/GBC/GBA 目录中的 `.gb`、`.gbc`、`.gba`、`.zip` 文件；
2. 在 RmlUi 封面 rail 中显示整理后的游戏名，并按相邻 `.media` 约定加载 PNG 封面；
3. 按 A 时生成两行 `{ system, rom }` 请求并正常退出桌面；
4. 模拟器退出后恢复前端状态。

后续再增加设备端封面刮削、最近游戏、收藏、独立 core 选择和存档槽管理。

## 实机验证状态

1. GB、GBC、GBA 已分别通过 Gambatte/Gambatte/gpSP 启动；
2. MinArch 正常退出后可以恢复 RmlUi 桌面与上次选择；
3. launcher 会阻止在模拟器运行时从外部发送停止信号；
4. 仍需持续回归更多游戏的画面比例、音频、按键和存档兼容性。

原厂 MainUI 和开机流程保持不变。测试运行时仅通过原厂交接脚本临时切换前端。

## 参考

- MinUI 官方仓库：<https://github.com/shauninman/MinUI>
- RetroArch 命令行：<https://docs.libretro.com/guides/cli-intro/>
