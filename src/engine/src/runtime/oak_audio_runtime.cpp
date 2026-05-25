/***  Oak Video Editor - OakAudio Runtime Loader  Copyright (C) 2025 mikesolar  ***/

#include "oak_audio_runtime.h"
#include <QDebug>

namespace olive {

OakAudioRuntime* OakAudioRuntime::Instance()
{
    static OakAudioRuntime instance;
    return &instance;
}

bool OakAudioRuntime::Load()
{
    if (IsLoaded()) {
        return true;
    }

#if defined(__APPLE__)
    if (!OakRuntimeLoader::Load(QStringLiteral("liboakaudio.dylib"))) {
        return false;
    }
#elif defined(__linux__)
    if (!OakRuntimeLoader::Load(QStringLiteral("liboakaudio.so"))) {
        return false;
    }
#elif defined(_WIN32)
    if (!OakRuntimeLoader::Load(QStringLiteral("oakaudio.dll"))) {
        return false;
    }
#endif

    filter_graph_create = GetSymbol<decltype(filter_graph_create)>("oak_audio_filter_graph_create");
    filter_graph_destroy = GetSymbol<decltype(filter_graph_destroy)>("oak_audio_filter_graph_destroy");
    filter_graph_process = GetSymbol<decltype(filter_graph_process)>("oak_audio_filter_graph_process");
    filter_graph_flush = GetSymbol<decltype(filter_graph_flush)>("oak_audio_filter_graph_flush");
    filter_graph_free_output = GetSymbol<decltype(filter_graph_free_output)>("oak_audio_filter_graph_free_output");

    if (!filter_graph_create || !filter_graph_destroy || !filter_graph_process) {
        qWarning() << "Failed to resolve essential audio symbols from oakaudio.so";
        return false;
    }

    return true;
}

} // namespace olive
