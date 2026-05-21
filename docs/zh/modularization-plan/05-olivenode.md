# libolivenode.so — 节点图系统

> **依赖**：`libolivecore.so`, `libolivecodec.so`  
> **外部依赖**：Qt::Core（QObject, QString, XML）  
> **包含源码**：`app/node/`, `app/timeline/`, `app/undo/`, `app/config/`  
> **当前状态**：整个项目的**核心枢纽**，被 `render/`, `widget/`, `panel/`, `task/` 等几乎所有上层模块依赖  
> **改造难度**：⭐⭐⭐⭐⭐（最困难，耦合最深）

---

## 1. 当前状态分析

`app/node/` 是整个编辑器的**数据与计算模型核心**，采用节点图范式组织所有处理。其特点是：

1. **被几乎所有模块依赖**：`render/` 遍历节点图，`widget/` 绘制节点连接，`panel/` 包装节点编辑器，`task/` 在导出时读取节点图。
2. **头文件耦合严重**：`Node.h` 直接 `#include` 了 `codec/frame.h` 和 `render/` 下的多个头文件（缓存类型、作业类型等）。
3. **Qt 深度集成**：`Node` 继承 `QObject`，使用信号槽、元对象系统、`QVariant`。
4. **序列化内建**：`ProjectSerializer` 支持将节点图保存/加载为 XML。

### 1.1 关键耦合点与解耦策略

| 耦合点 | 当前状态 | 解耦策略 |
|---|---|---|
| `Node.h` 包含 `render/rendercache.h` | `Node` 直接操作 `FrameHashCache` | 将缓存失效抽象为虚函数 `InvalidateCache()`，或注入 `NodeCacheCallbacks` 接口指针。移除 `rendercache.h` 的包含。 |
| `Node.h` 包含 `render/job/*.h` | `Node::ProcessShader()` 等虚函数使用具体 Job 类型 | 将 `ProcessShader` 等改为接受 `const void* job_data` + `JobType` 枚举，内部再 `static_cast`。或前向声明 Job 类（若已是不透明指针）。 |
| `Footage`（`node/project/footage/`）依赖 `Decoder` | `Footage` 需要解码器信息预览 | 保留此依赖，`libolivenode.so` 链接 `libolivecodec.so` 是合理的。 |
| `ViewerOutput` 被 UI 直接引用 | `ViewerOutput` 是节点图与 UI 的桥梁 | `ViewerOutput` 保留在 `node/` 中，C API 暴露 `OliveViewerOutput*` 句柄。 |
| `timeline/` 依赖 `node/` | `TimelineMarker` 等引用 `Node` | `timeline/` 并入 `libolivenode.so`，不单独拆分。 |
| `undo/` 依赖 `node/` | `UndoCommand` 操作 `Node` 对象 | `undo/` 并入 `libolivenode.so`。 |

---

## 2. C API 设计

### 2.1 头文件：`c_api/include/olive/node_api.h`

