# liboliveui.so — UI 层

> **依赖**：`libolivecore.so`, `libolivecodec.so`, `libolivenode.so`, `liboliverender.so`, `liboliveaudio.so`  
> **外部依赖**：Qt::Widgets, Qt::OpenGL, Qt::OpenGLWidgets, KDDockWidgets  
> **包含源码**：`app/widget/`, `app/panel/`, `app/window/`, `app/dialog/`, `app/tool/`, `app/ui/`  
> **当前状态**：单体 OBJECT 库的一部分  
> **改造难度**：⭐⭐⭐（中等，但代码量大）

---

## 1. 当前状态分析

UI 层是代码量最大的模块，但**耦合方向是单向的**：UI 依赖下层（node/render/codec），下层不依赖 UI。这使得 UI 层的拆分相对直接。

| 组件 | 说明 |
|---|---|
| `widget/` | 40+ 自定义 Qt Widget（节点视图、时间线、播放控制、颜色轮等） |
| `panel/` | 基于 KDDockWidgets 的可停靠面板包装 |
| `window/mainwindow/` | 主窗口 |
| `dialog/` | 模态对话框（导出、首选项、项目属性等） |
| `tool/` | 工具枚举 |
| `ui/` | 图标、光标、样式表、翻译资源 |

**关键问题**：
- `Core` 类（`core.h/cpp`）混合了业务逻辑和 UI 逻辑（`StartGUI()`, `main_window_` 等）。
- `RenderManager` 的 `RenderTicketWatcher` 使用 Qt 信号通知 UI。
- 大量 UI 类直接包含 `node/` 和 `render/` 的 C++ 头文件。

**策略**：
- `liboliveui.so` 的 C API 不需要非常完善，因为 UI 层**大概率仍然与主进程一同编译**（UI 是主进程的核心）。
- 但为了保持架构一致性，仍定义 C API 用于：
  1. 第三方脚本/插件通过 C API 操作 UI（未来扩展）。
  2. 单元测试通过 C API 驱动 UI（自动化测试）。
- **主进程中的 UI 代码可以继续使用 C++ 直接包含下层头文件**，不必全部改为 C API 调用。这是因为 UI 层在最顶层，不需要被其他模块依赖。

**修正策略**：`liboliveui.so` 的拆分重点在于：
1. 将 UI 代码从单体 OBJECT 库移出，编译为独立的 `liboliveui.so`。
2. 主进程显式加载 `liboliveui.so`。
3. UI 层内部继续使用 C++ 直接调用下层（node/render 等），只在跨库边界处遵循 ABI 规则。

---

## 2. C API 设计（精简版）

UI 层的 C API 不需要覆盖所有 Widget，只需提供应用级入口和关键面板操作：

### 2.1 头文件：`c_api/include/olive/ui_api.h`

