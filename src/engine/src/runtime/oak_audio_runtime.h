/***  Oak Video Editor - OakAudio Runtime Loader  Copyright (C) 2025 mikesolar  ***/

#ifndef OAK_AUDIO_RUNTIME_H
#define OAK_AUDIO_RUNTIME_H

#include "runtime_loader.h"
#include "oak/audio_api.h"

namespace olive {

class OakAudioRuntime : public OakRuntimeLoader {
public:
    static OakAudioRuntime* Instance();

    bool Load();

    /* --- filter graph --- */
    OakAudioFilterGraphHandle (*filter_graph_create)(const OakAudioParams* from,
                                                     const OakAudioParams* to,
                                                     double tempo) = nullptr;
    void (*filter_graph_destroy)(OakAudioFilterGraphHandle graph) = nullptr;
    int (*filter_graph_process)(OakAudioFilterGraphHandle graph,
                                const float** in_data, int64_t in_samples,
                                float** out_data, int64_t* out_samples,
                                int* out_channels) = nullptr;
    int (*filter_graph_flush)(OakAudioFilterGraphHandle graph,
                              float** out_data, int64_t* out_samples,
                              int* out_channels) = nullptr;
    void (*filter_graph_free_output)(float* data) = nullptr;

private:
    OakAudioRuntime() = default;
};

} // namespace olive

#endif // OAK_AUDIO_RUNTIME_H
