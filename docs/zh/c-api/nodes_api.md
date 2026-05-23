# oaknodes.so C API 设计

> 具体节点实现。不对外暴露独立函数，而是通过注册机制向 `oakcore.so` 注入节点工厂。
> 未来可被 Rust 实现的 `.so` 替换，只要导出同样的 `oak_nodes_init` 符号即可。

## 一、模块入口点

`oaknodes.so` 被主进程 `dlopen` 后，必须导出以下入口函数：

```c
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 模块初始化入口。
 * @return 0 成功，非 0 失败。
 * @note 该函数内部会调用 oak_node_register_type() 注册所有内置节点类型。
 */
int oak_nodes_init(void);

/**
 * @brief 模块卸载时调用（可选）。
 * @note 注销所有已注册的类型，释放全局资源。
 */
void oak_nodes_shutdown(void);

#ifdef __cplusplus
}
#endif
```

## 二、内部实现约定（C++）

`oaknodes.so` 内部是 C++，继承 `Node` 基类：

```cpp
#include "oak/core_api.h"   // C API 头文件
#include "oak/frame_api.h"  // OakFrame 定义
#include "oak/node_base.h"  // C++ 基类头文件（仅在 oaknodes.so 内部可见）

class BlurNode : public Node {
public:
    BlurNode();
    
    // 参数定义在构造函数中完成
    void Init() override {
        AddInput("Input", kVideoParam);      // 输入端口（期望 RGBA32F + ACEScg）
        AddOutput("Output", kVideoParam);    // 输出端口（保证 RGBA32F + ACEScg）
        AddParam("Radius", kFloatParam, 0.0f); // 参数
    }
    
    // 求值虚函数
    NodeValue Value(const QString& output, const rational& time) override {
        auto input = GetConnectedInput("Input");
        OakFrame frame = input->Value(time);  // 输入帧为 RGBA32F + ACEScg
        float radius = GetParam("Radius").toFloat();
        
        // Blur 在 ACEScg linear 空间下是物理正确的（高斯模糊 = 卷积 = 线性操作）
        return ApplyBlur(frame, radius);  // 输出仍为 RGBA32F + ACEScg
    }
};

// 工厂函数
extern "C" OakNodeHandle oak_create_blur_node(void) {
    return new BlurNode();
}

extern "C" int oak_nodes_init(void) {
    oak_node_register_type("org.oak.Blur", &oak_create_blur_node);
    // ... 注册其他所有节点
    return 0;
}
```

### 2.1 节点色彩空间契约

| 节点类型 | 输入期望 | 输出保证 | 特殊处理 |
|----------|----------|----------|----------|
| `Blur`、`Crop`、`Transform`、`Merge` | RGBA32F + ACEScg | RGBA32F + ACEScg | 无。线性空间下的合成数学正确。 |
| `ColorCorrection`、`OCIOTransform` | RGBA32F + ACEScg | RGBA32F + ACEScg | 直接在 ACEScg 下做颜色校正。 |
| `SolidGenerator`、`TextGenerator` | N/A | RGBA32F + ACEScg | 生成的颜色需按 ACEScg 解释。 |
| `PluginNode`（OFX） | 依插件而定 | 依插件而定 | 若插件不支持 ACEScg，自动做输入/输出转换（见 2.2）。 |
| `Footage`（输入节点） | N/A | RGBA32F + ACEScg | 调用 oakcodec.so，已通过 IDT 转换到 ACEScg。 |
| `Export`（输出节点） | RGBA32F + ACEScg | N/A | 调用 oakcodec.so，通过 ODT 转换到目标空间。 |

### 2.2 PluginNode 色彩空间桥接

当 OFX 插件不支持 ACEScg 时，`PluginNode` 自动做以下桥接：

```
上游节点 (RGBA32F + ACEScg)
    ↓
[Input Bridge]  oakcolor.so: ACEScg → 插件期望空间 (如 sRGB)
                + 格式转换: RGBA32F → RGBA8 (若插件只支持 8-bit)
    ↓
OFX 插件内部工作
    ↓
[Output Bridge] oakcolor.so: 插件空间 → ACEScg
                + 格式转换: RGBA8 → RGBA32F
    ↓
下游节点 (RGBA32F + ACEScg)
```

- **Input Bridge** 由 `PluginNode::PreProcess()` 在调用 `OfxImageEffectActionRender` 前自动执行。
- **Output Bridge** 由 `PluginNode::PostProcess()` 在插件返回后自动执行。
- 桥接对 caller 完全透明，无需用户手动配置。
- 桥接的源/目标色彩空间通过插件的 `kOfxImageEffectPropSupportedComponents` 和 `kOfxImageEffectPropSupportedPixelDepths` 自动推断。

## 三、内置节点类型清单（部分示例）

| 类型 ID | 类别 | 说明 |
|---------|------|------|
| `org.oak.Footage` | 输入 | 媒体文件输入节点 |
| `org.oak.Sequence` | 输入 | 序列（时间线）容器节点 |
| `org.oak.VideoTransform` | 变换 | 缩放、旋转、位移 |
| `org.oak.Crop` | 变换 | 裁切 |
| `org.oak.Blur` | 滤镜 | 高斯模糊 |
| `org.oak.ColorCorrection` | 颜色 | 基础颜色校正 |
| `org.oak.OCIOTransform` | 颜色 | OCIO 色彩空间转换 |
| `org.oak.AudioVolume` | 音频 | 音量控制 |
| `org.oak.AudioPan` | 音频 | 声像控制 |
| `org.oak.Merge` | 合成 | 图层合并（over/add/multiply） |
| `org.oak.TextGenerator` | 生成器 | 文字生成 |
| `org.oak.SolidGenerator` | 生成器 | 纯色生成 |
| `org.oak.PluginNode` | 插件 | OpenFX 插件包装节点 |

## 四、Rust 替换示例

未来可用 Rust 重写 `oaknodes.so`，只要导出同样的 C 入口：

```rust
#[no_mangle]
pub extern "C" fn oak_nodes_init() -> libc::c_int {
    unsafe {
        oak_node_register_type(cstr!("org.oak.Blur"), oak_create_blur_node);
    }
    0
}

#[no_mangle]
pub extern "C" fn oak_create_blur_node() -> *mut OakNode {
    Box::into_raw(Box::new(BlurNode::new())) as *mut OakNode
}
```

> **注意**：Rust 侧需要实现 `OakNode` 的 C++ vtable 布局，或者通过 `oakcore.so` 提供的 C++ 虚函数转发层间接调用。这是后续需要设计的 FFI 细节。