```c
#ifndef OLIVE_UI_API_H
#define OLIVE_UI_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core_api.h"
#include "node_api.h"

#define OLIVE_UI_API_VERSION 1

#ifdef OLIVE_BUILDING_UI
#  define OLIVE_UI_API __attribute__((visibility("default")))
#else
#  define OLIVE_UI_API
#endif

/* ========== 不透明类型 ========== */
typedef struct OliveApplication    OliveApplication;
typedef struct OliveMainWindow     OliveMainWindow;
typedef struct OliveViewerPanel    OliveViewerPanel;
typedef struct OliveTimelinePanel  OliveTimelinePanel;
typedef struct OliveNodeEditorPanel OliveNodeEditorPanel;

/* ========== API 版本 ========== */
OLIVE_UI_API int olive_ui_api_version(void);

/* ========== 应用生命周期 ========== */
OLIVE_UI_API OliveApplication* olive_ui_app_create(int argc, char** argv);
OLIVE_UI_API int olive_ui_app_exec(OliveApplication* app);
OLIVE_UI_API void olive_ui_app_quit(OliveApplication* app);
OLIVE_UI_API void olive_ui_app_destroy(OliveApplication* app);

/* ========== 主窗口 ========== */
OLIVE_UI_API OliveMainWindow* olive_ui_main_window_create(OliveApplication* app);
OLIVE_UI_API void olive_ui_main_window_destroy(OliveMainWindow* win);
OLIVE_UI_API void olive_ui_main_window_show(OliveMainWindow* win);
OLIVE_UI_API void olive_ui_main_window_set_fullscreen(OliveMainWindow* win, int fullscreen);

/* ========== 项目操作 ========== */
OLIVE_UI_API int olive_ui_open_project(OliveMainWindow* win, const char* filename);
OLIVE_UI_API int olive_ui_save_project(OliveMainWindow* win, const char* filename);
OLIVE_UI_API int olive_ui_import_footage(OliveMainWindow* win, const char** filenames, int count);

/* ========== 查看器（Viewer） ========== */
OLIVE_UI_API OliveViewerPanel* olive_ui_get_active_viewer(OliveMainWindow* win);
OLIVE_UI_API void olive_ui_viewer_set_time(OliveViewerPanel* viewer, OliveRational time);
OLIVE_UI_API void olive_ui_viewer_play(OliveViewerPanel* viewer);
OLIVE_UI_API void olive_ui_viewer_pause(OliveViewerPanel* viewer);
OLIVE_UI_API void olive_ui_viewer_stop(OliveViewerPanel* viewer);

/* ========== 时间线 ========== */
OLIVE_UI_API OliveTimelinePanel* olive_ui_get_active_timeline(OliveMainWindow* win);
OLIVE_UI_API void olive_ui_timeline_set_time(OliveTimelinePanel* timeline, OliveRational time);
OLIVE_UI_API void olive_ui_timeline_set_work_area(OliveTimelinePanel* timeline,
                                                   OliveRational in,
                                                   OliveRational out);

/* ========== 节点编辑器 ========== */
OLIVE_UI_API OliveNodeEditorPanel* olive_ui_get_node_editor(OliveMainWindow* win);
OLIVE_UI_API void olive_ui_node_editor_set_graph(OliveNodeEditorPanel* editor,
                                                  OliveNodeGraph* graph);

/* ========== 导出对话框 ========== */
OLIVE_UI_API int olive_ui_show_export_dialog(OliveMainWindow* win,
                                              OliveViewerOutput* viewer_output,
                                              const char* default_filename);

/* ========== 状态栏消息 ========== */
OLIVE_UI_API void olive_ui_show_status_message(OliveMainWindow* win,
                                                const char* message,
                                                int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif  // OLIVE_UI_API_H
```

---

## 3. CMake 改造

```cmake
# 由于 UI 层代码分散在 widget/, panel/, window/, dialog/, tool/, ui/ 多个目录，
# 需要在 app/CMakeLists.txt 中统一聚合。

set(UI_INTERNAL_SOURCES
  # widget/
  widget/viewer/viewerwidget.cpp widget/viewer/viewerwidget.h
  widget/timelinewidget/timelinewidget.cpp widget/timelinewidget/timelinewidget.h
  widget/nodeview/nodeview.cpp widget/nodeview/nodeview.h
  # ... 所有 widget 源文件

  # panel/
  panel/viewer/viewerpanel.cpp panel/viewer/viewerpanel.h
  panel/timeline/timelinepanel.cpp panel/timeline/timelinepanel.h
  panel/node/nodepanel.cpp panel/node/nodepanel.h
  # ... 所有 panel 源文件

  # window/
  window/mainwindow/mainwindow.cpp window/mainwindow/mainwindow.h

  # dialog/
  dialog/export/exportdialog.cpp dialog/export/exportdialog.h
  dialog/preferences/preferencesdialog.cpp dialog/preferences/preferencesdialog.h
  # ... 所有 dialog 源文件

  # tool/
  tool/tool.cpp tool/tool.h

  # ui/ 资源（.qrc 等）
  # ...
)

set(UI_API_SOURCES
  ${CMAKE_SOURCE_DIR}/c_api/src/ui_api.cpp
)

add_library(oliveui SHARED
  ${UI_INTERNAL_SOURCES}
  ${UI_API_SOURCES}
)

target_compile_definitions(oliveui PRIVATE OLIVE_BUILDING_UI)

target_include_directories(oliveui
  PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/c_api/include
    ${CMAKE_SOURCE_DIR}/ext/KDDockWidgets/src
  PUBLIC
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(oliveui
  PUBLIC
    olivenode
    oliverender
    olivecodec
    oliveaudio
    olivecore
    oliveplugin
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    KDAB::kddockwidgets
)

set_target_properties(oliveui PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

install(TARGETS oliveui DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/c_api/include/olive/ui_api.h DESTINATION include/olive)
```

