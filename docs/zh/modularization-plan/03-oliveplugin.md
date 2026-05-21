# liboliveplugin.so — OFX 插件宿主支持

> **依赖**：`libolivecore.so`  
> **外部依赖**：`third_party/openfx/HostSupport`（`OfxHost` 静态库），expat，Qt::Core  
> **包含源码**：`app/pluginSupport/`  
> **当前状态**：单体 OBJECT 库的一部分，通过 `target_link_libraries(olive-editor PUBLIC OfxHost)` 隐式链接  
> **改造难度**：⭐⭐（较简单，接口相对独立）

---

## 1. 当前状态分析

`app/pluginSupport/` 实现 OFX（OpenFX）标准的 Host 端接口，使 Olive 能够加载第三方插件（如 Sapphire、Neat Video 等）。

| 组件 | 说明 |
|---|---|
| `OliveHost` | OFX Host 接口主实现 |
| `PluginInstance` | 单个插件实例管理 |
| `OliveClip` / `OliveClipInstance` | OFX Clip 接口封装 |
| `OliveParam` / `OliveParamInstance` | OFX 参数接口封装 |
| `node/plugins/PluginNode` | OFX 插件在节点图中的封装节点 |

**特点**：
- `pluginSupport/` 与 `node/plugins/PluginNode` 存在双向依赖。
- OFX Host 支持库（`third_party/openfx/HostSupport`）是第三方代码，不应修改其接口。
- 插件渲染需要 OpenGL 上下文，因此 `liboliveplugin.so` 需要与渲染层协作。

**决策**：由于 `PluginNode` 继承自 `Node`（在 `libolivenode.so` 中），`PluginNode` 应留在 `libolivenode.so` 中。`liboliveplugin.so` 只包含纯 Host 支持代码（`pluginSupport/`），通过 C API 向 `libolivenode.so` 暴露插件加载和管理能力。

---

## 2. C API 设计

### 2.1 头文件：`c_api/include/olive/plugin_api.h`