```c
#ifndef OLIVE_NODE_API_H
#define OLIVE_NODE_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core_api.h"

#define OLIVE_NODE_API_VERSION 1

#ifdef OLIVE_BUILDING_NODE
#  define OLIVE_NODE_API __attribute__((visibility("default")))
#else
#  define OLIVE_NODE_API
#endif

/* ========== 不透明类型 ========== */
typedef struct OliveNodeGraph      OliveNodeGraph;
typedef struct OliveNode           OliveNode;
typedef struct OliveNodeInput      OliveNodeInput;
typedef struct OliveNodeOutput     OliveNodeOutput;
typedef struct OliveParam          OliveParam;
typedef struct OliveKeyframe       OliveKeyframe;
typedef struct OliveProject        OliveProject;
typedef struct OliveSequence       OliveSequence;
typedef struct OliveTrack          OliveTrack;
typedef struct OliveClip           OliveClip;
typedef struct OliveViewerOutput   OliveViewerOutput;

/* ========== 枚举 ========== */
typedef enum {
    OLIVE_NODE_TYPE_UNKNOWN = 0,
    OLIVE_NODE_TYPE_INPUT,
    OLIVE_NODE_TYPE_OUTPUT,
    OLIVE_NODE_TYPE_FILTER,
    OLIVE_NODE_TYPE_DISTORT,
    OLIVE_NODE_TYPE_GENERATOR,
    OLIVE_NODE_TYPE_COLOR,
    OLIVE_NODE_TYPE_AUDIO,
    OLIVE_NODE_TYPE_TRANSITION,
    OLIVE_NODE_TYPE_PLUGIN,
    OLIVE_NODE_TYPE_GROUP,
} OliveNodeType;

typedef enum {
    OLIVE_PARAM_TYPE_INT = 0,
    OLIVE_PARAM_TYPE_DOUBLE,
    OLIVE_PARAM_TYPE_STRING,
    OLIVE_PARAM_TYPE_RATIONAL,
    OLIVE_PARAM_TYPE_COLOR,
    OLIVE_PARAM_TYPE_BOOL,
    OLIVE_PARAM_TYPE_VECTOR2,
    OLIVE_PARAM_TYPE_VECTOR3,
    OLIVE_PARAM_TYPE_VECTOR4,
} OliveParamType;

/* ========== API 版本 ========== */
OLIVE_NODE_API int olive_node_api_version(void);

/* ========== NodeGraph ========== */
OLIVE_NODE_API OliveNodeGraph* olive_node_graph_create(void);
OLIVE_NODE_API void olive_node_graph_destroy(OliveNodeGraph* g);

// 序列化
OLIVE_NODE_API int olive_node_graph_load_xml(OliveNodeGraph* g,
                                              const char* xml_data,
                                              size_t xml_len);
OLIVE_NODE_API char* olive_node_graph_save_xml(OliveNodeGraph* g,
                                                size_t* out_len);

// 节点增删查
OLIVE_NODE_API OliveNode* olive_node_graph_add_node(OliveNodeGraph* g,
                                                      const char* node_type_id,
                                                      const char* node_id);
OLIVE_NODE_API int olive_node_graph_remove_node(OliveNodeGraph* g, OliveNode* node);
OLIVE_NODE_API OliveNode* olive_node_graph_find_node(OliveNodeGraph* g,
                                                       const char* node_id);
OLIVE_NODE_API int olive_node_graph_node_count(OliveNodeGraph* g);
OLIVE_NODE_API OliveNode* olive_node_graph_get_node(OliveNodeGraph* g, int index);

// 连接管理
OLIVE_NODE_API int olive_node_connect(OliveNode* from_node,
                                       int output_index,
                                       OliveNode* to_node,
                                       int input_index);
OLIVE_NODE_API int olive_node_disconnect(OliveNode* node, int input_index);
OLIVE_NODE_API OliveNode* olive_node_get_connected_node(OliveNode* node,
                                                          int input_index);

/* ========== Node 属性 ========== */
OLIVE_NODE_API const char* olive_node_get_id(OliveNode* node);
OLIVE_NODE_API const char* olive_node_get_label(OliveNode* node);
OLIVE_NODE_API OliveNodeType olive_node_get_type(OliveNode* node);
OLIVE_NODE_API const char* olive_node_get_type_id(OliveNode* node);

OLIVE_NODE_API int olive_node_input_count(OliveNode* node);
OLIVE_NODE_API int olive_node_output_count(OliveNode* node);

/* ========== Param 操作 ========== */
OLIVE_NODE_API int olive_node_param_count(OliveNode* node);
OLIVE_NODE_API OliveParam* olive_node_get_param(OliveNode* node, int index);
OLIVE_NODE_API OliveParam* olive_node_find_param(OliveNode* node,
                                                  const char* param_name);

OLIVE_NODE_API const char* olive_param_get_name(OliveParam* param);
OLIVE_NODE_API OliveParamType olive_param_get_type(OliveParam* param);

OLIVE_NODE_API int olive_param_set_int(OliveParam* param, int64_t value);
OLIVE_NODE_API int olive_param_set_double(OliveParam* param, double value);
OLIVE_NODE_API int olive_param_set_rational(OliveParam* param, OliveRational value);
OLIVE_NODE_API int olive_param_set_color(OliveParam* param, OliveColor value);
OLIVE_NODE_API int olive_param_set_string(OliveParam* param, const char* value);

OLIVE_NODE_API int64_t olive_param_get_int(OliveParam* param);
OLIVE_NODE_API double olive_param_get_double(OliveParam* param);
OLIVE_NODE_API OliveRational olive_param_get_rational(OliveParam* param);
OLIVE_NODE_API OliveColor olive_param_get_color(OliveParam* param);

/* ========== Keyframe ========== */
OLIVE_NODE_API int olive_param_add_keyframe(OliveParam* param,
                                             OliveRational time,
                                             double value);
OLIVE_NODE_API int olive_param_remove_keyframe(OliveParam* param,
                                                OliveRational time);
OLIVE_NODE_API int olive_param_keyframe_count(OliveParam* param);

/* ========== Project ========== */
OLIVE_NODE_API OliveProject* olive_project_create(const char* name);
OLIVE_NODE_API void olive_project_destroy(OliveProject* proj);
OLIVE_NODE_API int olive_project_load_file(OliveProject* proj, const char* filename);
OLIVE_NODE_API int olive_project_save_file(OliveProject* proj, const char* filename);
OLIVE_NODE_API OliveNodeGraph* olive_project_get_graph(OliveProject* proj);

/* ========== Sequence / Timeline ========== */
OLIVE_NODE_API OliveSequence* olive_sequence_create(const char* name,
                                                      OliveVideoParams vparams,
                                                      OliveAudioParams aparams);
OLIVE_NODE_API OliveViewerOutput* olive_sequence_get_viewer_output(OliveSequence* seq);

/* ========== ViewerOutput（渲染目标） ========== */
OLIVE_NODE_API const char* olive_viewer_output_get_node_id(OliveViewerOutput* viewer);
OLIVE_NODE_API OliveVideoParams olive_viewer_output_get_video_params(OliveViewerOutput* viewer);
OLIVE_NODE_API OliveAudioParams olive_viewer_output_get_audio_params(OliveViewerOutput* viewer);

/* ========== Undo ========== */
typedef struct OliveUndoStack OliveUndoStack;

OLIVE_NODE_API OliveUndoStack* olive_undo_stack_create(void);
OLIVE_NODE_API void olive_undo_stack_destroy(OliveUndoStack* stack);
OLIVE_NODE_API void olive_undo_stack_push(OliveUndoStack* stack,
                                           const char* action_name,
                                           void* undo_data,
                                           void (*undo_fn)(void*),
                                           void (*redo_fn)(void*),
                                           void (*free_fn)(void*));
OLIVE_NODE_API int olive_undo_stack_can_undo(OliveUndoStack* stack);
OLIVE_NODE_API int olive_undo_stack_can_redo(OliveUndoStack* stack);
OLIVE_NODE_API void olive_undo_stack_undo(OliveUndoStack* stack);
OLIVE_NODE_API void olive_undo_stack_redo(OliveUndoStack* stack);
OLIVE_NODE_API void olive_undo_stack_clear(OliveUndoStack* stack);

#ifdef __cplusplus
}
#endif

#endif  // OLIVE_NODE_API_H
```

