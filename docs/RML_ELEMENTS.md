# RML UI 元素与滚动列表

RML 看起来像 HTML，但不是浏览器 DOM。RmlUi 允许使用任意标签名；没有专门
注册的标签会成为普通 `Element`，再由 RCSS 决定它的布局和外观。

因此 `<div>`、`<span>`、`<p>`、`<h1>`、`<section>` 和普通 `<button>` 都
可以用于组织界面，但标签名本身不一定带有浏览器中的默认行为或默认样式。
例如 `h1` 不会自动成为 block，也不会自动变大，必须在 RCSS 中明确设置。

## 可以使用的元素

### 普通布局元素

可以自由使用语义清楚的标签：

```xml
<header>...</header>
<main>
    <aside>...</aside>
    <section class="content">...</section>
</main>
<footer>...</footer>
```

这些通常都是普通元素。推荐以 `div`、`span`、`button` 为基础，通过 class
复用样式。可交互元素需要 C++ 事件或数据模型配合。

### RmlUi 6.2 内建特殊元素

| 元素 | 用途 | 本原型状态 |
|---|---|---|
| `body` | 文档根内容 | 可用 |
| `img` | 图片 | RmlUi 支持，但当前 renderer 未实现图片加载 |
| `form` | 表单容器 | 可用，暂未接业务逻辑 |
| `input type="text"` | 单行文字输入 | 可用，需要文字输入/IME 事件接入 |
| `input type="password"` | 密码输入 | 同上 |
| `input type="checkbox"` | 复选框 | 可用，需要掌机按键映射和样式 |
| `input type="radio"` | 单选框 | 可用，需要掌机按键映射和样式 |
| `input type="range"` | 滑块 | 可用，需要左右键映射和样式 |
| `input type="button"` | 按钮 | 可用 |
| `input type="submit"` | 提交按钮 | 可用 |
| `textarea` | 多行文字输入 | 可用，需要输入事件接入 |
| `select` / `option` | 下拉选择 | 可用，需要掌机交互适配 |
| `label` | 表单标签 | 可用 |
| `tabset` | 页签容器 | 可用，需要按键交互和样式 |
| `progress` / `progressbar` | 进度条或环形进度 | 可用 |
| `handle` | 拖动、移动或缩放手柄 | 桌面鼠标场景可用，掌机通常不用 |

RmlUi 还会在输入框、下拉框、滑块和滚动容器内部生成一些元素，例如
`slidertrack`、`sliderbar`、`scrollbarvertical`。它们可以用 RCSS 选择器
单独设置样式。

## 滚动列表

可以写。RmlUi 没有单独的 `<list>` 或 `<listview>` 标签；滚动列表就是一个
固定高度、内容溢出的普通容器。

### RML

```xml
<div id="game-list" class="game-list">
    <button id="game-0" class="game-row">
        <span class="game-title">Super Mario World</span>
        <span class="game-system">SNES</span>
    </button>
    <button id="game-1" class="game-row">
        <span class="game-title">Sonic the Hedgehog</span>
        <span class="game-system">MD</span>
    </button>
    <!-- 更多行 -->
</div>
```

### RCSS

```css
.game-list {
    width: 100%;
    height: 480px;
    display: block;
    overflow-x: hidden;
    overflow-y: auto;
    padding-right: 8px;
}

.game-row {
    width: 100%;
    height: 64px;
    margin-bottom: 8px;
    padding: 0 18px;
    display: flex;
    flex-direction: row;
    align-items: center;
    color: #aab8d0;
    background-color: #111828;
    border: 1px #2b374d;
    focus: auto;
    tab-index: auto;
}

.game-row:focus-visible {
    color: #071019;
    background-color: #4de2bd;
    border: 2px #b8ffed;
}

.game-system {
    margin-left: auto;
    color: #64718a;
}
```

这里必须给 `.game-list` 一个确定的高度或 `max-height`。如果容器高度仍是
`auto`，它会被所有子项撑开，就没有溢出，也不会发生滚动。

`overflow-y: auto` 只在内容超出时生成滚动条；`scroll` 则始终预留滚动条。

## 掌机方向键如何让列表跟着滚动

RmlUi 原生键盘导航在切换焦点后会调用 `ScrollIntoView(Nearest)`。但当前原型
没有把 SDL 方向键转换成 RmlUi 的 `ProcessKeyDown()`；它由自定义
`FocusController` 直接调用 `Element::Focus(true)`。直接 Focus 不会自动滚动，
所以选中下一项后还需要显式滚入视野：

```cpp
void Select(int index)
{
    selected = index;
    if (Rml::Element* element = document->GetElementById(kItemIds[selected])) {
        element->Focus(true);
        element->ScrollIntoView(Rml::ScrollIntoViewOptions(
            Rml::ScrollAlignment::Nearest,
            Rml::ScrollAlignment::Nearest));
    }
}
```

需要包含：

```cpp
#include <RmlUi/Core/ScrollTypes.h>
```

这种方式的行为最适合掌机：焦点还在可视区域时不移动列表；焦点越过顶部或
底部时，只滚动刚好足够的距离。

另一种方案是把方向键提交给 RmlUi：

```cpp
context->ProcessKeyDown(Rml::Input::KI_DOWN, 0);
```

然后给项目设置 `tab-index: auto` 和 `nav-up/nav-down`。正式桌面建议最终转向
这一方案，让焦点、滚动和表单控件共享同一套输入系统；原型阶段继续使用
`FocusController + ScrollIntoView()` 更容易控制和调试。

## 滚动条样式

RmlUi 自动生成的滚动条可以完全用 RCSS 设置。当前 renderer 不支持图片，
所以使用纯色即可：

```css
scrollbarvertical {
    width: 6px;
}

scrollbarvertical slidertrack {
    background-color: #0b101c;
}

scrollbarvertical sliderbar {
    min-height: 36px;
    background-color: #465572;
}

scrollbarvertical sliderbar:hover,
scrollbarvertical sliderbar:active {
    background-color: #4de2bd;
}

scrollbarvertical sliderarrowdec,
scrollbarvertical sliderarrowinc {
    display: none;
}

scrollbarcorner {
    background-color: #0b101c;
}
```

掌机通常不需要显示可点击箭头，也可以把整个滚动条设得很窄，主要用它提示
当前位置。如果完全不需要视觉滚动条，可以使用 `overflow-y: hidden` 并由
C++ 调用 `SetScrollTop()`，但这时 `ScrollIntoView()` 不会把该容器视为可滚动
容器，因此需要自己计算位置。推荐继续使用 `overflow-y: auto`。

## 长列表性能

- 几十到几百个简单行元素可以直接放进 DOM；
- 封面图片加入后，重点控制纹理数量和尺寸；
- 数千个游戏条目不要一次全部创建，应做虚拟列表，只保留可视区及前后少量
  行；
- 行高固定时最容易虚拟化，也最适合方向键导航；
- 更新数据时优先复用行元素，不要每帧重建整份 RML。

第一版游戏库列表建议先做 30～100 个纯文字项目，验证焦点、滚动、按键重复
和帧率，再加入封面和虚拟化。
