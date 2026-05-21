/*
 * Olive - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * Pure C API for libolivecodec — codec, frame, and media probing.
 */

#ifndef OLIVE_CODEC_API_H
#define OLIVE_CODEC_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "export.h"
#include "core_api.h"

#define OLIVE_CODEC_API_VERSION 1

/* ========== Opaque types ========== */
typedef struct OliveFrame     OliveFrame;
typedef struct OliveMediaInfo OliveMediaInfo;

/* ========== Version ========== */
OLIVE_CODEC_API int olive_codec_api_version(void);

/* ========== Frame ========== */
OLIVE_CODEC_API OliveFrame* olive_frame_create(int width, int height,
                                                 OlivePixelFormat format,
                                                 int channel_count);
OLIVE_CODEC_API void        olive_frame_destroy(OliveFrame* frame);
OLIVE_CODEC_API int         olive_frame_width(OliveFrame* frame);
OLIVE_CODEC_API int         olive_frame_height(OliveFrame* frame);
OLIVE_CODEC_API int         olive_frame_channel_count(OliveFrame* frame);
OLIVE_CODEC_API OlivePixelFormat olive_frame_format(OliveFrame* frame);
OLIVE_CODEC_API char*       olive_frame_data(OliveFrame* frame);
OLIVE_CODEC_API int         olive_frame_linesize_bytes(OliveFrame* frame);
OLIVE_CODEC_API int         olive_frame_allocated_size(OliveFrame* frame);
OLIVE_CODEC_API int         olive_frame_allocate(OliveFrame* frame);
OLIVE_CODEC_API void        olive_frame_destroy_data(OliveFrame* frame);

/* ========== Media Probe ========== */
OLIVE_CODEC_API OliveMediaInfo* olive_media_info_probe(const char* filename);
OLIVE_CODEC_API void            olive_media_info_destroy(OliveMediaInfo* info);
OLIVE_CODEC_API int             olive_media_info_is_valid(OliveMediaInfo* info);
OLIVE_CODEC_API int             olive_media_info_stream_count(OliveMediaInfo* info);
OLIVE_CODEC_API int             olive_media_info_video_stream_count(OliveMediaInfo* info);
OLIVE_CODEC_API int             olive_media_info_audio_stream_count(OliveMediaInfo* info);
OLIVE_CODEC_API int             olive_media_info_get_video_width(OliveMediaInfo* info,
                                                                    int stream_index);
OLIVE_CODEC_API int             olive_media_info_get_video_height(OliveMediaInfo* info,
                                                                    int stream_index);
OLIVE_CODEC_API OliveRational   olive_media_info_get_video_frame_rate(OliveMediaInfo* info,
                                                                        int stream_index);
OLIVE_CODEC_API int             olive_media_info_get_audio_sample_rate(OliveMediaInfo* info,
                                                                         int stream_index);
OLIVE_CODEC_API int             olive_media_info_get_audio_channel_count(OliveMediaInfo* info,
                                                                           int stream_index);

/* ========== Decoder enumeration ========== */
OLIVE_CODEC_API int         olive_decoder_count(void);
OLIVE_CODEC_API const char* olive_decoder_id(int index);

#ifdef __cplusplus
}
#endif

#endif // OLIVE_CODEC_API_H
