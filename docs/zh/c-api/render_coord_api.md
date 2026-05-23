# oakengine.so（渲染协调层）C API 设计

> 主进程中的渲染协调：管理渲染器进程池、调度帧范围、聚合结果、与 PreviewAutoCacher / DiskManager 交互。
> 对外暴露的接口供 `oak-editor` 主程序调用。
>
> **全链路约定**：渲染器进程内部及共享内存传输的帧均为 **RGBA32F + ACEScg**。
> 预览窗口显示前必须通过 `oakcolor.so` 做 View Transform（RRT + ODT）。

## 一、类型定义

```c
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakRenderCoordinator* OakCoordHandle;
typedef struct OakRenderJob*         OakRenderJobHandle;
typedef struct OakRenderFrame*       OakRenderFrameHandle;

/**
 * @brief 渲染质量预设。
 */
typedef enum {
    OAK_RENDER_QUALITY_FULL = 0,      // 全分辨率、最高质量
    OAK_RENDER_QUALITY_PREVIEW,       // 半分辨率、快速预览
    OAK_RENDER_QUALITY_PROXY,         // 代理分辨率（1/4 或更低）
} OakRenderQuality;

/**
 * @brief 渲染帧状态。
 */
typedef enum {
    OAK_FRAME_PENDING = 0,    // 等待渲染
    OAK_FRAME_RENDERING,      // 正在渲染
    OAK_FRAME_DONE,           // 已完成
    OAK_FRAME_FAILED,         // 渲染失败
    OAK_FRAME_CANCELLED,      // 已取消
} OakFrameStatus;
```

## 二、协调器生命周期

```c
/**
 * @brief 创建渲染协调器。
 * @param renderer_executable_path oakrenderer 可执行文件的绝对路径。
 * @param max_concurrent_renderers 同时运行的最大渲染器进程数（建议等于 CPU 物理核心数）。
 * @param shm_prefix 共享内存段名称前缀（如 "/oak_render_"），用于避免与其他实例冲突。
 * @return 协调器句柄，NULL 表示失败。
 */
OakCoordHandle oak_coord_create(const char* renderer_executable_path,
                                int max_concurrent_renderers,
                                const char* shm_prefix);

/**
 * @brief 销毁协调器，取消所有进行中的任务，清理共享内存。
 */
void oak_coord_destroy(OakCoordHandle coord);

/**
 * @brief 设置磁盘缓存路径。
 * @param cache_path 缓存目录。渲染中间结果（如 conform 后的音频、预计算的数据）存储于此。
 */
void oak_coord_set_cache_path(OakCoordHandle coord, const char* cache_path);

/**
 * @brief 设置渲染质量。
 */
void oak_coord_set_quality(OakCoordHandle coord, OakRenderQuality quality);
```

## 三、渲染任务提交

```c
/**
 * @brief 提交一个渲染任务。
 * @param coord 协调器句柄。
 * @param graph_json 节点图的 JSON 序列化字符串（由 oakcore.so 的 oak_graph_serialize 生成）。
 * @param start_frame 起始帧（包含）。
 * @param end_frame   结束帧（包含）。
 * @param frame_step  帧步进（通常为 1，慢动作预览可为 2+）。
 * @param video_params 视频参数 JSON（如 '{"width":1920,"height":1080,"fps_num":24,"fps_den":1}'）。
 * @param color_config_path OCIO 配置文件路径（NULL 表示使用默认）。
 * @param out_job_id 输出任务 ID（由调用者分配 int 变量地址）。
 * @return 0 成功，非 0 失败。
 */
int oak_coord_submit_job(OakCoordHandle coord,
                         const char* graph_json,
                         int start_frame, int end_frame, int frame_step,
                         const char* video_params,
                         const char* color_config_path,
                         int* out_job_id);

/**
 * @brief 取消指定任务。
 * @param job_id 任务 ID。
 */
void oak_coord_cancel_job(OakCoordHandle coord, int job_id);

/**
 * @brief 取消所有任务。
 */
void oak_coord_cancel_all(OakCoordHandle coord);
```

## 四、状态轮询与结果获取

