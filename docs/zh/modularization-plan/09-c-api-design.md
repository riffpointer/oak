# C API 设计规范总纲

> **必读**：本文件定义了所有 Olive/Oak 动态库的 C API 设计约定。`01-` 到 `07-` 各模块的 API 均遵循此规范。

---

## 1. 设计原则

### 1.1 不透明指针（Opaque Pointer）

所有 C++ 对象在 C 接口中均隐藏实现，仅暴露为 `struct` 的前向声明：

```c
// 公共头文件（.h）中
typedef struct OliveNodeGraph OliveNodeGraph;  // 只有声明，无定义

// 实现文件（.cpp）中
struct OliveNodeGraph {
    olive::NodeGraph* impl;  // 实际的 C++ 对象
};
```

外部代码只能操作指针，无法解引用或 sizeof。

### 1.2 纯 C 接口

- 函数名使用 `snake_case`，前缀为 `olive_<module>_`。
- 参数和返回值仅使用 C 基础类型、结构体、不透明指针。
- 禁止使用 C++ 特性：类、引用、重载、模板、异常、`std::string`、`QString`。
- 字符串使用 `const char*`（UTF-8 编码）。
- 布尔值使用 `int`（0 = false，非 0 = true）。

### 1.3 动态库自身可以用 C++

动态库的实现文件（`.cpp`）内部可以继续使用：
- Qt（`QObject`, `QString`, `QList`, 信号槽等）
- C++ STL
- 虚函数、模板、Lambda
- 异常（但不得穿透 C 接口边界）

C 接口层只是薄薄的封装胶合层。

---

## 2. 命名规范

| 元素 | 规范 | 示例 |
|---|---|---|
| 类型名 | `Olive` + `PascalCase` | `OliveNodeGraph`, `OliveFrame` |
| 函数名 | `olive_<module>_<snake_case>` | `olive_node_graph_create`, `olive_codec_decoder_open` |
| 枚举名 | `Olive<Module><PascalCase>` | `OliveCodecResultOk`, `OliveRenderModeOffline` |
| 常量宏 | `OLIVE_<MODULE>_UPPER_SNAKE` | `OLIVE_NODE_OK`, `OLIVE_CODEC_ERROR_NOT_FOUND` |
| 版本宏 | `OLIVE_<MODULE>_API_VERSION` | `OLIVE_NODE_API_VERSION 1` |

---

## 3. 内存管理约定

### 3.1 谁创建，谁释放

- **库创建的对象**，必须由库的对应 `destroy`/`free` 函数释放。
- **主进程分配并传入的缓冲区**（如 `char*` 参数），由主进程管理，库内部只读或复制。
- **库返回的字符串/缓冲区**，必须使用库提供的 `free` 函数释放，不能用 C 标准 `free()`（因为库的堆和主进程的堆可能是分离的，尤其是在 Windows 上）。

```c
// 正确：库分配，库释放
char* xml = olive_node_graph_save_xml(graph, &len);
// ... 使用 xml ...
olive_core_free(xml);  // 使用库提供的释放函数

// 错误：
free(xml);  // 危险！堆可能不一致
```

### 3.2 通用释放函数

每个模块提供一个通用释放函数：

```c
void olive_core_free(void* ptr);   // 释放字符串/二进制缓冲区
void olive_core_mem_free(void* ptr, size_t size);  // 带大小的释放（用于安全擦除）
```

### 3.3 对象生命周期模式

```c
// 模式 A：Create/Destroy（堆分配）
OliveNodeGraph* olive_node_graph_create(void);
void olive_node_graph_destroy(OliveNodeGraph* obj);

// 模式 B：Init/Cleanup（栈分配或外部缓冲区）
int olive_frame_init(OliveFrame* frame, int w, int h, OlivePixelFormat fmt);
void olive_frame_cleanup(OliveFrame* frame);

// 模式 C：Ref/Unref（引用计数）
void olive_frame_ref(OliveFrame* frame);
void olive_frame_unref(OliveFrame* frame);
```

优先使用 **模式 A（Create/Destroy）**，因为不透明指针天然适合堆分配。

---

## 4. 错误处理

### 4.1 返回码约定

所有可能失败的函数返回 `int`：

