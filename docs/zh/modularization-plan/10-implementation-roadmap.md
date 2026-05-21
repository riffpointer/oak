# 10 周小步快跑实施路线图

> 本路线图为**渐进式、可回退、可并行**的实施计划。每一周都有明确的交付物和验收标准。任何一周的任务如果超时或遇到阻碍，都可以独立回退或跳过，不影响其他周的进度。

---

## 关键原则

1. **不改现有代码，只增代码**：每一周的改造都通过新增文件（`c_api/`、`app/render/renderer_main.cpp` 等）完成，现有源文件尽量不动。只有必须解耦的地方才修改现有头文件。
2. **编译开关控制**：通过 CMake 选项 `-DOLIVE_DYNAMIC_MODULES=ON` 控制是否走动态库路径。默认 OFF，确保主干始终可编译可运行。
3. **每周可独立验证**：每周结束都有一个可运行的版本，即使后续周不开始，当前成果也是有价值的。
4. **先易后难**：从耦合最低的模块（core、codec）开始，积累经验和工具链，最后攻克最难的 node/render。

---

## 人员分工建议（假设 2–3 人）

| 角色 | 负责内容 |
|---|---|
| **基础设施工程师** | ModuleLoader、CMake 改造、CI 适配、C API 规范执行 |
| **编解码工程师** | `libolivecore.so`、`libolivecodec.so`、`liboliveaudio.so` |
| **渲染工程师** | `libolivenode.so` 解耦、`liboliverender.so`、`olive-renderer` 多进程 |
| **UI 工程师** | `liboliveui.so`、主进程集成、ViewerWidget 改造 |

**注意**：初期阶段（Week 1–3）只需 1 人负责基础设施 + 1 人负责编解码即可。渲染和 UI 改造在 Week 4 之后全面展开。

---

## Week 1：基础设施与工具链（1 人主责）

### 目标
建立 C API 基础设施，验证显式加载工具链在所有目标平台正常工作。

### 任务清单

- [ ] **T1.1** 创建目录结构 `c_api/include/olive/`, `c_api/src/`, `c_api/tests/`。
- [ ] **T1.2** 编写 `ModuleLoader` 类（`c_api/src/module_loader.h/cpp`），支持 POSIX + Windows。
- [ ] **T1.3** 定义全局 C API 规范文件：`c_api/include/olive/core_api.h` 的基础部分（`OliveResult`, `OliveRational`, 内存管理函数）。
- [ ] **T1.4** 编写最小测试动态库 `c_api/tests/test_module/`（只导出一个 `int test_add(int, int)`），验证 `ModuleLoader` 可以正确加载和调用。
- [ ] **T1.5** 在 CMake 中新增 `OLIVE_DYNAMIC_MODULES` 选项（默认 OFF）。
- [ ] **T1.6** 在 CI（GitHub Actions）中增加一个 job：开启 `OLIVE_DYNAMIC_MODULES=ON` 编译，验证 Linux/macOS/Windows 三平台。

### 验收标准

```bash
# 运行测试
./tests/c_api/test_module_loader
# 输出：
# [PASS] Load test_module.so
# [PASS] Call test_add(2, 3) = 5
# [PASS] Unload test_module.so
```

### 回退策略

若 `ModuleLoader` 在某平台工作异常，该周可仅完成 POSIX 平台，Windows 平台延后处理。

---

## Week 2：libolivecore.so（1 人主责）

### 目标
将 `ext/core/` 从静态库改造为显式加载的动态库，建立第一个完整的 C API。

### 任务清单

- [ ] **T2.1** 修改 `ext/core/CMakeLists.txt`：`add_library(olivecore SHARED ...)`，添加 `CXX_VISIBILITY_PRESET hidden`。
- [ ] **T2.2** 为 `ext/core/` 中需要跨库使用的类添加导出宏（或保持 C API 头文件中的 `OLIVE_CORE_API`）。
- [ ] **T2.3** 完成 `c_api/include/olive/core_api.h`：Rational, Color, TimeRange, Timecode, PixelFormat, SampleBuffer, VideoParams, AudioParams。
- [ ] **T2.4** 编写 `c_api/src/core_api.cpp`，封装所有上述类型。
- [ ] **T2.5** 编写单元测试 `tests/c_api/test_core_api.cpp`。
- [ ] **T2.6** 在主进程 `Core::Start()` 中尝试显式加载 `libolivecore.so`，失败时打印警告但不阻塞启动。

