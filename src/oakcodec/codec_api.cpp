/***

  oakcodec.so C API Implementation
  Copyright (C) 2025 mikesolar

***/

#include "oak/codec_api.h"
#include "oakcodec_internal.h"

#include <cstring>
#include <cstdlib>

#include "decoder.h"
#include "encoder.h"
#include "conformmanager.h"
#include "ffmpeg/ffmpegdecoder.h"
#include "oiio/oiiodecoder.h"
#include "ffmpeg/ffmpegencoder.h"
#include "oiio/oiioencoder.h"
#include "olive/common/ffmpegutils.h"

/* ================================================================ */
/*  Helpers                                                          */
/* ================================================================ */

static olive::PixelFormat OakPixFmtToOlive(OakPixelFormat fmt)
{
    switch (fmt) {
    case OAK_PIX_FMT_RGBA8:    return olive::PixelFormat::U8;
    case OAK_PIX_FMT_RGBA16:   return olive::PixelFormat::U16;
    case OAK_PIX_FMT_RGBA32F:  return olive::PixelFormat::F32;
    case OAK_PIX_FMT_RGB8:     return olive::PixelFormat::U8;
    case OAK_PIX_FMT_YUV420P8: return olive::PixelFormat::U8;
    case OAK_PIX_FMT_YUV422P8: return olive::PixelFormat::U8;
    case OAK_PIX_FMT_YUV444P8: return olive::PixelFormat::U8;
    default:                   return olive::PixelFormat::INVALID;
    }
}

static OakPixelFormat OlivePixFmtToOak(olive::PixelFormat fmt, int channels)
{
    if (channels == 4) {
        switch (fmt) {
        case olive::PixelFormat::U8:  return OAK_PIX_FMT_RGBA8;
        case olive::PixelFormat::U16: return OAK_PIX_FMT_RGBA16;
        case olive::PixelFormat::F32: return OAK_PIX_FMT_RGBA32F;
        default: break;
        }
    }
    return OAK_PIX_FMT_INVALID;
}

static olive::SampleFormat OakAudioFmtToOlive(OakAudioFormat fmt)
{
    switch (fmt) {
    case OAK_AUDIO_FMT_U8:  return olive::SampleFormat::U8;
    case OAK_AUDIO_FMT_S16: return olive::SampleFormat::S16;
    case OAK_AUDIO_FMT_S32: return olive::SampleFormat::S32;
    case OAK_AUDIO_FMT_FLT: return olive::SampleFormat::F32;
    case OAK_AUDIO_FMT_DBL: return olive::SampleFormat::F64;
    default:                return olive::SampleFormat::INVALID;
    }
}

static OakAudioFormat OliveAudioFmtToOak(olive::SampleFormat fmt)
{
    switch (fmt) {
    case olive::SampleFormat::U8:  return OAK_AUDIO_FMT_U8;
    case olive::SampleFormat::S16: return OAK_AUDIO_FMT_S16;
    case olive::SampleFormat::S32: return OAK_AUDIO_FMT_S32;
    case olive::SampleFormat::F32: return OAK_AUDIO_FMT_FLT;
    case olive::SampleFormat::F64: return OAK_AUDIO_FMT_DBL;
    default:                       return OAK_AUDIO_FMT_INVALID;
    }
}

/* ================================================================ */
/*  Decoder lifecycle                                                 */
/* ================================================================ */