```c
#define OLIVE_OK                0   // 成功
#define OLIVE_ERROR_GENERIC    -1   // 通用错误
#define OLIVE_ERROR_INVALID    -2   // 无效参数
#define OLIVE_ERROR_NOMEM      -3   // 内存不足
#define OLIVE_ERROR_NOT_FOUND  -4   // 找不到对象/文件
#define OLIVE_ERROR_IO         -5   // IO 错误
#define OLIVE_ERROR_CANCELLED  -6   // 操作被取消
#define OLIVE_ERROR_UNSUPPORTED -7  // 不支持的操作
```

### 4.2 详细错误信息

提供线程局部的错误信息获取函数：

```c
int olive_core_last_error_code(void);
const char* olive_core_last_error_string(void);  // 线程安全，返回静态缓冲区或 TLS
```

实现方式：

```cpp
// .cpp 中
thread_local int g_last_error_code = OLIVE_OK;
thread_local char g_last_error_string[1024];

static void SetError(int code, const char* fmt, ...) {
    g_last_error_code = code;
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error_string, sizeof(g_last_error_string), fmt, args);
    va_end(args);
}
```

---

## 5. 字符串处理

### 5.1 输入字符串

- 所有 `const char*` 参数均视为 **UTF-8 编码**。
- 库内部在边界处转换为 `QString`：

```cpp
// 封装层内部
QString qstr = QString::fromUtf8(cstr);
```

### 5.2 输出字符串

- 返回 `char*` 的函数，使用 `olive_core_free()` 释放。
- 如果只需读取而不持有，提供 `const char*` 返回版本：

```c
const char* olive_node_get_type_name(OliveNode* node);  // 生命周期与 node 绑定
char* olive_node_graph_save_xml(OliveNodeGraph* graph, size_t* out_len);  // 需释放
```

---

## 6. 显式加载器（ModuleLoader）

### 6.1 设计目标

主进程通过一个统一的 `ModuleLoader` 类显式加载所有动态库，将 `dlopen`/`dlsym` 的细节隐藏。

### 6.2 C++ 封装类

```cpp
// app/moduleloader.h
#pragma once

#include <QString>
#include <QHash>
#include <functional>

namespace olive {

class ModuleLoader {
public:
    ModuleLoader();
    ~ModuleLoader();

    // 加载指定路径的动态库
    bool Load(const QString& module_name, const QString& library_path);

    // 卸载
    void Unload(const QString& module_name);

    // 获取函数指针（模板封装，内部调用 dlsym）
    template<typename FuncPtr>
    FuncPtr GetFunction(const QString& module_name, const char* func_name) {
        return reinterpret_cast<FuncPtr>(GetFunctionRaw(module_name, func_name));
    }

    // 检查是否已加载
    bool IsLoaded(const QString& module_name) const;

    // 获取加载错误信息
    QString LastError() const;

private:
    void* GetFunctionRaw(const QString& module_name, const char* func_name);

    struct ModuleHandle {
        void* handle;  // dlopen handle
        QString path;
    };
    QHash<QString, ModuleHandle> modules_;
    QString last_error_;
};

// 便捷宏：从指定模块获取函数并调用
#define OLIVE_LOAD_FUNC(loader, module, name, type) \
    auto name = (loader).GetFunction<type>(module, #name); \
    if (!name) { qFatal("Failed to load function: " #name " from module: " #module); }

}  // namespace olive
```

### 6.3 实现（POSIX）

```cpp
// app/moduleloader.cpp
#include "moduleloader.h"
#include <dlfcn.h>
#include <QDebug>

namespace olive {

bool ModuleLoader::Load(const QString& module_name, const QString& library_path) {
    if (modules_.contains(module_name)) return true;

    void* handle = dlopen(library_path.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        last_error_ = QString::fromUtf8(dlerror());
        qWarning() << "Failed to load" << library_path << ":" << last_error_;
        return false;
    }

    modules_.insert(module_name, {handle, library_path});
    qInfo() << "Loaded module:" << module_name << "from" << library_path;
    return true;
}

void ModuleLoader::Unload(const QString& module_name) {
    auto it = modules_.find(module_name);
    if (it != modules_.end()) {
        dlclose(it->handle);
        modules_.erase(it);
    }
}

void* ModuleLoader::GetFunctionRaw(const QString& module_name, const char* func_name) {
    auto it = modules_.find(module_name);
    if (it == modules_.end()) return nullptr;
    dlerror();  // 清除之前的错误
    void* func = dlsym(it->handle, func_name);
    return func;
}

}  // namespace olive
```

### 6.4 实现（Windows）

