# RmlUi on TrimUI Brick：最小原型

这个原型验证 RmlUi 6.2 能否通过 TrimUI Brick 自带的 SDL2/OpenGL ES 2 渲染，并验证中文字体、基础 RCSS、手柄输入和安全退出。

## 当前结论

实机验证已经通过：

- 1024×768 全屏输出正常；
- SDL renderer 为 `opengles2`；
- `SDL_RenderGeometry`、矩形裁剪和字体纹理工作正常；
- 预乘 Alpha 自定义混合模式可用，`renderer_ok=yes`；
- 使用设备 `/usr/trimui/res/regular.ttf` 的阿里巴巴普惠体，中文显示正常；
- 稳定帧率约 60 FPS；
- SDL 将内置手柄识别为 `Xbox 360 Controller`，方向键、A、B 可收到；
- 程序退出后，原厂 `MainUI` 会由 `runtrimui.sh` 自动恢复；
- 没有修改原厂 `MainUI` 或开机流程。

实机部署位置：

```text
/mnt/UDISK/trimui-dev/rmlui-prototype/
├── current/
│   ├── trimui-rmlui-prototype
│   ├── launch.sh
│   └── assets/
├── latest.log
└── screenshot.bmp
```

## 代码结构

```text
.
├── assets/
│   ├── main.rml
│   ├── main.rcss
│   └── launch-brick.sh
├── cmake/
│   └── zig-aarch64-linux.cmake
├── scripts/
│   ├── build-macos.sh
│   ├── build-brick.sh
│   ├── package-brick.sh
│   ├── run-brick.sh
│   ├── run-macos.sh
│   └── setup-toolchain.sh
└── src/
    ├── main.cpp
    ├── prototype_render_interface.*
    └── prototype_system_interface.*
```

不含 RmlUi 第三方源码，原型约 1,000 行，其中 C/C++ 约 600 行，RML/RCSS 约 290 行，其余为构建和工具链脚本。

## 关键技术选择

- 固定 RmlUi 6.2 commit `2230d1a6e8e0848ed87a5761e2a5160b2a175ba4`；
- 使用 C++14 编译，以匹配 RmlUi 6.2 和旧目标环境；正式项目仍可使用 C++17；
- 不使用官方 SDL renderer 的桌面 OpenGL 编译检查；原型包含一个最小 SDL renderer，直接调用 `SDL_RenderGeometry`；
- 仅动态链接设备已有的 SDL2、FreeType 和 glibc；C++ 标准库由 Zig 链接，不依赖目标机的 `GLIBCXX` 版本；
- 使用 TrimUI TG5040 SDK 的 SDL2/FreeType 头文件和链接库；
- 默认不包含字体文件，实机复用原厂字体，避免复制或重新分发字体资源。

同一个方向键动作会同时产生 SDL GameController 和底层 Joystick hat 事件。原型在 GameController 成功打开时只处理高层事件，底层 hat/axis 仅作为兼容后备，防止一次按键移动两格。

## 构建

开发机需要 CMake 和 Zig：

```sh
brew install cmake zig
git clone --depth 1 --branch 6.2 \
  https://github.com/mikke89/RmlUi.git /tmp/RmlUi-6.2
```

本机版本：

```sh
./scripts/build-macos.sh
FONT=/tmp/trimui-regular.ttf ./scripts/run-macos.sh
```

Brick 版本：

```sh
./scripts/build-brick.sh
./scripts/package-brick.sh
```

`setup-toolchain.sh` 会下载并校验官方 TG5040 SDK，解压到被忽略的 `.deps/`。生成的可部署目录位于 `build/package/rmlui-prototype/`。

## 实机运行

在开发机运行（会提示输入设备 SSH 密码）：

```sh
./scripts/run-brick.sh start
```

退出方式：

- 在掌机上按 `B`，程序正常退出，原厂 `MainUI` 自动恢复；
- 界面无响应时，在开发机执行 `./scripts/run-brick.sh stop`；
- `./scripts/run-brick.sh status` 查看状态；
- `./scripts/run-brick.sh logs` 查看最近 100 行日志。

脚本默认连接 `root@192.168.31.117`，默认程序目录为
`/mnt/UDISK/trimui-dev/rmlui-prototype/current`。可以分别用
`TRIMUI_DEVICE` 和 `TRIMUI_REMOTE_DIR` 环境变量覆盖。脚本不包含或保存密码。

也可以在设备端直接启动低层 launcher：

```sh
cd /mnt/UDISK/trimui-dev/rmlui-prototype/current
./launch.sh
```

调试参数：

```text
--seconds N         N 秒后自动退出
--screenshot FILE   稳定渲染后保存 BMP 截图
--renderer NAME     指定 SDL renderer；Brick 使用 opengles2
--fullscreen        1024×768 全屏
```

为避免与原厂桌面争抢屏幕，推荐使用 `run-brick.sh start`。它会通过原厂
`/tmp/cmd_to_run.sh` 完成交接，而不是在 `MainUI` 仍占用显示时直接运行。
测试结束后原桌面自动恢复；该流程不覆盖原厂程序或开机配置。

## 当前限制

- 这是兼容性原型，不是桌面框架；
- 图片加载有意关闭，只验证 RmlUi 动态生成的字体纹理；
- 焦点管理目前是单列循环，不包含二维空间导航；
- 只映射了上、下、A、B，其他按键仅用于后续扩展；
- 尚未接入游戏扫描、电池、网络、音量、启动游戏或休眠恢复；
- Debugger 已编入原型，正式 Release 应移除以减小代码量；
- 原厂脚本会输出一次无害的 `setterm: not found`，不影响 SDL/RmlUi 渲染。

## 下一步

1. 增加图片纹理加载，验证 PNG 透明度和纹理缓存；
2. 建立统一的 Brick 输入映射和按键重复策略；
3. 抽出 UI 数据模型和设备 adapter；
4. 做应用/游戏列表长列表与封面压力测试；
5. 验证原生 OSD、休眠和唤醒后的 GLES2 上下文恢复。