```c
/**
 * @brief 轮询任务状态。
 * @param coord 协调器句柄。
 * @param job_id 任务 ID。
 * @param out_completed_frames 输出已完成帧数。
 * @param out_total_frames 输出总帧数。
 * @param out_failed_frames 输出失败帧数。
 * @return 任务总体状态：0 = 进行中，1 = 全部完成，2 = 有失败，3 = 已取消。
 */
int oak_coord_poll_job(OakCoordHandle coord, int job_id,
                       int* out_completed_frames,
                       int* out_total_frames,
                       int* out_failed_frames);

/**
 * @brief 获取指定帧的渲染结果。
 * @param coord 协调器句柄。
 * @param job_id 任务 ID。
 * @param frame_number 帧号。
 * @param out_status 输出帧状态（OAK_FRAME_DONE 表示可用）。
 * @param out_frame  输出帧描述符（浅拷贝，指向共享内存）。
 *                     像素格式固定为 RGBA32F，色彩空间为 ACEScg。
 *                     调用者**不得**修改帧数据，**不得**调用 oak_frame_release。
 *                     该帧的生命周期由协调器管理，必须在 oak_coord_release_frame 前使用。
 * @return 0 成功，非 0 失败（帧尚未渲染或不存在）。
 * @note 获取成功后，调用者必须在合理时间内调用 oak_coord_release_frame，否则共享内存不会被回收。
 */
int oak_coord_get_frame(OakCoordHandle coord, int job_id, int frame_number,
                        int* out_status,
                        OakFrame* out_frame);

/**
 * @brief 释放帧的共享内存引用。
 * @param coord 协调器句柄。
 * @param job_id 任务 ID。
 * @param frame_number 帧号。
 */
void oak_coord_release_frame(OakCoordHandle coord, int job_id, int frame_number);
```

## 五、预览缓存（PreviewAutoCacher 集成）

```c
/**
 * @brief 启动自动预览缓存。
 * @param coord 协调器句柄。
 * @param graph_json 当前节点图。
 * @param playhead_frame 当前播放头位置。
 * @param cache_radius 缓存半径（播放头前后各缓存多少帧）。
 */
void oak_coord_start_preview_cache(OakCoordHandle coord,
                                   const char* graph_json,
                                   int playhead_frame, int cache_radius);

/**
 * @brief 更新播放头位置，触发缓存预加载。
 */
void oak_coord_update_playhead(OakCoordHandle coord, int playhead_frame);

/**
 * @brief 停止预览缓存，释放相关资源。
 */
void oak_coord_stop_preview_cache(OakCoordHandle coord);

/**
 * @brief 查询指定帧是否在预览缓存中。
 * @return true 表示缓存命中，可直接通过 oak_coord_get_frame 获取。
 */
bool oak_coord_preview_cached(OakCoordHandle coord, int frame_number);
```

## 六、磁盘缓存管理（DiskManager 集成）

```c
/**
 * @brief 获取当前磁盘缓存占用大小（字节）。
 */
int64_t oak_coord_cache_size(OakCoordHandle coord);

/**
 * @brief 设置磁盘缓存上限。
 */
void oak_coord_set_cache_limit(OakCoordHandle coord, int64_t limit_bytes);

/**
 * @brief 手动清理过期缓存。
 * @param older_than_seconds 清理超过多少秒未被访问的缓存。传 0 表示全部清理。
 */
void oak_coord_purge_cache(OakCoordHandle coord, int older_than_seconds);
```

## 七、回调注册

```c
/**
 * @brief 渲染进度回调。
 */
typedef void (*OakRenderProgressFn)(int job_id, int completed, int total, void* user_data);

/**
 * @brief 帧完成回调（每完成一帧触发一次）。
 */
typedef void (*OakRenderFrameDoneFn)(int job_id, int frame_number, void* user_data);

/**
 * @brief 任务完成回调。
 */
typedef void (*OakRenderJobDoneFn)(int job_id, bool success, void* user_data);

typedef struct {
    OakRenderProgressFn  on_progress;
    OakRenderFrameDoneFn on_frame_done;
    OakRenderJobDoneFn   on_job_done;
} OakCoordCallbacks;

void oak_coord_set_callbacks(OakCoordHandle coord,
                             const OakCoordCallbacks* callbacks,
                             void* user_data);
```

#ifdef __cplusplus
}
#endif