```cpp
#include <windows.h>

bool ModuleLoader::Load(const QString& module_name, const QString& library_path) {
    HMODULE handle = LoadLibraryW(library_path.toStdWString().c_str());
    if (!handle) {
        last_error_ = QString::number(GetLastError());
        return false;
    }
    modules_.insert(module_name, {handle, library_path});
    return true;
}

void* ModuleLoader::GetFunctionRaw(const QString& module_name, const char* func_name) {
    auto it = modules_.find(module_name);
    if (it == modules_.end()) return nullptr;
    return GetProcAddress(static_cast<HMODULE>(it->handle), func_name);
}
```

### 6.5 使用示例

```cpp
// core.cpp 中初始化
module_loader_ = new ModuleLoader();
module_loader_->Load("core", FindLibraryPath("libolivecore.so"));
module_loader_->Load("codec", FindLibraryPath("libolivecodec.so"));
module_loader_->Load("node", FindLibraryPath("libolivenode.so"));

// 获取函数
OLIVE_LOAD_FUNC(*module_loader_, "core", olive_rational_make, OliveRational(*)(int64_t, int64_t));

OliveRational r = olive_rational_make(1001, 30000);
```

---

## 7. 类型映射表

| C++ 类型（内部） | C 接口类型（公共） | 说明 |
|---|---|---|
| `olive::Rational` | `OliveRational` | `struct { int64_t num, den; }` |
| `olive::Color` | `OliveColor` | `struct { double r, g, b, a; }` |
| `olive::TimeRange` | `OliveTimeRange*` | 不透明指针 |
| `olive::Frame` | `OliveFrame*` | 不透明指针 |
| `olive::SampleBuffer` | `OliveSampleBuffer*` | 不透明指针 |
| `olive::PixelFormat` | `OlivePixelFormat` | `enum` |
| `olive::VideoParams` | `OliveVideoParams` | 公开结构体（POD） |
| `olive::AudioParams` | `OliveAudioParams` | 公开结构体（POD） |
| `olive::Node*` | `OliveNode*` | 不透明指针 |
| `olive::NodeGraph*` | `OliveNodeGraph*` | 不透明指针 |
| `olive::RenderTicketPtr` | `OliveRenderTicket*` | 不透明指针（引用计数内部管理） |
| `QString` | `const char*` | UTF-8 编码 |
| `QSize` | `struct { int width; int height; }` | `OliveSize` |
| `QMatrix4x4` | `float[16]` | 列优先 |

### 7.1 POD 结构体定义示例

```c
// olivecore_api.h

typedef struct {
    int64_t num;
    int64_t den;
} OliveRational;

typedef struct {
    double r;
    double g;
    double b;
    double a;
} OliveColor;

typedef struct {
    int width;
    int height;
    int depth;
    int channel_count;
    OlivePixelFormat format;
    double pixel_aspect_num;
    double pixel_aspect_den;
} OliveVideoParams;

typedef struct {
    int sample_rate;
    int64_t channel_layout;  // FFmpeg AV_CH_LAYOUT_* 值
    OliveSampleFormat format;
} OliveAudioParams;

typedef struct {
    int width;
    int height;
} OliveSize;
```

---

## 8. 线程安全

### 8.1 API 层面

- **默认不保证线程安全**。除非文档明确标注 `thread-safe`，否则每个 `OliveXxx*` 对象只能在创建它的线程中使用。
- 这是刻意的设计：由于渲染进程是"用完即弃"的，不存在多线程共享渲染状态的问题。

### 8.2 主进程中的线程使用

- `ModuleLoader` 本身是线程安全的（只读查找，加载/卸载在初始化/退出时串行执行）。
- UI 对象在主线程操作。
- IO/解码可以在工作线程中通过 C API 操作独立的 `OliveDecoder*` 实例。

---

## 9. 版本与 ABI 兼容性

### 9.1 API 版本号

每个模块的 C API 有一个主版本号：

```c
#define OLIVE_NODE_API_VERSION 1

int olive_node_api_version(void);  // 返回 OLIVE_NODE_API_VERSION
```

### 9.2 加载时版本检查

```cpp
bool LoadNodeModule(ModuleLoader* loader, const QString& path) {
    if (!loader->Load("node", path)) return false;
    auto version_fn = loader->GetFunction<int(*)()>("node", "olive_node_api_version");
    if (!version_fn || version_fn() != EXPECTED_NODE_API_VERSION) {
        qFatal("Incompatible libolivenode.so version");
        return false;
    }
    return true;
}
```

