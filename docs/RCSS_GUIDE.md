# RmlUi 6.2 RCSS 使用指南

本项目使用 RmlUi `6.2`，固定提交为
`2230d1a6e8e0848ed87a5761e2a5160b2a175ba4`。RmlUi 使用的样式语言叫
RCSS。它接近 CSS2，并吸收了一部分 CSS3，但不是浏览器，也不支持完整的
现代 CSS。

还需要区分两层能力：

1. **RmlUi 6.2 核心支持**：RCSS 能被解析和布局；
2. **本项目 renderer 支持**：`PrototypeRenderInterface` 是否实现了绘制该效果
   所需的纹理、变换、图层、遮罩或 shader。

日常写界面时，可以放心使用盒模型、Flexbox、纯色、边框和文字。图片、复杂
渐变、滤镜和 transform 需要先补 renderer。

## RCSS 放在哪里

推荐把样式放在独立 `.rcss` 文件中，在 RML 的 `<head>` 中引用：

```xml
<rml>
<head>
    <link type="text/css" href="main.rcss" />
</head>
<body>
    <div class="card">内容</div>
</body>
</rml>
```

也支持 `<style>` 和元素的 `style="..."`，但正式界面优先使用外部 RCSS。
注释使用 `/* ... */`。

## 选择器

RmlUi 6.2 支持这些常用选择器：

```css
*                         /* 全部元素 */
button                    /* 标签 */
.card                     /* class */
#settings                 /* id */
button.primary            /* 组合 */
.sidebar .item            /* 后代 */
.sidebar > .item          /* 直接子元素 */
.item + .item             /* 紧邻兄弟 */
.item ~ .item             /* 后续兄弟 */
[disabled]
[data-kind="game"]
[data-kind^="game-"]
.item:hover
.item:active
.item:focus
.item:first-child
.item:last-child
.item:nth-child(2n + 1)
.item:not(.disabled)
```

结构伪类还包括 `:first-of-type`、`:last-of-type`、`:only-child`、
`:only-of-type`、`:empty`、`:nth-last-child()`、`:nth-of-type()` 和
`:nth-last-of-type()`。

不要依赖 `::before`、`::after`、`::first-letter` 等伪元素；6.2 不支持它们。
RmlUi 6.2 支持动态 `:focus-visible`。本原型调用 `Focus(true)`，所以可以同时
用 `:focus` 表示所有焦点，用 `:focus-visible` 表示需要显示焦点框的焦点。

## 数值、单位和颜色

可用单位：

- 长度：`px`、`dp`、`vw`、`vh`、`em`、`rem`、`in`、`cm`、`mm`、
  `pt`、`pc`；
- 比例：`%`；
- 角度：`deg`、`rad`；
- 分辨率媒体查询：`x`，例如 `2x`；
- 无单位数字，例如 `opacity: 0.8`、`line-height: 1.4`。

Brick 的 1024×768 对角线正好是 1280 像素。若面板为精确的 3.2 英寸，则是
400 PPI；标称 405 PPI 来自面板尺寸取值或规格四舍五入。所谓“几倍屏”取决于
比较基准：

- 相对网页/CSS 的 96 PPI，大约是 `4.2x`；
- 相对 Android `mdpi = 160 dpi`，大约是 `2.5x`；
- 不能仅凭 PPI 判断苹果式 `2x/3x`，因为它还取决于设备采用的逻辑分辨率。

RmlUi 不会自动读取 PPI。当前原型未调用
`Context::SetDensityIndependentPixelRatio()`，因此默认 `1dp = 1px`。在
405 PPI 的 3.2 英寸屏幕上，`20px` 文字的实际高度只有约 1.25 mm，确实偏小。

建议保持 RmlUi 和 SDL 使用 1024×768 原生像素进行清晰渲染，同时先设置：

```cpp
context->SetDensityIndependentPixelRatio(2.0f);
```

然后把界面主要尺寸改用 `dp`：

```css
body       { font-size: 18dp; }  /* 36 个物理像素 */
.caption   { font-size: 14dp; }  /* 28 个物理像素 */
.title     { font-size: 28dp; }  /* 56 个物理像素 */
.game-row  { height: 52dp; }     /* 104 个物理像素 */
.hairline  { border: 1px #33405a; } /* 保留 1 像素细线 */
```

`dp_ratio = 2` 相当于按 512×384 的逻辑尺寸设计，但最终仍在 1024×768 上生成
和绘制字体，不会像把低分辨率画面整体放大那样模糊。若实机仍偏小，可以试
`2.25` 或 `2.5`；`2.5` 对应约 410×307 的逻辑工作区，更接近手机密度，但
一屏能容纳的内容会更少。

