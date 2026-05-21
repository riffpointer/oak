#ifndef OAKCOORD_INTERNAL_H
#define OAKCOORD_INTERNAL_H

#include "oak/coord_api.h"

#include <string>
#include <vector>
#include <map>

/* Internal C++ structures backing the C API opaque handles.
 * Will manage renderer process pool, shared memory, and cache. */

struct OakRenderJob {
    int job_id = 0;
    int start_frame = 0;
    int end_frame = 0;
    int frame_step = 1;
    std::string graph_json;
    std::string video_params;
    std::string color_config_path;
    // TODO: process handle, shm segments
};

struct OakRenderCoordinator {
    std::string renderer_executable_path;
    std::string shm_prefix;
    int max_concurrent = 4;
    std::string cache_path;
    OakRenderQuality quality = OAK_RENDER_QUALITY_FULL;

    std::map<int, OakRenderJob> jobs;
    int next_job_id = 1;

    OakCoordCallbacks callbacks{};
    void* callback_user_data = nullptr;

    // TODO: process pool, shm management, preview cache, disk cache
};

#endif /* OAKCOORD_INTERNAL_H */