### 9.3 ABI 兼容性规则

- **允许**：新增函数、新增枚举值（在末尾）、新增结构体字段（在末尾，且文档标注"v2 起可用"）。
- **不允许**：删除函数、修改函数签名、修改已有字段含义、改变枚举值顺序。
- **结构体扩展**：POD 结构体新增字段时，提供初始化宏确保旧代码不会未初始化新字段：

```c
#define OLIVE_VIDEO_PARAMS_DEFAULT { \
    .width = 1920, .height = 1080, .depth = 1, \
    .channel_count = 4, .format = OLIVE_PIXEL_FMT_RGBA32F, \
    .pixel_aspect_num = 1.0, .pixel_aspect_den = 1.0 \
}
```

---

## 10. 头文件组织

### 10.1 目录结构

```
c_api/
├── include/
│   ├── olive/               # 公共 C API 头文件（安装时发布）
│   │   ├── core_api.h
│   │   ├── codec_api.h
│   │   ├── node_api.h
│   │   ├── render_api.h
│   │   ├── audio_api.h
│   │   ├── plugin_api.h
│   │   ├── ui_api.h
│   │   └── olive_api.h      # 总入口，包含所有模块
│   └── olivecpp/            # 主进程内部使用的 C++ 辅助封装
│       ├── module_loader.h
│       ├── core_wrapper.h   // RAII 包装类
│       ├── node_wrapper.h
│       └── ...
└── src/
    ├── core_api.cpp         // 对应各模块的 C 封装实现
    ├── codec_api.cpp
    ├── node_api.cpp
    ├── render_api.cpp
    ├── audio_api.cpp
    ├── plugin_api.cpp
    └── ui_api.cpp
```

### 10.2 C API 头文件示例

```c
// c_api/include/olive/node_api.h
#ifndef OLIVE_NODE_API_H
#define OLIVE_NODE_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core_api.h"

#define OLIVE_NODE_API_VERSION 1

/* ========== 类型前向声明 ========== */
typedef struct OliveNodeGraph OliveNodeGraph;
typedef struct OliveNode      OliveNode;
typedef struct OliveParam     OliveParam;

/* ========== 函数导出宏 ========== */
#ifdef OLIVE_BUILDING_NODE
#  define OLIVE_NODE_API __attribute__((visibility("default")))
#else
#  define OLIVE_NODE_API
#endif

/* ========== API 版本 ========== */
OLIVE_NODE_API int olive_node_api_version(void);

/* ========== NodeGraph ========== */
OLIVE_NODE_API OliveNodeGraph* olive_node_graph_create(void);
OLIVE_NODE_API void olive_node_graph_destroy(OliveNodeGraph* g);

OLIVE_NODE_API int olive_node_graph_load_xml(OliveNodeGraph* g,
                                              const char* xml_data,
                                              size_t xml_len);
OLIVE_NODE_API char* olive_node_graph_save_xml(OliveNodeGraph* g,
                                                size_t* out_len);

OLIVE_NODE_API OliveNode* olive_node_graph_find_node(OliveNodeGraph* g,
                                                      const char* node_id);
OLIVE_NODE_API int olive_node_graph_add_node(OliveNodeGraph* g,
                                              const char* node_type,
                                              const char* node_id);

/* ========== Node ========== */
OLIVE_NODE_API const char* olive_node_get_id(OliveNode* node);
OLIVE_NODE_API const char* olive_node_get_type_name(OliveNode* node);

OLIVE_NODE_API int olive_node_connect(OliveNode* from_node,
                                       int from_output_index,
                                       OliveNode* to_node,
                                       int to_input_index);

/* ========== Param ========== */
OLIVE_NODE_API int olive_node_set_param_int(OliveNode* node,
                                             const char* param_name,
                                             int64_t value);
OLIVE_NODE_API int olive_node_set_param_double(OliveNode* node,
                                                const char* param_name,
                                                double value);
OLIVE_NODE_API int olive_node_set_param_rational(OliveNode* node,
                                                  const char* param_name,
                                                  OliveRational value);
OLIVE_NODE_API int olive_node_set_param_string(OliveNode* node,
                                                const char* param_name,
                                                const char* value);

/* ========== Project ========== */
OLIVE_NODE_API OliveNodeGraph* olive_project_create(const char* name);
OLIVE_NODE_API int olive_project_load_file(OliveNodeGraph* project,
                                            const char* filename);
OLIVE_NODE_API int olive_project_save_file(OliveNodeGraph* project,
                                            const char* filename);

#ifdef __cplusplus
}
#endif

#endif  // OLIVE_NODE_API_H
```