### 验收标准

```cpp
ModuleLoader loader;
loader.Load("core", "./libolivecore.so");
auto make = loader.GetFunction<OliveRational(*)(int64_t,int64_t)>("core", "olive_rational_make");
auto add = loader.GetFunction<OliveRational(*)(OliveRational,OliveRational)>("core", "olive_rational_add");
OliveRational r = add(make(1,2), make(1,3));
assert(r.num == 5 && r.den == 6);
```

### 回退策略

若 C API 封装工作量超预期，本周可只完成 Rational + TimeRange 的最小子集，其余类型后续补充。

---

## Week 3：libolivecodec.so（1 人主责）

### 目标
将 `app/codec/` + `app/common/` 封装为显式加载动态库，实现解码器的 C API。

### 任务清单

- [ ] **T3.1** 将 `app/common/` 的源文件并入 `app/codec/CMakeLists.txt`。
- [ ] **T3.2** 创建 `libolivecodec.so` 的 SHARED 目标。
- [ ] **T3.3** 编写 `c_api/include/olive/codec_api.h`（最小子集）：MediaInfo, Decoder, Frame。
- [ ] **T3.4** 编写 `c_api/src/codec_api.cpp`。
- [ ] **T3.5** 编写测试：加载视频文件 → 解码第一帧 → 验证宽高 > 0。
- [ ] **T3.6** 在主进程中显式加载 `libolivecodec.so`。

### 验收标准

```cpp
auto decoder = olive_decoder_create(nullptr);
olive_decoder_open(decoder, "test.mp4", 0);
OliveFrame* frame = nullptr;
olive_decoder_decode_video(decoder, olive_rational_make(0,1), &frame);
assert(olive_frame_width(frame) == 1920);
olive_frame_destroy(frame);
olive_decoder_destroy(decoder);
```

### 回退策略

若 `common/` 中有代码依赖 `node/` 或 `render/`，先将这些代码移回主库，再继续。

---

## Week 4：libolivenode.so 解耦（2 人并行，最关键的一周）

### 目标
解决 `Node.h` 对 `render/` 的头文件依赖，为 `libolivenode.so` 的独立编译扫清障碍。

### 任务清单

- [ ] **T4.1** 创建 `app/node/nodecachecallbacks.h`，定义 `NodeCacheCallbacks` 纯虚接口。
- [ ] **T4.2** 修改 `app/node/node.h`：
  - 移除 `#include "render/rendercache.h"`。
  - 添加 `NodeCacheCallbacks* cache_callbacks_` 和 `SetCacheCallbacks()`。
  - 将 `InvalidateCache` 相关逻辑改为调用 `cache_callbacks_->InvalidateCache()`。
- [ ] **T4.3** 创建 `app/node/jobtypes.h`，定义 `NodeJobType` 和 `NodeJobData`。
- [ ] **T4.4** 修改 `Node.h` 中的 `ProcessXxx` 虚函数签名，使用 `NodeJobData`。
- [ ] **T4.5** 修改 `RenderProcessor`，适配新的 `NodeCacheCallbacks` 和 `NodeJobData`。
- [ ] **T4.6** 验证 `app/node/` 目录可以独立编译（写一个临时 CMake 测试）。

### 验收标准

```bash
cd /tmp && cmake /path/to/oak/app/node && make
# 编译成功，不报错
```

### 回退策略

**若解耦工作量超预期**：允许 `node/` 和 `render/` 暂时合并为 `libolive-engine.so`。这是最重要的回退策略——宁可合并也不阻塞进度。合并后仍可继续封装 C API，后续再拆分。

---

## Week 5：libolivenode.so C API + liboliveplugin.so（2 人并行）

### 目标
完成节点图系统的 C API，并将 OFX 插件宿主独立为动态库。

### 任务清单（节点图工程师）

- [ ] **T5.1** 创建 `libolivenode.so`，聚合 `node/`, `timeline/`, `undo/`, `config/`。
- [ ] **T5.2** 编写 `c_api/include/olive/node_api.h` 和 `c_api/src/node_api.cpp`。
- [ ] **T5.3** 实现最小 C API：NodeGraph create/destroy, load_xml/save_xml, add_node, connect, param set/get。
- [ ] **T5.4** 编写测试：用 C API 构建一个 Generator -> ViewerOutput 的图，序列化后反序列化验证。

### 任务清单（插件工程师）