建议的单位分工：

- 字号、控件高度、padding、gap、圆角使用 `dp`；
- 发丝边框和需要对齐物理像素的细节使用 `px`；
- 容器宽高和两栏比例使用 `%` 或 Flexbox；
- `vw`、`vh` 只用于确实需要跟随整个屏幕的尺寸。

颜色可以写成：

```css
color: #4de2bd;
color: #4de2bd80;          /* 最后两位是 alpha */
color: rgb(77, 226, 189);
color: rgba(77, 226, 189, 180);
color: hsl(164, 72%, 59%);
color: transparent;
```

6.2 也支持 `lab()`、`lch()`、`oklab()` 和 `oklch()`，但掌机界面建议使用
十六进制颜色，最直观也最容易复现。

## 可用属性速查

### 盒模型和尺寸

```text
width, min-width, max-width
height, min-height, max-height
margin, margin-top, margin-right, margin-bottom, margin-left
padding, padding-top, padding-right, padding-bottom, padding-left
box-sizing
```

推荐全局使用：

```css
* { box-sizing: border-box; }
```

### 边框和圆角

```text
border, border-top, border-right, border-bottom, border-left
border-width, border-*-width
border-color, border-*-color
border-radius, border-top-left-radius, border-top-right-radius
border-bottom-right-radius, border-bottom-left-radius
```

RCSS 的边框没有浏览器中的 `solid`、`dashed`、`dotted` 等 style 值：

```css
/* 正确 */
border: 2px #4de2bd;

/* 不要写 */
border: 2px solid #4de2bd;
```

圆角背景和圆角边框可由 RmlUi 生成，但本 renderer 没有实现 clip mask，因此
`border-radius` 配合 `overflow: hidden` 时，子内容不会可靠地按圆角裁切。

### 布局

`display` 支持：

```text
none, block, inline, inline-block, flow-root
flex, inline-flex
table, inline-table, table-row, table-row-group
table-column, table-column-group, table-cell
```

其他布局属性：

```text
position: static | relative | absolute | fixed
top, right, bottom, left, inset
float, clear, z-index
vertical-align
```

不支持 `position: sticky`，也不支持 CSS Grid。

### Flexbox

```text
display: flex | inline-flex
flex, flex-basis, flex-grow, flex-shrink
flex-direction, flex-wrap, flex-flow
justify-content
align-items, align-self, align-content
gap, row-gap, column-gap
```

这是掌机界面的首选布局方式。例如：

```css
.toolbar {
    display: flex;
    flex-direction: row;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
}

.content {
    flex: 1 1 auto;
    min-width: 0;
}
```

### 背景、显示和裁剪

```text
background, background-color
color, opacity, visibility
overflow, overflow-x, overflow-y
clip, z-index
image-color
```

`overflow` 支持 `visible`、`hidden`、`auto`、`scroll`。当前 renderer 已实现
矩形 scissor，所以普通矩形裁剪可用。

`background` 在 RCSS 中只是 `background-color` 的简写，不接受
`url(...)` 或标准 CSS 的 `linear-gradient(...)`。

### 字体和文字

```text
font, font-family, font-style, font-weight, font-size, font-kerning
line-height, letter-spacing
text-align, text-decoration, text-transform
white-space, word-break, text-overflow
caret-color
font-effect
--rmlui-language, --rmlui-direction
```

注意：

- 字体必须先由 C++ 调用 `Rml::LoadFontFace()` 加载；没有 `@font-face`；
- `font-family` 只能写一个字体族，不能写浏览器式 fallback 列表；
- `font-style` 只有 `normal` 和 `italic`；
- `font-weight` 可写 `normal`、`bold` 或 `1..1000`，但必须已经加载对应字重；
- `text-align` 在 6.2 实际应使用 `left`、`right`、`center`，不要依赖
  `justify`；
- 没有标准 `text-shadow`，使用 `font-effect`。

`--rmlui-language` 和 `--rmlui-direction: auto | ltr | rtl` 是 RmlUi 保留的
内建属性。它们的 `--` 前缀不代表 6.2 支持任意 CSS 变量。

文字效果示例：

```css
.title {
    font-effect: shadow(2px 2px #0008);
}

.selected {
    font-effect: outline(1px #ffffff80);
}
```

可用 font effect 包括 `shadow`、`outline`、`glow` 和 `blur`。它们通过字体
纹理生成，理论上兼容当前 renderer，但还需要分别做实机性能测试。

### 焦点和输入相关属性

```text
cursor, pointer-events
tab-index, focus
nav-up, nav-right, nav-down, nav-left, nav
drag
scrollbar-margin, overscroll-behavior
```