---

## 11. CMake 中的 C API 编译

### 11.1 为模块添加 C API 目标

```cmake
# app/node/CMakeLists.txt

# 原有 C++ 源码（内部实现，不暴露头文件）
set(NODE_INTERNAL_SOURCES
  node.cpp node.h
  traverser.cpp traverser.h
  project/project.cpp project/project.h
  # ... 其他内部文件
)

# C API 封装层源码
set(NODE_API_SOURCES
  ${CMAKE_SOURCE_DIR}/c_api/src/node_api.cpp
)

# 模块对外头文件（安装时发布）
set(NODE_API_HEADERS
  ${CMAKE_SOURCE_DIR}/c_api/include/olive/node_api.h
)

# 创建动态库
add_library(olivenode SHARED
  ${NODE_INTERNAL_SOURCES}
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
  PRIVATE
    olivecore
    olivecodec
    Qt${QT_VERSION_MAJOR}::Core
)

# 设置符号可见性：默认隐藏，只有标记 OLIVE_NODE_API 的才导出
set_target_properties(olivenode PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

# 安装 C API 头文件
install(FILES ${NODE_API_HEADERS} DESTINATION include/olive)
install(TARGETS olivenode DESTINATION lib)
```

### 11.2 主可执行文件不链接业务库

```cmake
# app/CMakeLists.txt（改造后）

add_executable(olive-editor
  main.cpp
  core.cpp
  core.h
  ${CMAKE_SOURCE_DIR}/c_api/src/module_loader.cpp  # 显式加载器实现
)

# 主程序只链接 Qt 和系统库，不链接 olivecore/olivecodec 等业务库！
target_link_libraries(olive-editor PRIVATE
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Widgets
  # ... 其他 UI 依赖
)

target_include_directories(olive-editor PRIVATE
  ${CMAKE_SOURCE_DIR}/c_api/include
)
```

---

## 12. 测试策略

### 12.1 C API 单元测试

为每个 C API 函数编写独立测试：

```cpp
// tests/c_api/test_node_api.cpp
#include <gtest/gtest.h>
#include "olive/node_api.h"

TEST(NodeAPITest, CreateDestroy) {
    OliveNodeGraph* g = olive_node_graph_create();
    ASSERT_NE(g, nullptr);
    olive_node_graph_destroy(g);
}

TEST(NodeAPITest, AddNodeAndParam) {
    OliveNodeGraph* g = olive_node_graph_create();
    ASSERT_EQ(OLIVE_OK, olive_node_graph_add_node(g, "Transform", "T1"));
    OliveNode* n = olive_node_graph_find_node(g, "T1");
    ASSERT_NE(n, nullptr);
    ASSERT_EQ(OLIVE_OK, olive_node_set_param_double(n, "position_x", 100.0));
    olive_node_graph_destroy(g);
}
```

### 12.2 ABI 稳定性测试

在 CI 中：
1. 编译当前版本的动态库。
2. 用上一个发布版本的测试可执行文件加载当前动态库运行。
3. 验证所有测试通过（确保未破坏 ABI）。

---

## 13. 常见陷阱

| 陷阱 | 说明 | 对策 |
|---|---|---|
| **异常穿透 C 边界** | C++ 异常抛出到 C 调用方是 UB。 | 所有 C API 函数用 `try/catch(...)` 包裹，捕获所有异常并转换为错误码。 |
| **RTTI 跨边界** | `dynamic_cast` 在不同动态库间可能失败。 | C 接口不使用 RTTI，内部若必须 `dynamic_cast`，确保类型定义在同一个库内。 |
| **Qt 元对象跨库** | `qobject_cast` 依赖 moc 生成的静态元对象数据，跨库时可能失效。 | 不在 C API 中暴露 Qt 对象，所有 Qt 对象封装在库内部。 |
| **全局静态变量** | 多个动态库各有一份全局静态变量。 | 避免在 C API 头文件中定义全局静态变量，使用函数内 static + 首次调用初始化。 |
| **堆不一致（Windows）** | A 库 `malloc`，B 库 `free` 导致崩溃。 | 严格遵循"谁分配谁释放"，使用库提供的 `olive_core_free()`。 |
