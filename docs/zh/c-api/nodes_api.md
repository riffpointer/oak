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
#include "oak/node_base.h"  // C++ 基类头文件（仅在 oaknodes.so 内部可见）

class BlurNode : public Node {
public:
    BlurNode();
    
    // 参数定义在构造函数中完成
    void Init() override {
        AddInput("Input", kVideoParam);      // 输入端口
        AddOutput("Output", kVideoParam);    // 输出端口
        AddParam("Radius", kFloatParam, 0.0f); // 参数
    }
    
    // 求值虚函数
    NodeValue Value(const QString& output, const rational& time) override {
        auto input = GetConnectedInput("Input");
        auto frame = input->Value(time);
        float radius = GetParam("Radius").toFloat();
        return ApplyBlur(frame, radius);
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