- [ ] **T5.5** 将 `pluginSupport/` 从主 OBJECT 库移出，创建 `liboliveplugin.so`。
- [ ] **T5.6** 编写 `c_api/include/olive/plugin_api.h`（最小子集）：host create/destroy, add_path, rescan, plugin count/get。
- [ ] **T5.7** 验证主进程可以扫描 OFX 插件目录并列出插件名称。

### 验收标准

```cpp
// 节点图测试
OliveNodeGraph* g = olive_node_graph_create();
olive_node_graph_add_node(g, "SolidGenerator", "Solid1");
olive_node_graph_add_node(g, "ViewerOutput", "Viewer1");
OliveNode* solid = olive_node_graph_find_node(g, "Solid1");
OliveNode* viewer = olive_node_graph_find_node(g, "Viewer1");
olive_node_connect(solid, 0, viewer, 0);
size_t len;
char* xml = olive_node_graph_save_xml(g, &len);
assert(len > 0);
olive_core_free(xml);
```

### 回退策略

若 `node/` C API 工作量过大，优先保证 `load_xml` / `save_xml` / `find_node` 三个函数（这是渲染子进程最需要的），其余延后。

---

## Week 6：olive-renderer 单帧端到端（2 人并行）

### 目标
实现第一个可用的 `olive-renderer` 可执行文件，能渲染一帧测试图。

### 任务清单

- [ ] **T6.1** 编写 `app/render/renderer_main.cpp`，实现命令行解析。
- [ ] **T6.2** 实现 `olive_shm_create/open/close/unlink`（POSIX + Windows）。
- [ ] **T6.3** 在 `olive-renderer` 中集成：加载 XML → 初始化 OpenGL → 渲染 → 写入 SHM → 输出 JSON。
- [ ] **T6.4** 编写主进程中的 `RenderProcessLauncher::RenderFrameSync`。
- [ ] **T6.5** 端到端测试：主进程启动 `olive-renderer` 渲染一帧纯色，验证 SHM 中的像素值正确。

### 验收标准

```bash
# 命令行直接测试
olive-renderer --mode=frame --node-graph=test_solid.xml --time=0/1 \
  --video-params='{"width":100,"height":100,"format":"rgba32f"}' \
  --output-shm=/olive_test --output-shm-size=160000
# 输出：
# {"status":"ok","width":100,"height":100,"format":"rgba32f",...}
```

### 回退策略

若 OpenGL 离屏上下文初始化在某平台失败，该平台暂时使用 `--backend=dummy`（只测试进程模型，不测试实际渲染）。

---

## Week 7：ViewerWidget 集成 + 用完即弃验证（2 人并行）

### 目标
将 `olive-renderer` 集成到主进程的 Viewer 中，验证"用完即弃"模型在实际场景中的可行性。

### 任务清单

- [ ] **T7.1** 修改 `ViewerWidget`：从 `RenderManager::RenderFrame()` 改为启动 `olive-renderer`。
- [ ] **T7.2** 实现异步完成回调：`OnRenderProcessFinished()` 读取 SHM 并更新 Texture。
- [ ] **T7.3** 处理子进程崩溃：崩溃时忽略该帧，保持上一帧显示，记录日志。
- [ ] **T7.4** 测量实际性能：拖动时间线时的帧率、CPU 占用、进程启动耗时。
- [ ] **T7.5** 若性能不达标，实现批处理模式（`--mode=batch`）。

### 验收标准

- 打开一个简单项目（单轨道 + 纯色生成器），拖动时间线，Viewer 实时更新。
- `kill -9` 一个渲染子进程，主进程不崩溃，Viewer 保持显示。

### 回退策略

若"用完即弃"性能完全不可接受（如帧率 < 5fps），立即切换为**批处理模式**或**进程池模式**（预启动 N 个进程，循环使用，每个进程渲染一批后自杀）。

---

## Week 8：liboliveaudio.so + liboliveui.so（2 人并行）

### 目标
完成音频库和 UI 库的动态库拆分。

### 任务清单（音频工程师）

- [ ] **T8.1** 创建 `liboliveaudio.so`。
- [ ] **T8.2** 编写 `audio_api.h/cpp`（最小子集）：manager init/play/pause/push_buffer。
- [ ] **T8.3** 验证音频播放通过 C API 正常工作。

### 任务清单（UI 工程师）

