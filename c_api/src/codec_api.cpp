/*
 * Olive - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * C API implementation for libolivecodec.
 */

#include "olive/codec_api.h"

#include "codec/decoder.h"
#include "codec/frame.h"
#include "codec/ffmpeg/ffmpegdecoder.h"
#include "render/videoparams.h"
#include "olive/core/render/audioparams.h"

#include <QVector>
#include <QString>
#include <memory>
#include <cstring>

using olive::DecoderPtr;
using olive::FramePtr;
using olive::VideoParams;
using olive::AudioParams;

/* ========== Helpers: PixelFormat C <-> C++ ========== */

static olive::PixelFormat ToCppPixelFormat(OlivePixelFormat fmt)
{
    switch (fmt) {
    case OLIVE_PIXEL_FMT_U8:   return olive::PixelFormat::U8;
    case OLIVE_PIXEL_FMT_U16:  return olive::PixelFormat::U16;
    case OLIVE_PIXEL_FMT_F16:  return olive::PixelFormat::F16;
    case OLIVE_PIXEL_FMT_F32:  return olive::PixelFormat::F32;
    default:                   return olive::PixelFormat::INVALID;
    }
}

static OlivePixelFormat ToCPixelFormat(olive::PixelFormat fmt)
{
    switch (static_cast<olive::PixelFormat::Format>(fmt)) {
    case olive::PixelFormat::U8:   return OLIVE_PIXEL_FMT_U8;
    case olive::PixelFormat::U16:  return OLIVE_PIXEL_FMT_U16;
    case olive::PixelFormat::F16:  return OLIVE_PIXEL_FMT_F16;
    case olive::PixelFormat::F32:  return OLIVE_PIXEL_FMT_F32;
    default:                       return OLIVE_PIXEL_FMT_INVALID;
    }
}

/* ========== OliveFrame wrapper ========== */

struct OliveFrame {
    FramePtr ptr;

    explicit OliveFrame(FramePtr p)
        : ptr(std::move(p))
    {
    }
};

/* ========== OliveMediaInfo wrapper ========== */

struct OliveMediaInfo {
    bool valid = false;
    int total_stream_count = 0;
    int video_stream_count = 0;
    int audio_stream_count = 0;

    struct VideoStream {
        int width = 0;
        int height = 0;
        int stream_index = -1;
        olive::core::rational frame_rate;
    };
    struct AudioStream {
        int sample_rate = 0;
        int channel_count = 0;
        int stream_index = -1;
    };

    std::vector<VideoStream> video_streams;
    std::vector<AudioStream> audio_streams;
};

/* ========== C API implementation ========== */

