/*
 *  oaknodes.so C API
 *  节点类型注册入口。 oaknodes.so 被 dlopen 后必须导出 oak_nodes_init / oak_nodes_shutdown。
 */

#ifndef OAK_NODES_API_H
#define OAK_NODES_API_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 模块初始化入口。
 * @return 0 成功，非 0 失败。
 * @note 内部会调用 oak_node_register_type() 注册所有内置节点类型。
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

#endif /* OAK_NODES_API_H */
