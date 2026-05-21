/*
 *  oakengine.so（渲染协调层）C API
 *  主进程中的渲染协调：管理渲染器进程池、调度帧范围、聚合结果。
 */

#ifndef OAK_COORD_API_H
#define OAK_COORD_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Opaque handles                                                      */
/* ------------------------------------------------------------------ */

typedef struct OakRenderCoordinator* OakCoordHandle;
typedef struct OakRenderJob*         OakRenderJobHandle;
typedef struct OakRenderFrame*       OakRenderFrameHandle;

/* ------------------------------------------------------------------ */
/*  Enums                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_RENDER_QUALITY_FULL = 0,      /* 全分辨率、最高质量 */
    OAK_RENDER_QUALITY_PREVIEW,       /* 半分辨率、快速预览 */
    OAK_RENDER_QUALITY_PROXY,         /* 代理分辨率（1/4 或更低） */
} OakRenderQuality;

typedef enum {
    OAK_FRAME_PENDING = 0,    /* 等待渲染 */
    OAK_FRAME_RENDERING,      /* 正在渲染 */
    OAK_FRAME_DONE,           /* 已完成 */
    OAK_FRAME_FAILED,         /* 渲染失败 */
    OAK_FRAME_CANCELLED,      /* 已取消 */
} OakFrameStatus;

/* ------------------------------------------------------------------ */
/*  Coordinator lifecycle                                               */
/* ------------------------------------------------------------------ */

OakCoordHandle oak_coord_create(const char* renderer_executable_path,
                                int max_concurrent_renderers,
                                const char* shm_prefix);
void           oak_coord_destroy(OakCoordHandle coord);

void           oak_coord_set_cache_path(OakCoordHandle coord, const char* cache_path);
void           oak_coord_set_quality(OakCoordHandle coord, OakRenderQuality quality);

/* ------------------------------------------------------------------ */
/*  Job submission                                                      */
/* ------------------------------------------------------------------ */

int  oak_coord_submit_job(OakCoordHandle coord,
                          const char* graph_json,
                          int start_frame, int end_frame, int frame_step,
                          const char* video_params,
                          const char* color_config_path,
                          int* out_job_id);

void oak_coord_cancel_job(OakCoordHandle coord, int job_id);
void oak_coord_cancel_all(OakCoordHandle coord);

/* ------------------------------------------------------------------ */
/*  Polling & result retrieval                                          */
/* ------------------------------------------------------------------ */

int  oak_coord_poll_job(OakCoordHandle coord, int job_id,
                        int* out_completed_frames,
                        int* out_total_frames,
                        int* out_failed_frames);

int  oak_coord_get_frame(OakCoordHandle coord, int job_id, int frame_number,
                         int* out_status,
                         void** out_data,
                         int* out_width, int* out_height, int* out_pix_fmt,
                         int* out_stride);

void oak_coord_release_frame(OakCoordHandle coord, int job_id, int frame_number);

/* ------------------------------------------------------------------ */
/*  Preview cache (PreviewAutoCacher integration)                       */
/* ------------------------------------------------------------------ */

void oak_coord_start_preview_cache(OakCoordHandle coord,
                                   const char* graph_json,
                                   int playhead_frame, int cache_radius);

void oak_coord_update_playhead(OakCoordHandle coord, int playhead_frame);
void oak_coord_stop_preview_cache(OakCoordHandle coord);
bool oak_coord_preview_cached(OakCoordHandle coord, int frame_number);

/* ------------------------------------------------------------------ */
/*  Disk cache management (DiskManager integration)                     */
/* ------------------------------------------------------------------ */

int64_t oak_coord_cache_size(OakCoordHandle coord);
void    oak_coord_set_cache_limit(OakCoordHandle coord, int64_t limit_bytes);
void    oak_coord_purge_cache(OakCoordHandle coord, int older_than_seconds);

/* ------------------------------------------------------------------ */
/*  Callbacks                                                           */
/* ------------------------------------------------------------------ */

typedef void (*OakRenderProgressFn)(int job_id, int completed, int total, void* user_data);
typedef void (*OakRenderFrameDoneFn)(int job_id, int frame_number, void* user_data);
typedef void (*OakRenderJobDoneFn)(int job_id, bool success, void* user_data);

typedef struct {
    OakRenderProgressFn  on_progress;
    OakRenderFrameDoneFn on_frame_done;
    OakRenderJobDoneFn   on_job_done;
} OakCoordCallbacks;

void oak_coord_set_callbacks(OakCoordHandle coord,
                             const OakCoordCallbacks* callbacks,
                             void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* OAK_COORD_API_H */