### 2.2 关键解耦实现：NodeCacheCallbacks

```cpp
// app/node/node.h（改造后，移除 render/ 头文件包含）

// 前向声明
class NodeCacheCallbacks;

class Node : public QObject {
    // ...
    void SetCacheCallbacks(NodeCacheCallbacks* callbacks);

protected:
    virtual void InvalidateCacheInternal(const TimeRange& range);

private:
    NodeCacheCallbacks* cache_callbacks_ = nullptr;
};

// app/node/nodecachecallbacks.h（新增）
class NodeCacheCallbacks {
public:
    virtual ~NodeCacheCallbacks() = default;
    virtual void InvalidateCache(const QString& cache_id, const TimeRange& range) = 0;
    virtual void InvalidateAllCaches() = 0;
};
```

`RenderManager`（在 `liboliverender.so` 中）实现 `NodeCacheCallbacks`，并在创建节点时注入：

```cpp
class RenderCacheCallbacks : public NodeCacheCallbacks {
    void InvalidateCache(const QString& cache_id, const TimeRange& range) override {
        // 原有 FrameHashCache 的失效逻辑
    }
    // ...
};
```

这样 `Node.h` 不再需要包含 `render/rendercache.h`，编译期依赖被打破。

### 2.3 关键解耦实现：RenderJob 虚函数参数抽象