```c
#ifndef OLIVE_PLUGIN_API_H
#define OLIVE_PLUGIN_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core_api.h"

#define OLIVE_PLUGIN_API_VERSION 1

#ifdef OLIVE_BUILDING_PLUGIN
#  define OLIVE_PLUGIN_API __attribute__((visibility("default")))
#else
#  define OLIVE_PLUGIN_API
#endif

/* ========== 类型前向声明 ========== */
typedef struct OlivePluginHost     OlivePluginHost;
typedef struct OlivePlugin         OlivePlugin;
typedef struct OlivePluginInstance OlivePluginInstance;
typedef struct OlivePluginParam    OlivePluginParam;

/* ========== API 版本 ========== */
OLIVE_PLUGIN_API int olive_plugin_api_version(void);

/* ========== Host 生命周期 ========== */
OLIVE_PLUGIN_API OlivePluginHost* olive_plugin_host_create(void);
OLIVE_PLUGIN_API void olive_plugin_host_destroy(OlivePluginHost* host);

// 设置插件搜索路径（可多次调用添加多个路径）
OLIVE_PLUGIN_API int olive_plugin_host_add_path(OlivePluginHost* host, const char* path);

// 扫描所有路径，加载可用插件
OLIVE_PLUGIN_API int olive_plugin_host_rescan(OlivePluginHost* host);

// 获取已加载插件数量
OLIVE_PLUGIN_API int olive_plugin_host_plugin_count(OlivePluginHost* host);

// 获取指定索引的插件
OLIVE_PLUGIN_API OlivePlugin* olive_plugin_host_get_plugin(OlivePluginHost* host, int index);

/* ========== Plugin 信息 ========== */
OLIVE_PLUGIN_API const char* olive_plugin_get_id(OlivePlugin* plugin);
OLIVE_PLUGIN_API const char* olive_plugin_get_name(OlivePlugin* plugin);
OLIVE_PLUGIN_API const char* olive_plugin_get_group(OlivePlugin* plugin);  // 分类，如 "Filter/Blur"
OLIVE_PLUGIN_API int olive_plugin_is_hardware_rendering_supported(OlivePlugin* plugin);

/* ========== Plugin Instance ========== */
OLIVE_PLUGIN_API OlivePluginInstance* olive_plugin_instance_create(OlivePlugin* plugin,
                                                                     int width,
                                                                     int height);
OLIVE_PLUGIN_API void olive_plugin_instance_destroy(OlivePluginInstance* instance);

// 参数操作（通过字符串名称）
OLIVE_PLUGIN_API int olive_plugin_instance_set_param_int(OlivePluginInstance* instance,
                                                          const char* param_name,
                                                          int value);
OLIVE_PLUGIN_API int olive_plugin_instance_set_param_double(OlivePluginInstance* instance,
                                                             const char* param_name,
                                                             double value);
OLIVE_PLUGIN_API int olive_plugin_instance_set_param_string(OlivePluginInstance* instance,
                                                             const char* param_name,
                                                             const char* value);

// 渲染一帧（输入/输出均为 Frame）
OLIVE_PLUGIN_API int olive_plugin_instance_render(OlivePluginInstance* instance,
                                                   OliveRational time,
                                                   OliveFrame* input_frame,
                                                   OliveFrame** output_frame);

/* ========== Param 枚举（用于 UI 构建控件） ========== */
OLIVE_PLUGIN_API int olive_plugin_instance_param_count(OlivePluginInstance* instance);
OLIVE_PLUGIN_API OlivePluginParam* olive_plugin_instance_get_param(OlivePluginInstance* instance,
                                                                    int index);

OLIVE_PLUGIN_API const char* olive_plugin_param_get_name(OlivePluginParam* param);
OLIVE_PLUGIN_API const char* olive_plugin_param_get_label(OlivePluginParam* param);
OLIVE_PLUGIN_API int olive_plugin_param_get_type(OlivePluginParam* param);  // 0=int, 1=double, 2=string, 3=bool, 4=color, 5=choice
OLIVE_PLUGIN_API int olive_plugin_param_get_int_min(OlivePluginParam* param);
OLIVE_PLUGIN_API int olive_plugin_param_get_int_max(OlivePluginParam* param);
OLIVE_PLUGIN_API double olive_plugin_param_get_double_min(OlivePluginParam* param);
OLIVE_PLUGIN_API double olive_plugin_param_get_double_max(OlivePluginParam* param);
OLIVE_PLUGIN_API int olive_plugin_param_get_choice_count(OlivePluginParam* param);
OLIVE_PLUGIN_API const char* olive_plugin_param_get_choice_label(OlivePluginParam* param, int index);

#ifdef __cplusplus
}
#endif

#endif  // OLIVE_PLUGIN_API_H
```

### 2.2 实现要点

```cpp
// c_api/src/plugin_api.cpp

#include "olive/plugin_api.h"
#include "pluginSupport/olivehost.h"
#include "pluginSupport/plugininstance.h"
#include "pluginSupport/oliveparam.h"
#include "codec/frame.h"

struct OlivePluginHost {
    olive::OliveHost* impl;
};

struct OlivePlugin {
    olive::Plugin* impl;  // 内部插件描述对象
};

struct OlivePluginInstance {
    olive::PluginInstance* impl;
};

// ...

extern "C" {

OlivePluginHost* olive_plugin_host_create(void) {
    try {
        auto* h = new OlivePluginHost();
        h->impl = new olive::OliveHost();
        return h;
    } catch (...) {
        return nullptr;
    }
}

void olive_plugin_host_destroy(OlivePluginHost* host) {
    if (host) {
        delete host->impl;
        delete host;
    }
}

int olive_plugin_host_add_path(OlivePluginHost* host, const char* path) {
    if (!host || !path) return OLIVE_ERROR_INVALID;
    try {
        host->impl->AddPath(QString::fromUtf8(path));
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

int olive_plugin_host_rescan(OlivePluginHost* host) {
    if (!host) return OLIVE_ERROR_INVALID;
    try {
        host->impl->RescanPlugins();
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

// ... 其他封装类似 ...

}  // extern "C"
```

---

## 3. CMake 改造

### 3.1 `app/pluginSupport/CMakeLists.txt`