RCSS 本身提供焦点和空间导航属性，但当前原型的上下键由 C++
`FocusController` 管理固定列表，因此 `nav-*` 暂时不会改变掌机方向键行为。
视觉焦点样式可以正常使用：

```css
.menu-item:focus {
    color: #071019;
    background-color: #4de2bd;
    border: 2px #b8ffed;
}
```

## 动画、渐变和高级效果

RmlUi 6.2 核心支持：

```text
transition
animation 和 @keyframes
transform, transform-origin
perspective, perspective-origin
decorator 和 @decorator
filter, backdrop-filter, mask-image, box-shadow
```

但这些能力依赖 renderer，不能一概认为本原型已经可用。

### 当前可以尝试

纯色、透明度、位置和尺寸的 transition/animation 不依赖高级 shader，可以
尝试：

```css
.menu-item {
    transition: background-color 0.12s, color 0.12s, opacity 0.12s;
}

@keyframes pulse {
    from { opacity: 0.65; }
    to   { opacity: 1; }
}

.notice {
    animation: pulse 0.8s alternate infinite;
}
```

RmlUi 特有的两色直线渐变由普通顶点颜色生成，适合当前 renderer：

```css
.panel {
    decorator: vertical-gradient(#151d2e #0d1320);
}
```

### 当前不要使用

以下语法能被 RmlUi 6.2 解析，但本项目的最小 renderer 尚未实现相应接口：

- `transform` 和 `perspective`：缺少 `SetTransform()`；
- `filter`、`backdrop-filter`、`box-shadow`：缺少离屏 layer 和 filter；
- `mask-image` 和圆角内容遮罩：缺少 clip mask；
- `linear-gradient`、`radial-gradient`、`conic-gradient` decorator：缺少
  gradient shader；
- `shader(...)` decorator：缺少自定义 shader；
- `image(...)`、`ninepatch(...)`、tiled decorator、`fill-image` 和 `<img>`：
  `LoadTexture()` 目前有意返回失败。

这些不是 RCSS 文件写错，而是 C++ renderer 还需要继续实现。

## 媒体查询

6.2 支持 `@media`，可判断 `width`、`height`、`aspect-ratio`、`resolution`、
`orientation` 和 RmlUi 自定义的 `theme`：

```css
@media (max-width: 800px) {
    .sidebar { width: 210px; }
}

@media (orientation: landscape) and (min-width: 1000px) {
    .cards { flex-direction: row; }
}
```

限制是：不能嵌套 `@media`，不支持 CSS Level 4 的比较写法，也不能用复杂
括号嵌套条件。Brick 当前固定为 1024×768，一般不必大量使用媒体查询。

## 和网页 CSS 的主要差异

以下浏览器写法在本项目中不要使用：

```text
display: grid 以及全部 grid-* 属性
background-image: url(...)
background: linear-gradient(...)
border: 1px solid ... 中的 solid
position: sticky
calc(), min(), max(), clamp()
::before, ::after 等伪元素
@font-face
!important
inherit, initial, unset, revert 等 CSS-wide 关键字
vmin, vmax, ch, ex 等单位
```

RmlUi 官网的最新文档已经出现 custom properties 和 `var()`，但它们尚未进入
本项目固定的 6.2 源码。因此现在不要写：

```css
/* RmlUi 6.2 不支持 */
:root { --accent: #4de2bd; }
.item { color: var(--accent); }
```

在升级 RmlUi 前，颜色和尺寸 token 可以先通过统一的 RCSS 规则集中维护。

## 本原型的推荐子集

为了在 Brick 上获得最稳定、最容易调试的结果，第一阶段建议只使用：

- `block`、`inline`、`flex` 布局；
- 固定 `px` 尺寸，少量 `%`；
- margin、padding、gap；
- 纯色背景、边框、颜色和 opacity；
- 字体、字号、行高、字距、对齐、截断；
- `overflow: hidden` 的矩形裁剪；
- `:focus`、`:focus-visible`、class、id 和后代选择器；
- 必要时使用简短的颜色/透明度 transition；
- 可选的 `horizontal-gradient` / `vertical-gradient`。

完整属性说明见 [RmlUi RCSS property index](https://mikke89.github.io/RmlUiDoc/pages/rcss/property_index.html)，
但官网内容可能领先于 6.2。出现差异时，以本项目固定提交的
[StyleSheetSpecification.cpp](https://github.com/mikke89/RmlUi/blob/2230d1a6e8e0848ed87a5761e2a5160b2a175ba4/Source/Core/StyleSheetSpecification.cpp)
和当前 renderer 实现为准。
