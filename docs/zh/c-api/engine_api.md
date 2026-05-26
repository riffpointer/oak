# oakengine.so C API 设计

> 节点图加载与渲染会话适配层。
> 包装现有的 `olive::Project`、`olive::RenderProcessor` 等内部 C++ 类，对外提供稳定的 C 接口。
> 外部模块（包括测试套件）只能通过这些函数与引擎交互。

---

## 一、头文件关系

```c
#include "oak/frame_api.h"   /* OakFrame, OakFramePixelFormat ... */
#include "oak/engine_api.h"  /* 本头文件 */
```

`oak/engine_api.h` 内部会 `#include "oak/frame_api.h"`，因此调用者只需包含 `oak/engine_api.h` 即可获得所有类型。

---

## 二、类型定义

```c
#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakEngineProject* OakEngineProjectHandle;
typedef struct OakEngineSession* OakEngineSessionHandle;
```

---

## 三、项目 / 节点图生命周期

### 3.1 `oak_engine_project_load_xml`

```c
/**
 * @brief 从 XML 字符串加载项目。
 * @param xml_str UTF-8 编码的 XML 项目字符串。
 * @return 项目句柄，失败返回 NULL（如 XML 格式错误、空字符串等）。
 * @note 调用者拥有返回的句柄，必须通过 oak_engine_project_destroy 释放。
 */
OakEngineProjectHandle oak_engine_project_load_xml(const char* xml_str);
```

**加载行为**：
- 合法的 XML 项目字符串会被解析为内部 `olive::Project` 对象。
- 空字符串或畸形 XML 可能返回 NULL（具体取决于内部 XML 解析器的容错策略）。
- 返回的句柄是 `olive::Project*` 的不透明包装。

### 3.2 `oak_engine_project_destroy`

```c
/**
 * @brief 销毁项目并释放所有关联资源。
 * @param proj 项目句柄（传 NULL 则静默忽略）。
 * @note 销毁后句柄失效，禁止再次使用或重复销毁。
 */
void oak_engine_project_destroy(OakEngineProjectHandle proj);
```

### 3.3 `oak_engine_project_node_count`

```c
/**
 * @brief 获取项目中的节点数量。
 * @param proj 项目句柄。
 * @return 节点数量；proj 为 NULL 时返回 0。
 */
int oak_engine_project_node_count(OakEngineProjectHandle proj);
```

---

## 四、渲染会话

### 4.1 `oak_engine_session_create`

```c
/**
 * @brief 为项目创建渲染会话。
 * @param proj          项目句柄。
 * @param width         输出帧宽度（像素）。
 * @param height        输出帧高度（像素）。
 * @param pixel_format  像素格式枚举值（如 PixelFormat::Format 转为 int）。
 *                      默认管线使用 RGBA32F。
 * @param timebase_num  时间基分子。
 * @param timebase_den  时间基分母（必须 > 0）。
 * @return 会话句柄，失败返回 NULL。
 *
 * @note 在无头环境（没有显示/GPU）下，此函数返回 NULL，因为渲染器需要 OpenGL 上下文。
 */
OakEngineSessionHandle oak_engine_session_create(OakEngineProjectHandle proj,
                                                 int width, int height,
                                                 int pixel_format,
                                                 int64_t timebase_num, int64_t timebase_den);
```

**失败原因**：
- `proj` 为 NULL
- `width` 或 `height` 为 0
- `timebase_den` 为 0
- 无头环境下 GPU 初始化失败（`QGuiApplication::instance()` 不存在）

**内部流程**：
1. 验证参数合法性。
2. 在无头环境下提前返回 NULL（避免 OpenGL 初始化崩溃）。
3. 创建 `OpenGLRendererProxy` 并初始化。
4. 设置视频参数（分辨率、像素格式、时间基）。
5. 返回会话句柄。

### 4.2 `oak_engine_session_destroy`

```c
/**
 * @brief 销毁渲染会话。
 * @param session 会话句柄（传 NULL 则静默忽略）。
 */
void oak_engine_session_destroy(OakEngineSessionHandle session);
```

