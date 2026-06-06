# v0.4 调色与 LUT 实施计划

本文档对应 `docs/zh/README.md` 路线图中 v0.4「调色与 LUT」里程碑。

## 目标

- 支持 `.cube` 与 `.3dl` LUT 文件作为可用调色入口。
- 完成示波器面板的波形、矢量、直方图三类视图。
- 提供三向色轮面板，面向阴影、中间调、高光做基础调色控制。
- 尽量复用现有 OpenColorIO、节点系统、Viewer/Scope 面板和 GPU 渲染管线，不引入独立的调色框架。

## 当前状态

- 已有 OpenColorIO 基础能力：颜色管理、显示变换、OCIO 调色节点和渲染侧配置。
- Scope 面板已提供波形、矢量、直方图三类视图。
- 已有色轮基础控件；当前三向调色先通过节点参数面板暴露 Shadows、Midtones、Highlights 三组颜色与强度参数。
- LUT 节点已接入节点工厂，并已增加 `.cube`/`.3dl` 相关测试。

## 阶段 1：LUT 节点入口

状态：已完成首版。

交付内容：

- 新增 OCIO LUT 节点，使用 OpenColorIO `FileTransform` 读取外部 LUT 文件。
- 明确支持 `.cube` 与 `.3dl` 扩展名，并拒绝未知格式。
- 在节点工厂中注册 LUT 节点，保证工程加载和节点创建路径一致。
- 增加 gtest 覆盖 LUT 扩展名支持和简单 LUT 转换结果。

验收标准：

- `olive-gtest` 中 LUT 相关测试通过。
- `olive-editor` 和 `olive-render-worker` 可正常构建。
- LUT 文件缺失、格式不支持、OCIO 处理器创建失败时不会导致崩溃。

## 阶段 2：示波器补齐

状态：已完成首版。

交付内容：

- 保留现有波形和直方图视图。
- 新增矢量示波器视图，并接入 Scope 面板下拉选择。
- 矢量示波器应使用当前 Viewer 帧，并经过现有显示/颜色管理路径。
- 为新增 shader 或资源入口增加资源存在性测试。

验收标准：

- Scope 面板可在 Waveform、Vectorscope、Histogram 间切换。
- 无当前帧时视图保持空白或安全占位，不崩溃。
- shader 资源测试和编辑器构建通过。

## 阶段 3：三向色轮面板

状态：已完成节点参数面板首版；独立三向色轮 dock 面板作为后续体验增强。

交付内容：

- 基于现有参数面板提供 Shadows、Midtones、Highlights 三组控制。
- 为每组控制提供色彩偏移和强度/亮度相关参数。
- 将三向色轮参数映射到现有 OCIO 调色节点，或新增可序列化节点承载参数。
- 保证参数能随工程保存、加载、撤销和重做。

验收标准：

- 用户可以在 UI 中操作三向调色参数并看到 Viewer 结果变化。
- 参数在工程文件中可序列化并可恢复。
- 节点参数变更不破坏现有 OCIO 调色节点兼容性。

## 阶段 4：集成与体验

状态：当前范围已完成；独立三向色轮 dock 面板和更细的交互体验作为后续增强。

交付内容：

- 为 LUT 节点补齐清晰的文件选择过滤器和用户可见名称。已完成。
- 在调色相关 UI 中保持命名一致：LUT、Waveform、Vectorscope、Histogram、Shadows、Midtones、Highlights。已完成首版。
- 更新中文文档，说明 LUT、示波器和三向调色的当前入口。已在本文档记录。

验收标准：

- 用户能从现有节点/UI 路径发现 LUT 和调色功能。
- 文档与实际 UI 命名一致。
- LUT 文件选择器限制为 `.cube` 与 `.3dl`，并保留 All Files 兜底。
- 不引入和现有翻译系统冲突的硬编码字符串。

## 阶段 5：验证

状态：自动化构建和核心测试已通过；手动 Viewer/Scope 观感检查将在真实项目中继续验证。

构建命令：

```sh
ninja -C cmake-build-debug olive-gtest olive-editor olive-render-worker -j2
```

测试命令：

```sh
QT_QPA_PLATFORM=offscreen cmake-build-debug/tests/gtest/olive-gtest --gtest_filter='ColorLut.*:ColorV04.*:Shaders.*:NodeSerialization.*:NodeValue.*' --gtest_brief=1
```

手动检查：

- 打开工程并加载一段素材。
- 在 Scope 面板分别切换 Waveform、Vectorscope、Histogram。
- 添加 LUT 节点并选择 `.cube` 或 `.3dl` 文件。
- 调整三向色轮参数，确认 Viewer 输出和工程保存/加载行为。

## 风险与待定点

- 三向色轮应优先映射到 OCIO 现有调色能力；如果现有节点表达能力不足，再新增独立节点。
- 矢量示波器 shader 需要兼容当前 OpenGL 版本和已有渲染抽象，避免只在单一驱动上可用。
- LUT 文件路径序列化需要尊重现有工程文件路径策略，避免绝对路径导致工程不可迁移。