当前 `Node` 有虚函数：

```cpp
// 改造前
virtual void ProcessShader(TexturePtr destination, const Node* node, const ShaderJob* job);
```

改造后：

```cpp
// app/node/jobtypes.h（新增，只含枚举和基类，无 render/ 依赖）
enum class NodeJobType {
    kShader,
    kGenerate,
    kFootage,
    kColorTransform,
    kSample,
    kCache,
};

struct NodeJobData {
    NodeJobType type;
    void* data;  // 实际数据由 render/ 中的具体类解释
};

// app/node/node.h
virtual void ProcessJob(TexturePtr destination, const NodeJobData& job);
```

`RenderProcessor`（在 `liboliverender.so` 中）调用时：

```cpp
ShaderJob job = ...;
NodeJobData data{NodeJobType::kShader, &job};
node->ProcessJob(destination, data);
```

---

## 3. CMake 改造

```cmake
# app/node/CMakeLists.txt

set(NODE_INTERNAL_SOURCES
  node.cpp node.h
  traverser.cpp traverser.h
  traverserproxy.cpp traverserproxy.h
  nodevalue.cpp nodevalue.h
  # ... 所有 node/ 子目录源文件
)

set(TIMELINE_SOURCES
  ../timeline/timelinecoordinate.cpp ../timeline/timelinecoordinate.h
  ../timeline/timelinemarker.cpp ../timeline/timelinemarker.h
  ../timeline/timelineworkarea.cpp ../timeline/timelineworkarea.h
  ../timeline/undo/*.cpp ../timeline/undo/*.h
)

set(UNDO_SOURCES
  ../undo/undocommand.cpp ../undo/undocommand.h
  ../undo/undostack.cpp ../undo/undostack.h
)

set(CONFIG_SOURCES
  ../config/config.cpp ../config/config.h
)

set(NODE_API_SOURCES
  ${CMAKE_SOURCE_DIR}/c_api/src/node_api.cpp
)

add_library(olivenode SHARED
  ${NODE_INTERNAL_SOURCES}
  ${TIMELINE_SOURCES}
  ${UNDO_SOURCES}
  ${CONFIG_SOURCES}
  ${NODE_API_SOURCES}
)

target_compile_definitions(olivenode PRIVATE OLIVE_BUILDING_NODE)

target_include_directories(olivenode
  PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/c_api/include
  PUBLIC
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(olivenode
  PUBLIC
    olivecore
    olivecodec
    Qt${QT_VERSION_MAJOR}::Core
)

set_target_properties(olivenode PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

install(TARGETS olivenode DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/c_api/include/olive/node_api.h DESTINATION include/olive)
```

---

## 4. 小步快跑实施步骤

### Step 0: 头文件解耦（3–4 天，最关键）

