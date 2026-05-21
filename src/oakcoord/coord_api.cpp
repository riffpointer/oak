#include "oak/coord_api.h"
#include "oakcoord_internal.h"

#include <cstring>

/* ------------------------------------------------------------------ */
/*  Coordinator lifecycle                                               */
/* ------------------------------------------------------------------ */

OakCoordHandle oak_coord_create(const char* renderer_executable_path,
                                int max_concurrent_renderers,
                                const char* shm_prefix) {
    auto* c = new OakRenderCoordinator();
    c->renderer_executable_path = renderer_executable_path ? renderer_executable_path : "";
    c->max_concurrent = max_concurrent_renderers;
    c->shm_prefix = shm_prefix ? shm_prefix : "/oak_render_";
    // TODO: init process pool, shared memory manager
    return c;
}

void oak_coord_destroy(OakCoordHandle coord) {
    if (!coord) return;
    // TODO: cancel all jobs, terminate renderer processes, unlink shm
    delete coord;
}

void oak_coord_set_cache_path(OakCoordHandle coord, const char* cache_path) {
    if (coord && cache_path) coord->cache_path = cache_path;
}

void oak_coord_set_quality(OakCoordHandle coord, OakRenderQuality quality) {
    if (coord) coord->quality = quality;
}

/* ------------------------------------------------------------------ */
/*  Job submission                                                      */
/* ------------------------------------------------------------------ */

int oak_coord_submit_job(OakCoordHandle coord,
                         const char* graph_json,
                         int start_frame, int end_frame, int frame_step,
                         const char* video_params,
                         const char* color_config_path,
                         int* out_job_id) {
    if (!coord) return -1;

    OakRenderJob job;
    job.job_id = coord->next_job_id++;
    job.graph_json = graph_json ? graph_json : "";
    job.start_frame = start_frame;
    job.end_frame = end_frame;
    job.frame_step = frame_step;
    job.video_params = video_params ? video_params : "";
    job.color_config_path = color_config_path ? color_config_path : "";

    int id = job.job_id;
    coord->jobs[id] = std::move(job);

    // TODO: fork/exec renderer process, send load_graph + config + render_range

    if (out_job_id) *out_job_id = id;
    return 0;
}

void oak_coord_cancel_job(OakCoordHandle coord, int job_id) {
    if (!coord) return;
    auto it = coord->jobs.find(job_id);
    if (it == coord->jobs.end()) return;
    // TODO: send cancel command to renderer process
    coord->jobs.erase(it);
}

void oak_coord_cancel_all(OakCoordHandle coord) {
    if (!coord) return;
    // TODO: cancel every job
    coord->jobs.clear();
}

/* ------------------------------------------------------------------ */
/*  Polling & result retrieval                                          */
/* ------------------------------------------------------------------ */

int oak_coord_poll_job(OakCoordHandle coord, int job_id,
                       int* out_completed_frames,
                       int* out_total_frames,
                       int* out_failed_frames) {
    if (!coord) return -1;
    auto it = coord->jobs.find(job_id);
    if (it == coord->jobs.end()) return -1;

    int total = it->second.end_frame - it->second.start_frame + 1;
    if (out_total_frames)     *out_total_frames     = total;
    if (out_completed_frames) *out_completed_frames = 0;
    if (out_failed_frames)    *out_failed_frames    = 0;

    // TODO: read stdout from renderer process, parse progress events
    return 0; /* in progress */
}

int oak_coord_get_frame(OakCoordHandle coord, int job_id, int frame_number,
                        int* out_status,
                        void** out_data,
                        int* out_width, int* out_height, int* out_pix_fmt,
                        int* out_stride) {
    (void)coord; (void)job_id; (void)frame_number;
    if (out_status) *out_status = OAK_FRAME_PENDING;
    if (out_data)   *out_data   = nullptr;
    if (out_width)  *out_width  = 0;
    if (out_height) *out_height = 0;
    if (out_pix_fmt)*out_pix_fmt = 0;
    if (out_stride) *out_stride = 0;
    return -1; /* TODO */
}

void oak_coord_release_frame(OakCoordHandle coord, int job_id, int frame_number) {
    (void)coord; (void)job_id; (void)frame_number;
    // TODO: send release_frame command to renderer, unmap shm
}

/* ------------------------------------------------------------------ */
/*  Preview cache                                                       */
/* ------------------------------------------------------------------ */

void oak_coord_start_preview_cache(OakCoordHandle coord,
                                   const char* graph_json,
                                   int playhead_frame, int cache_radius) {
    (void)coord; (void)graph_json; (void)playhead_frame; (void)cache_radius;
    // TODO: submit low-priority background job for playhead +/- cache_radius
}

void oak_coord_update_playhead(OakCoordHandle coord, int playhead_frame) {
    (void)coord; (void)playhead_frame;
    // TODO: adjust preview cache job range
}

void oak_coord_stop_preview_cache(OakCoordHandle coord) {
    (void)coord;
    // TODO: cancel preview cache job
}

bool oak_coord_preview_cached(OakCoordHandle coord, int frame_number) {
    (void)coord; (void)frame_number;
    return false; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Disk cache management                                               */
/* ------------------------------------------------------------------ */

int64_t oak_coord_cache_size(OakCoordHandle coord) {
    (void)coord;
    return 0; /* TODO */
}

void oak_coord_set_cache_limit(OakCoordHandle coord, int64_t limit_bytes) {
    (void)coord; (void)limit_bytes;
    // TODO
}

void oak_coord_purge_cache(OakCoordHandle coord, int older_than_seconds) {
    (void)coord; (void)older_than_seconds;
    // TODO
}

/* ------------------------------------------------------------------ */
/*  Callbacks                                                           */
/* ------------------------------------------------------------------ */

void oak_coord_set_callbacks(OakCoordHandle coord,
                             const OakCoordCallbacks* callbacks,
                             void* user_data) {
    if (!coord) return;
    if (callbacks) {
        coord->callbacks = *callbacks;
    } else {
        coord->callbacks = OakCoordCallbacks{};
    }
    coord->callback_user_data = user_data;
}