- [ ] **T8.1** 聚合所有 UI 源文件，创建 `liboliveui.so`。
- [ ] **T8.2** 解耦 `Core` 类中的 UI 逻辑（`StartGUI()` 迁移到 `liboliveui.so` 内部）。
- [ ] **T8.3** 主进程显式加载 `liboliveui.so`，成功启动主窗口。
- [ ] **T8.4** 编写 `ui_api.h`（最小子集）：app create/exec, main_window show, open_project。

### 验收标准

- 主程序通过显式加载 `liboliveui.so` 启动，界面正常。
- 音频播放正常（可听到声音）。

---

## Week 9：导出集成 + 稳定性打磨（2 人并行）

### 目标
将导出流程集成到多进程渲染模型，全面稳定性测试。

### 任务清单

- [ ] **T9.1** 修改导出任务（`task/export/`），使用 `olive-renderer` 逐帧/逐批渲染。
- [ ] **T9.2** 导出天然适合批处理：一次性发送 10–50 帧给子进程。
- [ ] **T9.3** 编写压力测试：连续渲染 100 帧，验证无内存泄漏、无共享内存泄漏。
- [ ] **T9.4** 测试 OFX 插件在子进程中的渲染（选择几个免费 OFX 插件测试）。
- [ ] **T9.5** 测试崩溃场景：`kill -9` 随机子进程，验证主进程稳定。

### 验收标准

- 成功导出一个 10 秒视频（300 帧），画面正确。
- 连续启动 100 个渲染子进程，系统无共享内存泄漏（`ls /dev/shm/` 检查）。

---

## Week 10：打包适配 + 文档 + 性能优化（2 人并行）

### 目标
完成打包脚本适配，编写用户文档，进行最终性能优化。

### 任务清单

- [ ] **T10.1** 更新 macOS 打包脚本：确保 `libolive*.dylib` 和 `olive-renderer` 被打入 `.app` Bundle，`@rpath` 设置正确。
- [ ] **T10.2** 更新 Windows 打包脚本：确保 `.dll` 和 `olive-renderer.exe` 在_installer 中。
- [ ] **T10.3** 更新 Linux AppImage 打包：确保动态库在 AppImage 内可加载。
- [ ] **T10.4** 更新 `docs/build.md` 和 `docs/build-macos-zh.md`，说明新的运行时依赖。
- [ ] **T10.5** 性能优化：
  - 节点图 XML 缓存（相同图只序列化一次）。
  - 共享内存预分配池（避免反复创建/销毁）。
  - 批处理大小动态调整（根据上一批的渲染时间调整下一批的帧数）。
- [ ] **T10.6** 全面回归测试：导入、编辑、预览、导出、Undo/Redo、保存/加载项目。

### 验收标准

- 在三平台上都能通过 `make install` 或打包脚本生成可分发包。
- 新用户按照 `build.md` 可以成功编译并运行。
- 与 Week 0（改造前）相比，导出速度不差于 90%，预览帧率不差于 70%。

---

## 并行工作流

```
Week  1: [基础设施]
Week  2: [core]        (依赖 W1)
Week  3: [codec]       (依赖 W2)
Week  4: [node 解耦]    (可并行 W3, 但建议 W3 后启动)
Week  5: [node C API] + [plugin]  (依赖 W4)
Week  6: [renderer]    (依赖 W5)
Week  7: [Viewer 集成]  (依赖 W6)
Week  8: [audio] + [ui] (可并行 W7)
Week  9: [导出集成]     (依赖 W7)
Week 10: [打包/文档/优化] (依赖 W9)
```

**最大并行度**：Week 8 时可以有 3 人同时工作（1 人 audio，1 人 ui，1 人优化 renderer）。

---

## 回退总策略

| 场景 | 回退方案 |
|---|---|
| 某周任务无法按时完成 | 将该周剩余任务移到下一周，当前周只交付已完成部分。 |
| `node/` 解耦完全不可行 | 将 `node/` + `render/` 合并为 `libolive-engine.so`，后续再拆分。 |
| "用完即弃"性能完全不可接受 | 切换为"批处理模式"（每进程渲染 N 帧），或"进程池模式"（预启动 N 个进程）。 |
| 动态库在某平台加载失败 | 该平台暂时保持静态链接，其他平台先用动态库。 |
| C API 维护成本过高 | 保留 C API 用于子进程通信，主进程内部恢复直接 C++ 链接（但库仍编译为动态库，由操作系统隐式加载）。 |
| 项目期限紧张 | 优先完成 `olive-renderer` 多进程（核心价值），动态库拆分可以延后。 |