- [ ] 创建 `app/node/nodecachecallbacks.h`，定义 `NodeCacheCallbacks` 接口。
- [ ] 修改 `Node.h`：移除 `render/rendercache.h` 包含，添加 `NodeCacheCallbacks*` 成员和 `SetCacheCallbacks()` 方法。
- [ ] 创建 `app/node/jobtypes.h`，定义 `NodeJobType` 枚举和 `NodeJobData` 结构体。
- [ ] 修改 `Node.h`：将所有 `ProcessXxx` 虚函数合并为 `ProcessJob(TexturePtr, const NodeJobData&)`，或保留原签名但将参数类型改为前向声明。
- [ ] 修改 `RenderProcessor`：适配新的 `NodeCacheCallbacks` 和 `NodeJobData`。

**验收标准**：`app/node/` 目录可以独立编译，不直接或间接包含 `app/render/` 下的任何头文件。

### Step 1: 独立编译 libolivenode.so（1 天）

- [ ] 将 `node/`, `timeline/`, `undo/`, `config/` 的源文件聚合，构建为 `olivenode SHARED`。
- [ ] 处理 `node/` 下的 `add_subdirectory` 嵌套，确保所有源文件被正确收集。

**验收标准**：`libolivenode.so` 编译成功，`nm -D libolivenode.so | grep olive_node` 能看到导出的 C 符号。

### Step 2: 最小 C API（2 天）

- [ ] 先实现项目级接口：
  - `olive_project_create/destroy/load_file/save_file`
  - `olive_node_graph_create/destroy/load_xml/save_xml`
- [ ] 这些接口是渲染子进程最需要的：子进程需要加载 XML 节点图并渲染。

**验收标准**：可以用 C API 创建一个项目、保存为 XML、再加载回来，内容一致。

### Step 3: 节点操作 API（2 天）

- [ ] 实现节点增删查改：`add_node`, `remove_node`, `find_node`, `connect`, `disconnect`。
- [ ] 实现参数读写：`set_param_double`, `get_param_double`, `set_param_rational` 等。

**验收标准**：可以用 C API 构建一个简单的节点图（如 Generator -> ViewerOutput），并序列化为 XML。

### Step 4: Undo API（1 天）

- [ ] 实现 `olive_undo_stack_*` 系列函数。
- [ ] C API 的 undo 采用函数指针回调模式，避免暴露 C++ 的 `UndoCommand` 类。

### Step 5: ViewerOutput 和 Sequence（1 天）

- [ ] 实现 `OliveSequence*` 和 `OliveViewerOutput*` 的 C API。
- [ ] 这是渲染的入口：渲染子进程需要知道哪个 `ViewerOutput` 是输出目标。

---

## 5. 风险与回退

| 风险 | 对策 |
|---|---|
| Node.h 解耦工作量过大，影响面太广 | **分阶段**：第一阶段只做"编译期解耦"（移除 include），不改虚函数签名。若仍然困难，允许 `libolivenode.so` 和 `liboliverender.so` 暂时合并为 `libolive-engine.so`，后续再拆分。 |
| `QObject` 信号槽跨动态库 | Qt 信号槽跨动态库在正确链接 Qt 的情况下工作正常。确保所有含 `Q_OBJECT` 的类在动态库内被 `moc` 处理。 |
| `NodeValueTable` 等模板类难以导出 C 接口 | 不在 C API 中暴露模板类。`NodeTraverser` 的遍历结果（`NodeValueTable`）在 C++ 内部处理，C API 只提供高阶函数如 `olive_node_graph_evaluate_at_time`。 |
| 序列化 XML 格式变更 | C API 中的 `load_xml`/`save_xml` 直接使用现有的 `ProjectSerializer`，XML 格式完全不变，向下兼容。 |
| `Footage` 节点持有 `Decoder` | `Footage` 内部持有 `DecoderPtr`，C API 不暴露 Decoder 细节，只暴露 `Footage` 的文件路径设置/获取。 |
