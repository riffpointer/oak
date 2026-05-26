/***  Oak Video Editor - Encoder Proxy  Copyright (C) 2025 mikesolar  ***/

#include "encoder_proxy.h"
#include "runtime/oak_codec_runtime.h"
#include "codec/exportformat.h"
#include "codec/exportcodec.h"

namespace olive {

EncoderProxy::EncoderProxy()
    : handle_(nullptr)
{
}

EncoderProxy::~EncoderProxy()
{
    Close();
}

bool EncoderProxy::Create(const QString &filepath, const QString &container,
                          const QString &video_codec, const QString &audio_codec)
{
    auto rt = OakCodecRuntime::Instance();
    if (!rt->Load()) return false;
    handle_ = rt->encoder_create(filepath.toUtf8().constData(),
                                 container.toUtf8().constData(),
                                 video_codec.toUtf8().constData(),
                                 audio_codec.toUtf8().constData());
    return handle_ != nullptr;
}

bool EncoderProxy::Open(const EncodingParams &params)
{
    params_ = params;

    QString container = ExportFormat::GetName(params.format());
    QString video_codec = params.video_enabled() ? ExportCodec::GetCodecName(params.video_codec()) : QString();
    QString audio_codec = params.audio_enabled() ? ExportCodec::GetCodecName(params.audio_codec()) : QString();

    if (!Create(params.filename(), container, video_codec, audio_codec)) {
        return false;
    }

    if (params.video_enabled()) {
        SetVideoParams(params.video_params());
    }
    if (params.audio_enabled()) {
        SetAudioParams(params.audio_params());
    }
    return true;
}

void EncoderProxy::Close()
{
    auto rt = OakCodecRuntime::Instance();
    if (handle_) {
        rt->encoder_close(handle_);
        handle_ = nullptr;
    }
}

void EncoderProxy::SetVideoParams(const VideoParams &params)
{
    auto rt = OakCodecRuntime::Instance();
    if (!handle_) return;
    rt->encoder_set_video_params(handle_, params.effective_width(), params.effective_height(),
                                 OAK_FRAME_PIX_RGBA8, // TODO: map
                                 params.time_base().numerator(),
                                 params.time_base().denominator(),
                                 params.frame_rate().toDouble());
}

void EncoderProxy::SetAudioParams(const AudioParams &params)
{
    auto rt = OakCodecRuntime::Instance();
    if (!handle_) return;
    rt->encoder_set_audio_params(handle_, params.sample_rate(), params.channel_count(),
                                 OAK_AUDIO_FMT_FLT,
                                 params.time_base().numerator(),
                                 params.time_base().denominator());
}

bool EncoderProxy::WriteFrame(const OakFrame *frame)
{
    auto rt = OakCodecRuntime::Instance();
    if (!handle_ || !frame) return false;
    return rt->encoder_write_video(handle_, frame) == 0;
}

bool EncoderProxy::WriteAudio(const float *data, int64_t samples, const rational &pts)
{
    auto rt = OakCodecRuntime::Instance();
    if (!handle_) return false;
    return rt->encoder_write_audio(handle_, data, samples,
                                   pts.numerator(), pts.denominator()) == 0;
}

bool EncoderProxy::WriteAudioData(const AudioParams &audio_params,
                                    const uint8_t **data,
                                    int64_t frames)
{
    int64_t samples = frames * audio_params.channel_count();
    const float *float_data = reinterpret_cast<const float*>(data[0]);
    return WriteAudio(float_data, samples, rational(0, 1));
}

bool EncoderProxy::Finalize()
{
    auto rt = OakCodecRuntime::Instance();
    if (!handle_) return false;
    return rt->encoder_finalize(handle_) == 0;
}

} // namespace olive