OakDecoderHandle oak_decoder_open(const char *filepath, const char *codec_hint,
                                  OakMediaInfo *out_info)
{
    (void)codec_hint;
    if (!filepath) return nullptr;

    QString fn = QString::fromUtf8(filepath);
    olive::CancelAtom cancel;

    QVector<olive::DecoderPtr> all_decoders = olive::Decoder::ReceiveListOfAllDecoders();
    olive::DecoderPtr selected;
    olive::FootageDescription desc;

    for (const olive::DecoderPtr &d : all_decoders) {
        desc = d->Probe(fn, &cancel);
        if (desc.IsValid()) {
            selected = d;
            break;
        }
    }

    if (!selected) return nullptr;

    auto *wrapper = new oakcodec::OakDecoderWrapper();
    wrapper->filepath = fn;
    wrapper->desc = desc;
    wrapper->decoder = selected;

    // Open first suitable stream (prefer video)
    if (!desc.GetVideoStreams().isEmpty()) {
        const olive::VideoParams &vp = desc.GetVideoStreams().first();
        olive::Decoder::CodecStream stream(fn, vp.stream_index());
        wrapper->decoder->Open(stream);
    } else if (!desc.GetAudioStreams().isEmpty()) {
        const olive::AudioParams &ap = desc.GetAudioStreams().first();
        olive::Decoder::CodecStream stream(fn, ap.stream_index());
        wrapper->decoder->Open(stream);
    }

    if (out_info) {
        out_info->video_stream_count = desc.GetVideoStreams().size();
        out_info->audio_stream_count = desc.GetAudioStreams().size();
        out_info->subtitle_stream_count = desc.GetSubtitleStreams().size();

        if (out_info->video_stream_count > 0) {
            out_info->video_streams = (OakVideoStreamInfo *)std::malloc(
                out_info->video_stream_count * sizeof(OakVideoStreamInfo));
            for (int i = 0; i < out_info->video_stream_count; i++) {
                const olive::VideoParams &vp = desc.GetVideoStreams().at(i);
                out_info->video_streams[i] = {
                    vp.width(), vp.height(),
                    OlivePixFmtToOak(vp.format(), vp.channel_count()),
                    vp.time_base().numerator(), vp.time_base().denominator(),
                    vp.frame_rate().toDouble(), vp.duration()
                };
            }
        } else {
            out_info->video_streams = nullptr;
        }

        if (out_info->audio_stream_count > 0) {
            out_info->audio_streams = (OakAudioStreamInfo *)std::malloc(
                out_info->audio_stream_count * sizeof(OakAudioStreamInfo));
            for (int i = 0; i < out_info->audio_stream_count; i++) {
                const olive::AudioParams &ap = desc.GetAudioStreams().at(i);
                out_info->audio_streams[i] = {
                    ap.sample_rate(), ap.channel_count(),
                    OliveAudioFmtToOak(ap.format()),
                    ap.time_base().numerator(), ap.time_base().denominator(),
                    ap.duration()
                };
            }
        } else {
            out_info->audio_streams = nullptr;
        }
    }

    return reinterpret_cast<OakDecoderHandle>(wrapper);
}

void oak_decoder_close(OakDecoderHandle decoder)
{
    if (!decoder) return;
    auto *wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper *>(decoder);
    if (wrapper->decoder) {
        wrapper->decoder->Close();
    }
    delete wrapper;
}

void oak_media_info_free(OakMediaInfo *info)
{
    if (!info) return;
    if (info->video_streams) {
        std::free(info->video_streams);
        info->video_streams = nullptr;
    }
    if (info->audio_streams) {
        std::free(info->audio_streams);
        info->audio_streams = nullptr;
    }
    info->video_stream_count = 0;
    info->audio_stream_count = 0;
}

/* ================================================================ */
/*  Video decode                                                      */
/* ================================================================ */

int oak_decoder_read_video(OakDecoderHandle decoder, int stream_index,
                           int64_t time_num, int64_t time_den,
                           OakPixelFormat out_pix_fmt,
                           int out_width, int out_height,
                           void **out_data, int *out_stride)
{
    (void)stream_index;
    (void)out_width;
    (void)out_height;
    auto *wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper *>(decoder);
    if (!wrapper || !wrapper->decoder) return -1;

    olive::Decoder::RetrieveVideoParams p;
    p.time = olive::core::rational(time_num, time_den);
    p.maximum_format = OakPixFmtToOlive(out_pix_fmt);

    olive::AVFramePtr frame = wrapper->decoder->RetrieveVideo(p);
    if (!frame) return -1;

    // Allocate output buffer and copy data
    int channels = 4; // AV_PIX_FMT_RGBAF32
    int bytes_per_channel = 4;
    int stride = frame->width * channels * bytes_per_channel;
    size_t size = stride * frame->height;
    void *data = std::malloc(size);
    if (!data) return -1;

    // Copy line by line (linesize may include padding)
    for (int y = 0; y < frame->height; y++) {
        memcpy((uint8_t *)data + y * stride,
               frame->data[0] + y * frame->linesize[0],
               stride);
    }

    if (out_data) *out_data = data;
    if (out_stride) *out_stride = stride;
    return 0;
}