---

## 4. 小步快跑实施步骤

### Step 0: 解耦 Core 类（2 天）

- [ ] 分析 `core.h/cpp` 中哪些属于 UI 逻辑（`StartGUI()`, `main_window_`, `ImportFiles()` 等），哪些属于业务逻辑。
- [ ] 将 UI 相关逻辑迁移到 `liboliveui.so` 内部的一个 `UiCore` 类中。
- [ ] 保留 `Core` 类中纯业务逻辑（如 `FootageFileDialogFilter`, `CreateNewSequenceForProject`）。

**验收标准**：`core.h` 不再包含 `mainwindow.h` 或 `projectexplorer.h` 等 UI 头文件。

### Step 1: 聚合 UI 源码（1 天）

- [ ] 在 `app/CMakeLists.txt` 中聚合所有 UI 相关源文件（widget/, panel/, window/, dialog/, tool/, ui/）。
- [ ] 确保 `liboliveui.so` 可以编译。

**验收标准**：`liboliveui.so` 编译成功。

### Step 2: 主进程加载 UI 库（1 天）

- [ ] 修改 `main.cpp`：先通过 `ModuleLoader` 加载 `liboliveui.so`，然后调用 `olive_ui_app_create` 和 `olive_ui_main_window_create`。
- [ ] 若动态加载失败，回退到静态链接模式。

**验收标准**：主程序启动时日志显示成功加载 `ui` 模块，并正常显示主窗口。

### Step 3: C API 实现（按需，2–3 天）

- [ ] 实现 `olive_ui_app_create/exec/quit/destroy`。
- [ ] 实现 `olive_ui_main_window_create/show`。
- [ ] 实现项目操作：`open_project`, `save_project`, `import_footage`。
- [ ] 其他 UI C API 根据测试/脚本需求逐步实现。

**验收标准**：可以通过一个外部测试程序加载 `liboliveui.so` 并打开主窗口。

---

## 5. 风险与回退

| 风险 | 对策 |
|---|---|
| UI 代码量巨大，聚合时容易遗漏源文件 | 编写脚本自动收集 `widget/`, `panel/`, `window/`, `dialog/` 下的所有 `.cpp`/`.h` 文件，或在 CMake 中保持原有的 `add_subdirectory` 结构，只是最终输出为 SHARED 而非 OBJECT。 |
| KDDockWidgets 的符号跨动态库 | KDDockWidgets 以静态库形式链接进 `liboliveui.so`，其符号不外泄。确保 `liboliveui.so` 的 `CXX_VISIBILITY_PRESET hidden`。 |
| Qt 资源文件（.qrc）在动态库中的加载 | 将 `.qrc` 编译进 `liboliveui.so`，Qt 的资源系统在动态库中工作正常。确保 `Q_INIT_RESOURCE()` 在库加载时被调用。 |
| `Core` 类的信号槽跨模块 | `Core` 保留在主进程，`UiCore` 在 `liboliveui.so` 中。两者通过 C API 或 Qt 的跨进程信号（如果未来需要）通信。初期保持简单：主进程直接调用 UI 的 C API。 |
