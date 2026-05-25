/***  Oak Video Editor - Decoder Proxy  Copyright (C) 2025 mikesolar  ***/

#include "decoder_proxy.h"
#include "runtime/oak_codec_runtime.h"
#include <QFileInfo>

namespace olive {

const rational DecoderProxy::kAnyTimecode = rational(-1);

DecoderProxy::DecoderProxy()
    : handle_(nullptr)
    , last_accessed_(0)
{
}

DecoderProxy::~DecoderProxy()
{
    Close();
}

QString DecoderProxy::id() const
{
    auto rt = OakCodecRuntime::Instance();
    if (!handle_) return QString();
    const char* s = rt->decoder_id(handle_);
    return s ? QString::fromUtf8(s) : QString();
}

bool DecoderProxy::SupportsVideo()
{
    auto rt = OakCodecRuntime::Instance();
    return handle_ && rt->decoder_supports_video(handle_);
}

bool DecoderProxy::SupportsAudio()
{
    auto rt = OakCodecRuntime::Instance();
    return handle_ && rt->decoder_supports_audio(handle_);
}

bool DecoderProxy::Open(const CodecStream &stream)
{
    auto rt = OakCodecRuntime::Instance();
    if (!rt->Load() || !handle_) return false;
    stream_ = stream;
    return rt->decoder_open_stream(handle_, stream.filename().toUtf8().constData(),
                                   stream.stream()) == 0;
}

void DecoderProxy::Close()
{
    auto rt = OakCodecRuntime::Instance();
    if (handle_) {
        rt->decoder_close(handle_);
        handle_ = nullptr;
    }
}

AVFramePtr DecoderProxy::RetrieveVideo(const RetrieveVideoParams &p)
{
    auto rt = OakCodecRuntime::Instance();
    if (!handle_) return nullptr;

    OakDecoderVideoParams params;
    params.time_num = p.time.numerator();
    params.time_den = p.time.denominator();
    params.divider = p.divider;
    params.maximum_format = static_cast<int>(OAK_FRAME_PIX_FMT_RGBA8); // TODO: map
    params.force_range = 0;
    params.renderer_hint = p.renderer;
    params.cancelled = p.cancelled;

    OakFrame frame;
    memset(&frame, 0, sizeof(frame));
    if (rt->decoder_read_video_ex(handle_, stream_.stream(), &params, &frame) != 0) {
        return nullptr;
    }

    void* internal = frame.internal;
    if (!internal) {
        oak_frame_release(&frame);
        return nullptr;
    }

    AVFramePtr result = CloneAVFramePtr(internal);
    oak_frame_release(&frame);
    return result;
}

DecoderProxy::RetrieveAudioStatus
DecoderProxy::RetrieveAudio(SampleBuffer &dest, const TimeRange &range,
                            const AudioParams &params, const QString &cache_path,
                            LoopMode loop_mode, RenderMode::Mode mode)
{
    auto rt = OakCodecRuntime::Instance();
    if (!handle_) return kUnknownError;

    OakDecoderAudioParams ap;
    ap.start_sample = 0; // TODO: convert range to samples
    ap.sample_count = dest.audio_params().time_to_bytes(range.length()) / sizeof(float) / dest.audio_params().channel_count();
    ap.loop_mode = static_cast<int>(loop_mode);
    ap.render_mode = static_cast<int>(mode);
    ap.cache_path = cache_path.isEmpty() ? nullptr : cache_path.toUtf8().constData();

    float* data = nullptr;
    int64_t actual = 0;
    if (rt->decoder_read_audio_ex(handle_, stream_.stream(), &ap, &data, &actual) != 0) {
        return kUnknownError;
    }

    if (data && actual > 0) {
        // TODO: properly fill SampleBuffer from planar/interleaved float data
        rt->audio_buffer_free(data);
    }

    return kOK;
}

qint64 DecoderProxy::GetLastAccessedTime() const
{
    return last_accessed_;
}

void DecoderProxy::IncrementAccessTime(qint64 t)
{
    last_accessed_ = t;
}

bool DecoderProxy::ConformAudio(const QVector<QString> &output_filenames,
                                const AudioParams &params,
                                CancelAtom *cancelled)
{
    auto rt = OakCodecRuntime::Instance();
    if (!handle_) return false;

    // C API conform operates on a cache_path; we use the first output filename as cache path
    QString cache_path = output_filenames.isEmpty() ? QString() : output_filenames.first();
    int r = rt->decoder_conform_audio(handle_, cache_path.toUtf8().constData(),
                                      params.sample_rate(), params.channel_count(),
                                      OAK_AUDIO_FMT_FLT);
    return r == 0;
}

void DecoderProxy::SetProgressCallback(std::function<void(double)> cb)
{
    progress_cb_ = cb;
    auto rt = OakCodecRuntime::Instance();
    if (!handle_) return;

    if (cb) {
        rt->oak_decoder_set_progress_callback(handle_,
            [](double p, void* ud) {
                auto* self = static_cast<DecoderProxy*>(ud);
                if (self && self->progress_cb_) self->progress_cb_(p);
            }, this);
    } else {
        rt->oak_decoder_set_progress_callback(handle_, nullptr, nullptr);
    }
}

DecoderProxyPtr DecoderProxy::CreateFromID(const QString &id)
{
    auto rt = OakCodecRuntime::Instance();
    if (!rt->Load()) return nullptr;

    OakDecoderHandle h = rt->decoder_create_from_id(id.toUtf8().constData());
    if (!h) return nullptr;

    auto proxy = std::make_shared<DecoderProxy>();
    proxy->handle_ = h;
    return proxy;
}

QString DecoderProxy::TransformImageSequenceFileName(const QString &filename,
                                                     const int64_t &number)
{
    int digit_count = GetImageSequenceDigitCount(filename);
    QFileInfo file_info(filename);
    QString original_basename = file_info.completeBaseName();
    QString new_basename =
        original_basename.left(original_basename.size() - digit_count)
            .append(QStringLiteral("%1").arg(number, digit_count, 10, QChar('0')));
    return file_info.dir().filePath(
        file_info.fileName().replace(original_basename, new_basename));
}

int DecoderProxy::GetImageSequenceDigitCount(const QString &filename)
{
    QString basename = QFileInfo(filename).completeBaseName();
    int digit_count = 0;
    for (int i = basename.size() - 1; i >= 0; i--) {
        if (basename.at(i).isDigit()) {
            digit_count++;
        } else {
            break;
        }
    }
    return digit_count;
}

int64_t DecoderProxy::GetImageSequenceIndex(const QString &filename)
{
    int digit_count = GetImageSequenceDigitCount(filename);
    QFileInfo file_info(filename);
    QString original_basename = file_info.completeBaseName();
    QString number_only = original_basename.mid(original_basename.size() - digit_count);
    return number_only.toLongLong();
}

QVector<DecoderProxyPtr> DecoderProxy::ReceiveListOfAllDecoders()
{
    // Hardcoded list matching oakcodec.so internals
    QVector<DecoderProxyPtr> list;
    auto d_oiio = CreateFromID(QStringLiteral("oiio"));
    if (d_oiio) list.append(d_oiio);
    auto d_ffmpeg = CreateFromID(QStringLiteral("ffmpeg"));
    if (d_ffmpeg) list.append(d_ffmpeg);
    return list;
}

FootageDescription DecoderProxy::ProbeMedia(const QString &filename,
                                            const QString &decoder_id,
                                            CancelAtom *cancelled)
{
    auto proxy = CreateFromID(decoder_id);
    if (!proxy) return FootageDescription();

    auto rt = OakCodecRuntime::Instance();
    OakMediaInfo info;
    memset(&info, 0, sizeof(info));
    if (rt->decoder_probe_file(proxy->handle_, filename.toUtf8().constData(), &info) != 0) {
        return FootageDescription();
    }

    FootageDescription desc(decoder_id);
    desc.SetStreamCount(info.video_stream_count + info.audio_stream_count
                        + info.subtitle_stream_count);

    for (int i = 0; i < info.video_stream_count; i++) {
        const OakVideoStreamInfo& vi = info.video_streams[i];
        VideoParams vp(vi.width, vi.height,
                       rational(vi.timebase_num, vi.timebase_den),
                       PixelFormat::U8, 4); // TODO: map pix_fmt properly
        vp.set_stream_index(i);
        desc.AddVideoStream(vp);
    }

    for (int i = 0; i < info.audio_stream_count; i++) {
        const OakAudioStreamInfo& ai = info.audio_streams[i];
        AudioParams ap(ai.sample_rate, ai.channels,
                       SampleFormat::F32P);
        ap.set_stream_index(i + info.video_stream_count);
        desc.AddAudioStream(ap);
    }

    // TODO: free info arrays via media_info_free if exposed
    return desc;
}

void DecoderProxy::StaticProgressCallback(double progress, void* userdata)
{
    auto* self = static_cast<DecoderProxy*>(userdata);
    if (self && self->progress_cb_) {
        self->progress_cb_(progress);
    }
}

} // namespace olive
