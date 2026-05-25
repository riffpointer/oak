/***  Oak Video Editor - Codec Runtime Loader  Copyright (C) 2025 mikesolar  ***/

#include "oak_codec_runtime.h"

namespace olive {

OakCodecRuntime* OakCodecRuntime::Instance()
{
    static OakCodecRuntime instance;
    return &instance;
}

bool OakCodecRuntime::Load()
{
    if (IsLoaded()) {
        return true;
    }

#if defined(__APPLE__)
    if (!OakRuntimeLoader::Load(QStringLiteral("liboakcodec.dylib"))) {
        return false;
    }
#elif defined(__linux__)
    if (!OakRuntimeLoader::Load(QStringLiteral("liboakcodec.so"))) {
        return false;
    }
#elif defined(_WIN32)
    if (!OakRuntimeLoader::Load(QStringLiteral("oakcodec.dll"))) {
        return false;
    }
#endif

    decoder_open = GetSymbol<decltype(decoder_open)>("oak_decoder_open");
    decoder_close = GetSymbol<decltype(decoder_close)>("oak_decoder_close");
    media_info_free = GetSymbol<decltype(media_info_free)>("oak_media_info_free");

    decoder_create_from_id = GetSymbol<decltype(decoder_create_from_id)>("oak_decoder_create_from_id");
    decoder_id = GetSymbol<decltype(decoder_id)>("oak_decoder_id");
    decoder_supports_video = GetSymbol<decltype(decoder_supports_video)>("oak_decoder_supports_video");
    decoder_supports_audio = GetSymbol<decltype(decoder_supports_audio)>("oak_decoder_supports_audio");
    decoder_is_open = GetSymbol<decltype(decoder_is_open)>("oak_decoder_is_open");

    decoder_probe_file = GetSymbol<decltype(decoder_probe_file)>("oak_decoder_probe_file");
    decoder_open_stream = GetSymbol<decltype(decoder_open_stream)>("oak_decoder_open_stream");

    decoder_read_video = GetSymbol<decltype(decoder_read_video)>("oak_decoder_read_video");
    decoder_thumbnail = GetSymbol<decltype(decoder_thumbnail)>("oak_decoder_thumbnail");
    decoder_read_video_ex = GetSymbol<decltype(decoder_read_video_ex)>("oak_decoder_read_video_ex");

    decoder_read_audio = GetSymbol<decltype(decoder_read_audio)>("oak_decoder_read_audio");
    audio_buffer_free = GetSymbol<decltype(audio_buffer_free)>("oak_audio_buffer_free");
    decoder_read_audio_ex = GetSymbol<decltype(decoder_read_audio_ex)>("oak_decoder_read_audio_ex");

    decoder_conform_audio = GetSymbol<decltype(decoder_conform_audio)>("oak_decoder_conform_audio");

    conform_get = GetSymbol<decltype(conform_get)>("oak_conform_get");
    conform_poll = GetSymbol<decltype(conform_poll)>("oak_conform_poll");
    conform_free_filenames = GetSymbol<decltype(conform_free_filenames)>("oak_conform_free_filenames");
    conform_set_ready_callback = GetSymbol<decltype(conform_set_ready_callback)>("oak_conform_set_ready_callback");

    encoder_create = GetSymbol<decltype(encoder_create)>("oak_encoder_create");
    encoder_close = GetSymbol<decltype(encoder_close)>("oak_encoder_close");
    encoder_set_video_params = GetSymbol<decltype(encoder_set_video_params)>("oak_encoder_set_video_params");
    encoder_set_video_output_format = GetSymbol<decltype(encoder_set_video_output_format)>("oak_encoder_set_video_output_format");
    encoder_set_video_output_colorspace = GetSymbol<decltype(encoder_set_video_output_colorspace)>("oak_encoder_set_video_output_colorspace");
    encoder_set_audio_params = GetSymbol<decltype(encoder_set_audio_params)>("oak_encoder_set_audio_params");
    encoder_write_video = GetSymbol<decltype(encoder_write_video)>("oak_encoder_write_video");
    encoder_write_audio = GetSymbol<decltype(encoder_write_audio)>("oak_encoder_write_audio");
    encoder_finalize = GetSymbol<decltype(encoder_finalize)>("oak_encoder_finalize");

    if (!decoder_open || !decoder_close || !encoder_create || !encoder_close) {
        qWarning() << "Failed to resolve essential codec symbols from oakcodec.so";
        return false;
    }

    return true;
}

} // namespace olive