### 4.3 `oak_engine_session_render_frame`

```c
/**
 * @brief 在指定时间点渲染单帧。
 * @param session   会话句柄。
 * @param time_num  目标时间分子（以会话时间基为单位）。
 * @param time_den  目标时间分母。
 * @param out_frame 输出帧描述符。成功时填充帧元数据。
 *                  out_frame->data[0] 指向内部内存，有效期直到下一次 render_frame 调用或 session_destroy。
 * @return 0 成功，非 0 失败。
 *
 * @note 返回的帧内存归会话所有。如果需要跨渲染调用保留数据，调用者必须自行复制。
 */
int oak_engine_session_render_frame(OakEngineSessionHandle session,
                                    int64_t time_num, int64_t time_den,
                                    OakFrame* out_frame);
```

**渲染流程**：
1. 在项目中查找 `ViewerOutput` 节点。
2. 构建渲染图（RenderGraph）。
3. 通过 `OpenGLRendererProxy` 执行 GPU 渲染。
4. 将结果填充到 `OakFrame` 中（通常为 RGBA32F + ACEScg）。
5. 返回 0 表示成功。

---

## 五、与 oakcodec / oakcolor / oakgl 的交互

| 模块 | 交互方式 | 说明 |
|------|---------|------|
| `oakcodec.so` | `dlopen` + C API | 引擎内部通过运行时加载器 `OakCodecRuntime` 调用编解码函数。 |
| `oakcolor.so` | `dlopen` + C API | 引擎内部通过运行时加载器 `OakColorRuntime` 调用色彩管理函数。 |
| `oakgl.so` | `dlopen` + C API | 引擎内部通过 `OakRendererRuntime` 调用渲染函数。`OpenGLRendererProxy` 是引擎侧对 oakgl 的封装。 |

**设计原则**：`oakengine.so` 本身不直接链接 `oakcodec`、`oakcolor` 或 `oakgl`。所有跨模块调用均通过 `dlopen`/`dlsym` 动态解析，确保模块间严格解耦。

---

## 六、使用示例

### 6.1 加载项目并查询节点数

```c
const char* xml = R"xml(<?xml version="1.0"?>
<olive>
  <project>
    <name>Test</name>
    <nodes>
      <node id="viewer" type="ViewerOutput"/>
    </nodes>
  </project>
</olive>)xml";

OakEngineProjectHandle proj = oak_engine_project_load_xml(xml);
if (proj) {
    int n = oak_engine_project_node_count(proj);
    printf("Project has %d nodes\n", n);
    oak_engine_project_destroy(proj);
}
```

### 6.2 创建会话并渲染一帧（需要 GPU）

```c
OakEngineProjectHandle proj = oak_engine_project_load_xml(xml);
if (!proj) return;

OakEngineSessionHandle session = oak_engine_session_create(
    proj, 1920, 1080, 2 /* RGBA32F */, 1, 24);
if (!session) {
    // 无头环境或 GPU 不可用
    oak_engine_project_destroy(proj);
    return;
}

OakFrame frame = {0};
int ret = oak_engine_session_render_frame(session, 0, 1, &frame);
if (ret == 0) {
    printf("Rendered %dx%d frame\n", frame.width, frame.height);
    oak_frame_release(&frame);
}

oak_engine_session_destroy(session);
oak_engine_project_destroy(proj);
```

---

## 七、注意事项

1. **线程安全**：`oak_engine_project_load_xml` 和 `oak_engine_project_destroy` 应在同一线程调用，或在外部做好同步。
2. **GPU 依赖**：`oak_engine_session_create` 依赖 OpenGL 上下文。在无显示服务器的环境中（如 CI、SSH 会话）会返回 NULL。
3. **帧内存所有权**：`render_frame` 返回的帧数据指针属于会话内部缓存，**不要**在 `session_destroy` 之后访问，也**不要**在两次 `render_frame` 之间假设其仍然有效。
4. **色彩空间**：渲染输出的 `OakFrame` 默认标记为 `"ACES - ACEScg"`。若下游需要显示预览，必须通过 `oak_display_transform_apply` 做 View Transform。