void oak_frame_free(void *data)
{
    std::free(data);
}

int oak_decoder_thumbnail(OakDecoderHandle decoder, int stream_index,
                          int max_size,
                          void **out_data, int *out_width, int *out_height,
                          int *out_stride)
{
    (void)decoder;
    (void)stream_index;
    (void)max_size;
    if (out_data) *out_data = nullptr;
    if (out_width) *out_width = 0;
    if (out_height) *out_height = 0;
    if (out_stride) *out_stride = 0;
    return -1; /* TODO */
}

/* ================================================================ */
/*  Audio decode & conform                                            */
/* ================================================================ */

int oak_decoder_read_audio(OakDecoderHandle decoder, int stream_index,
                           int64_t start_sample, int64_t sample_count,
                           float **out_data, int64_t *out_actual_samples)
{
    (void)stream_index;
    auto *wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper *>(decoder);
    if (!wrapper || !wrapper->decoder) return -1;

    // Get audio params from description
    if (wrapper->desc.GetAudioStreams().isEmpty()) return -1;
    const olive::AudioParams &ap = wrapper->desc.GetAudioStreams().first();

    olive::SampleBuffer buffer(ap, static_cast<size_t>(sample_count));
    buffer.allocate();

    olive::TimeRange range(
        olive::core::rational(start_sample, ap.sample_rate()),
        olive::core::rational(sample_count, ap.sample_rate()));

    auto status = wrapper->decoder->RetrieveAudio(
        buffer, range, ap, QString(),
        olive::LoopMode::kLoopModeOff, olive::RenderMode::kOffline);

    if (status != olive::Decoder::kOK) {
        if (out_data) *out_data = nullptr;
        if (out_actual_samples) *out_actual_samples = 0;
        return -1;
    }

    // Convert planar float32 to interleaved float32
    int channels = buffer.channel_count();
    size_t samples = buffer.sample_count();
    size_t total = channels * samples;
    float *data = (float *)std::malloc(total * sizeof(float));
    if (!data) return -1;

    for (size_t s = 0; s < samples; s++) {
        for (int c = 0; c < channels; c++) {
            data[s * channels + c] = buffer.data(c)[s];
        }
    }

    if (out_data) *out_data = data;
    if (out_actual_samples) *out_actual_samples = static_cast<int64_t>(samples);
    return 0;
}

void oak_audio_buffer_free(float *data)
{
    std::free(data);
}

/* ---- Conform ---- */

int oak_conform_get(const char *decoder_id, const char *cache_path,
                    int stream_index,
                    int target_sample_rate, int target_channels,
                    bool wait,
                    const char ***out_filenames, int *out_count)
{
    (void)stream_index;
    if (!decoder_id || !cache_path) return -1;

    olive::ConformManager::CreateInstance();
    olive::ConformManager *cm = olive::ConformManager::instance();

    // We need a Decoder::CodecStream to identify the source.
    // The C API uses decoder_id (e.g. "ffmpeg") and we reconstruct a stream
    // from cached info. For now, this is a stub that requires the caller
    // to have opened the decoder beforehand.
    // TODO: store stream info in a global registry keyed by decoder_id

    (void)target_sample_rate;
    (void)target_channels;
    (void)wait;
    (void)cm;

    if (out_filenames) *out_filenames = nullptr;
    if (out_count) *out_count = 0;
    return -1; /* TODO: need stream reconstruction from decoder_id */
}

int oak_conform_poll(const char *decoder_id)
{
    (void)decoder_id;
    return -1; /* TODO */
}

void oak_conform_free_filenames(const char **filenames, int count)
{
    if (!filenames) return;
    for (int i = 0; i < count; i++) {
        std::free((void *)filenames[i]);
    }
    std::free((void *)filenames);
}

