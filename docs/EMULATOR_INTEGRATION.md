# TrimUI Brick 模拟器接入方案

记录时间：2026-08-01。

## 实机现状

当前设备没有安装可直接调用的模拟器：

- 找不到 `retroarch`、`mGBA`、`gpSP` 等可执行程序；
- 找不到 `*_libretro.so` core；
- `opkg` 没有安装模拟器包；
- SD 卡已有 `/mnt/SDCARD/Roms/GBA` 等目录，但当前 ROM 目录为空；
- SD 卡没有 MinUI 常用的 `/mnt/SDCARD/Emus` 目录。

因此“有 GBA 目录”只表示目录结构准备好了，不表示 GBA 模拟器已经存在。

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
所以不要让原型在运行期间覆盖同一个文件来接模拟器。我们的部署目录里应该增加自己的
launcher 和专用请求文件，例如 `/tmp/rmlui-next`。

## 建议目录

模拟器程序和配置放在 SD 卡，方便升级或拔卡恢复：

```text
/mnt/SDCARD/
├── Roms/GBA/
├── Bios/GBA/
├── Saves/GBA/
└── Emus/tg5040/
    ├── minarch.elf
    ├── gpsp_libretro.so
    ├── mgba_libretro.so        # 可选
    └── GBA/
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

C++ 层不要把 ROM 路径拼进 `system()` 字符串。应该把系统、core 和 ROM 存为结构化请求，
由 launcher 校验路径后使用 `execv()` 参数数组执行，避免文件名中的空格、引号或 shell 字符
导致启动失败或命令注入。

## 前端需要实现的接口

第一阶段只需要四块：

1. 扫描 `/mnt/SDCARD/Roms/GBA` 中的 `.gba`、`.zip` 文件；
2. 在 RmlUi 滚动列表中显示整理后的游戏名；
3. 按 A 时生成 `{ system: "GBA", rom: "绝对路径" }` 请求并正常退出桌面；
4. 模拟器退出后恢复前端状态。

随后再增加封面、最近游戏、收藏、独立 core 选择、存档槽和自动恢复。

## 实机验证顺序

1. 获取明确支持 `tg5040/aarch64` 的 MinArch 与 core；
2. 用自制或公开授权的 GBA ROM 通过 SSH 单独启动；
3. 验证画面比例、帧率、音频、方向键、A/B/L/R、存档；
4. 配置清晰的退出组合键，确认退出后桌面能恢复；
5. 最后再接入 RmlUi 游戏列表。

在完成单 ROM 测试前，不应修改开机流程或替换原厂 MainUI。

## 参考

- MinUI 官方仓库：<https://github.com/shauninman/MinUI>
- RetroArch 命令行：<https://docs.libretro.com/guides/cli-intro/>