extern "C" {

/* ----- Version ----- */
int olive_codec_api_version(void)
{
    return OLIVE_CODEC_API_VERSION;
}

/* ----- Frame ----- */

OliveFrame* olive_frame_create(int width, int height, OlivePixelFormat format,
                               int channel_count)
{
    try {
        FramePtr f = olive::Frame::Create();
        VideoParams params(width, height, ToCppPixelFormat(format), channel_count);
        f->set_video_params(params);
        return new OliveFrame(f);
    } catch (...) {
        return nullptr;
    }
}

void olive_frame_destroy(OliveFrame* frame)
{
    delete frame;
}

int olive_frame_width(OliveFrame* frame)
{
    if (!frame || !frame->ptr) {
        return 0;
    }
    try {
        return frame->ptr->width();
    } catch (...) {
        return 0;
    }
}

int olive_frame_height(OliveFrame* frame)
{
    if (!frame || !frame->ptr) {
        return 0;
    }
    try {
        return frame->ptr->height();
    } catch (...) {
        return 0;
    }
}

int olive_frame_channel_count(OliveFrame* frame)
{
    if (!frame || !frame->ptr) {
        return 0;
    }
    try {
        return frame->ptr->channel_count();
    } catch (...) {
        return 0;
    }
}

OlivePixelFormat olive_frame_format(OliveFrame* frame)
{
    if (!frame || !frame->ptr) {
        return OLIVE_PIXEL_FMT_INVALID;
    }
    try {
        return ToCPixelFormat(frame->ptr->format());
    } catch (...) {
        return OLIVE_PIXEL_FMT_INVALID;
    }
}

char* olive_frame_data(OliveFrame* frame)
{
    if (!frame || !frame->ptr) {
        return nullptr;
    }
    try {
        return frame->ptr->data();
    } catch (...) {
        return nullptr;
    }
}

int olive_frame_linesize_bytes(OliveFrame* frame)
{
    if (!frame || !frame->ptr) {
        return 0;
    }
    try {
        return frame->ptr->linesize_bytes();
    } catch (...) {
        return 0;
    }
}

int olive_frame_allocated_size(OliveFrame* frame)
{
    if (!frame || !frame->ptr) {
        return 0;
    }
    try {
        return frame->ptr->allocated_size();
    } catch (...) {
        return 0;
    }
}

int olive_frame_allocate(OliveFrame* frame)
{
    if (!frame || !frame->ptr) {
        return OLIVE_ERROR_INVALID;
    }
    try {
        return frame->ptr->allocate() ? OLIVE_OK : OLIVE_ERROR_GENERIC;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

void olive_frame_destroy_data(OliveFrame* frame)
{
    if (!frame || !frame->ptr) {
        return;
    }
    try {
        frame->ptr->destroy();
    } catch (...) {
        // swallow
    }
}

/* ----- Media Probe ----- */

OliveMediaInfo* olive_media_info_probe(const char* filename)
{
    if (!filename) {
        return nullptr;
    }

    try {
        auto info = std::make_unique<OliveMediaInfo>();

        olive::FFmpegDecoder decoder;
        olive::FootageDescription desc = decoder.Probe(QString::fromUtf8(filename), nullptr);

        if (!desc.IsValid()) {
            return info.release(); // return empty (invalid) info
        }

        info->valid = true;
        info->total_stream_count = desc.GetStreamCount();

        const QVector<VideoParams> &video_streams = desc.GetVideoStreams();
        info->video_stream_count = video_streams.size();
        for (const VideoParams &vp : video_streams) {
            OliveMediaInfo::VideoStream vs;
            vs.width = vp.width();
            vs.height = vp.height();
            vs.stream_index = vp.stream_index();
            vs.frame_rate = vp.time_base().flipped();
            info->video_streams.push_back(vs);
        }

        const QVector<AudioParams> &audio_streams = desc.GetAudioStreams();
        info->audio_stream_count = audio_streams.size();
        for (const AudioParams &ap : audio_streams) {
            OliveMediaInfo::AudioStream as;
            as.sample_rate = ap.sample_rate();
            as.channel_count = ap.channel_count();
            as.stream_index = ap.stream_index();
            info->audio_streams.push_back(as);
        }

        return info.release();
    } catch (...) {
        return nullptr;
    }
}

void olive_media_info_destroy(OliveMediaInfo* info)
{
    delete info;
}

int olive_media_info_is_valid(OliveMediaInfo* info)
{
    if (!info) {
        return 0;
    }
    return info->valid ? 1 : 0;
}

int olive_media_info_stream_count(OliveMediaInfo* info)
{
    if (!info) {
        return 0;
    }
    return info->total_stream_count;
}

int olive_media_info_video_stream_count(OliveMediaInfo* info)
{
    if (!info) {
        return 0;
    }
    return info->video_stream_count;
}

int olive_media_info_audio_stream_count(OliveMediaInfo* info)
{
    if (!info) {
        return 0;
    }
    return info->audio_stream_count;
}

int olive_media_info_get_video_width(OliveMediaInfo* info, int stream_index)
{
    if (!info || stream_index < 0 || stream_index >= info->video_stream_count) {
        return 0;
    }
    try {
        return info->video_streams[stream_index].width;
    } catch (...) {
        return 0;
    }
}

int olive_media_info_get_video_height(OliveMediaInfo* info, int stream_index)
{
    if (!info || stream_index < 0 || stream_index >= info->video_stream_count) {
        return 0;
    }
    try {
        return info->video_streams[stream_index].height;
    } catch (...) {
        return 0;
    }
}

OliveRational olive_media_info_get_video_frame_rate(OliveMediaInfo* info, int stream_index)
{
    if (!info || stream_index < 0 || stream_index >= info->video_stream_count) {
        return {0, 1};
    }
    try {
        const auto &fr = info->video_streams[stream_index].frame_rate;
        return {static_cast<int64_t>(fr.numerator()), static_cast<int64_t>(fr.denominator())};
    } catch (...) {
        return {0, 1};
    }
}

int olive_media_info_get_audio_sample_rate(OliveMediaInfo* info, int stream_index)
{
    if (!info || stream_index < 0 || stream_index >= info->audio_stream_count) {
        return 0;
    }
    try {
        return info->audio_streams[stream_index].sample_rate;
    } catch (...) {
        return 0;
    }
}

int olive_media_info_get_audio_channel_count(OliveMediaInfo* info, int stream_index)
{
    if (!info || stream_index < 0 || stream_index >= info->audio_stream_count) {
        return 0;
    }
    try {
        return info->audio_streams[stream_index].channel_count;
    } catch (...) {
        return 0;
    }
}

/* ----- Decoder enumeration ----- */

int olive_decoder_count(void)
{
    try {
        QVector<DecoderPtr> decoders = olive::Decoder::ReceiveListOfAllDecoders();
        return decoders.size();
    } catch (...) {
        return 0;
    }
}

const char* olive_decoder_id(int index)
{
    try {
        QVector<DecoderPtr> decoders = olive::Decoder::ReceiveListOfAllDecoders();
        if (index < 0 || index >= decoders.size()) {
            return "";
        }
        static thread_local QByteArray id_buf;
        id_buf = decoders[index]->id().toUtf8();
        return id_buf.constData();
    } catch (...) {
        return "";
    }
}

} // extern "C"