/* ================================================================ */
/*  Encoder                                                           */
/* ================================================================ */

OakEncoderHandle oak_encoder_create(const char *filepath,
                                    const char *container_format,
                                    const char *video_codec,
                                    const char *audio_codec)
{
    (void)container_format;
    (void)video_codec;
    (void)audio_codec;

    if (!filepath) return nullptr;

    olive::EncodingParams params;
    params.SetFilename(QString::fromUtf8(filepath));

    auto *wrapper = new oakcodec::OakEncoderWrapper();
    wrapper->params = params;
    // TODO: map container_format / video_codec / audio_codec to ExportCodec/ExportFormat

    return reinterpret_cast<OakEncoderHandle>(wrapper);
}

void oak_encoder_close(OakEncoderHandle encoder)
{
    if (!encoder) return;
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (wrapper->encoder) {
        wrapper->encoder->Close();
        delete wrapper->encoder;
    }
    delete wrapper;
}

void oak_encoder_set_video_params(OakEncoderHandle encoder,
                                  int width, int height, OakPixelFormat pix_fmt,
                                  int64_t timebase_num, int64_t timebase_den,
                                  double frame_rate)
{
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper) return;

    olive::VideoParams vp;
    vp.set_width(width);
    vp.set_height(height);
    vp.set_format(OakPixFmtToOlive(pix_fmt));
    vp.set_channel_count(4);
    vp.set_time_base(olive::core::rational(timebase_num, timebase_den));
    vp.set_frame_rate(olive::core::rational::fromDouble(frame_rate));

    wrapper->params.EnableVideo(vp, olive::ExportCodec::kCodecH264);
}

void oak_encoder_set_audio_params(OakEncoderHandle encoder,
                                  int sample_rate, int channels,
                                  OakAudioFormat sample_fmt,
                                  int64_t timebase_num, int64_t timebase_den)
{
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper) return;

    uint64_t layout_mask = (channels == 1) ? AV_CH_LAYOUT_MONO :
                           (channels == 2) ? AV_CH_LAYOUT_STEREO :
                           (channels == 6) ? AV_CH_LAYOUT_5POINT1 :
                           (channels == 8) ? AV_CH_LAYOUT_7POINT1 :
                           AV_CH_LAYOUT_STEREO;
    olive::AudioParams ap(sample_rate, layout_mask, OakAudioFmtToOlive(sample_fmt));
    ap.set_time_base(olive::core::rational(timebase_num, timebase_den));

    wrapper->params.EnableAudio(ap, olive::ExportCodec::kCodecAAC);
}

int oak_encoder_write_video(OakEncoderHandle encoder,
                            const void *data, int stride,
                            int64_t pts_num, int64_t pts_den)
{
    (void)stride;
    (void)pts_num;
    (void)pts_den;
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper) return -1;

    if (!wrapper->encoder) {
        // Lazily create encoder from params
        wrapper->encoder = olive::Encoder::CreateFromParams(wrapper->params);
        if (!wrapper->encoder) return -1;
        if (!wrapper->encoder->Open()) return -1;
    }

    // TODO: wrap raw data into olive::FramePtr and call WriteFrame
    (void)data;
    return -1; /* TODO: need Frame wrapping from raw pixels */
}

int oak_encoder_write_audio(OakEncoderHandle encoder,
                            const float *data, int64_t samples,
                            int64_t pts_num, int64_t pts_den)
{
    (void)pts_num;
    (void)pts_den;
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper) return -1;

    if (!wrapper->encoder) {
        wrapper->encoder = olive::Encoder::CreateFromParams(wrapper->params);
        if (!wrapper->encoder) return -1;
        if (!wrapper->encoder->Open()) return -1;
    }

    // TODO: wrap interleaved float data into olive::SampleBuffer and call WriteAudio
    (void)data;
    (void)samples;
    return -1; /* TODO: need SampleBuffer wrapping from interleaved float */
}

int oak_encoder_finalize(OakEncoderHandle encoder)
{
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper || !wrapper->encoder) return -1;

    wrapper->encoder->Close();
    return 0;
}