```cmake
set(PLUGIN_INTERNAL_SOURCES
  olivehost.cpp olivehost.h
  plugininstance.cpp plugininstance.h
  oliveclip.cpp oliveclip.h
  oliveclipinstance.cpp oliveclipinstance.h
  oliveparam.cpp oliveparam.h
  oliveparaminstance.cpp oliveparaminstance.h
  # ...
)

set(PLUGIN_API_SOURCES
  ${CMAKE_SOURCE_DIR}/c_api/src/plugin_api.cpp
)

add_library(oliveplugin SHARED
  ${PLUGIN_INTERNAL_SOURCES}
  ${PLUGIN_API_SOURCES}
)

target_compile_definitions(oliveplugin PRIVATE OLIVE_BUILDING_PLUGIN)

target_include_directories(oliveplugin
  PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/third_party/openfx/include
    ${CMAKE_SOURCE_DIR}/third_party/openfx/HostSupport/include
    ${CMAKE_SOURCE_DIR}/c_api/include
  PUBLIC
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(oliveplugin
  PUBLIC
    olivecore
    OfxHost          # third_party/openfx/HostSupport 构建的目标
    EXPAT::EXPAT
    Qt${QT_VERSION_MAJOR}::Core
)

set_target_properties(oliveplugin PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

install(TARGETS oliveplugin DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/c_api/include/olive/plugin_api.h DESTINATION include/olive)
```

---

## 4. 小步快跑实施步骤

### Step 0: 隔离 PluginNode（1 天）

- [ ] 将 `app/node/plugins/PluginNode` 移动到 `app/node/` 下（或保持原位，但确保它编译进 `libolivenode.so` 而非 `liboliveplugin.so`）。
- [ ] 确认 `PluginNode` 对 `pluginSupport/` 的依赖方向：PluginNode 使用 pluginSupport 的类，而非相反。

**验收标准**：`liboliveplugin.so` 编译时不包含任何 `node/` 下的源文件。

### Step 1: 构建 liboliveplugin.so（1 天）

- [ ] 创建 `app/pluginSupport/CMakeLists.txt`（若尚无）。
- [ ] 将 `pluginSupport/` 的源文件从主 OBJECT 库中移出，单独构建为 `oliveplugin SHARED`。
- [ ] 确保 `OfxHost` 静态库先被构建（`third_party/openfx/HostSupport`）。

**验收标准**：`liboliveplugin.so` 编译成功，能通过 `dlsym` 找到 `olive_plugin_api_version`。

### Step 2: 最小 C API（2 天）

- [ ] 先实现最必需的接口：
  - `olive_plugin_host_create/destroy/add_path/rescan`
  - `olive_plugin_host_plugin_count/get_plugin`
  - `olive_plugin_get_id/name`
- [ ] 暂不实现：渲染接口（`olive_plugin_instance_render`）、参数枚举。

**验收标准**：主进程可以扫描 OFX 插件目录并列出所有插件名称。

### Step 3: 扩展渲染接口（2 天）

- [ ] 实现 `olive_plugin_instance_create/destroy`。
- [ ] 实现 `olive_plugin_instance_render`（输入输出 `OliveFrame*`）。
- [ ] 此步骤需要 `libolivecodec.so` 的 `OliveFrame` 定义已就绪。

**验收标准**：可以创建一个 OFX 插件实例，传入一帧，获取处理后的一帧。

### Step 4: 参数枚举（2 天）

- [ ] 实现参数枚举接口，使 UI 层可以通过 C API 自动构建参数控件。
- [ ] 编写测试：加载一个已知插件（如 OFX 示例插件），验证参数数量与类型正确。

---

## 5. 风险与回退

| 风险 | 对策 |
|---|---|
| `OfxHost` 静态库中的符号与动态库导出冲突 | `OfxHost` 保持静态链接进 `liboliveplugin.so`，其符号不对外导出（`hidden` 可见性）。 |
| OFX 插件需要 OpenGL 上下文 | 渲染接口 `olive_plugin_instance_render` 需要传入或绑定 GL 上下文。在"用完即弃"的渲染子进程模型中，这天然解决：子进程自己创建 GL 上下文，插件在其上渲染。 |
| `PluginNode` 需要 `PluginInstance` 的 C++ 类 | `PluginNode` 在 `libolivenode.so` 内部，可以直接包含 `pluginSupport/` 的 C++ 头文件（因为 node 库可以在编译时访问 pluginSupport 源码）。只有跨库边界才需要 C API。 |
| OFX 插件多实例状态管理复杂 | C API 中每个 `OlivePluginInstance*` 对应一个独立的 OFX 实例句柄，状态完全隔离。 |
