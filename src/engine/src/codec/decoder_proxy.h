/***  Oak Video Editor - Decoder Proxy  Copyright (C) 2025 mikesolar  ***/

#ifndef DECODER_PROXY_H
#define DECODER_PROXY_H

#include <functional>
#include <memory>
#include <QVector>
#include <QString>
#include <QHash>

#include "codec/codec_stream.h"
#include "codec/footage_description.h"
#include "olive/common/cancelatom.h"
#include "olive/common/ffmpegutils.h"
#include "olive/render/videoparams.h"
#include "olive/core/render/audioparams.h"
#include "olive/core/util/rational.h"
#include "render/loopmode.h"
#include "render/rendermode.h"
#include "node/block/block.h"

namespace olive {

using namespace core;

class DecoderProxy;
using DecoderProxyPtr = std::shared_ptr<DecoderProxy>;

class DecoderProxy {
public:
    enum RetrieveState { kReady, kFailedToOpen, kIndexUnavailable };
    enum RetrieveAudioStatus {
        kInvalid = -1,
        kOK,
        kWaitingForConform,
        kUnknownError
    };

    struct RetrieveVideoParams {
        rational time;
        int divider = 1;
        PixelFormat maximum_format = PixelFormat::INVALID;
        CancelAtom *cancelled = nullptr;
        VideoParams::ColorRange force_range = VideoParams::kColorRangeDefault;
        VideoParams::Interlacing src_interlacing = VideoParams::kInterlaceNone;
        void* renderer = nullptr;
    };

    DecoderProxy();
    ~DecoderProxy();

    QString id() const;
    bool SupportsVideo();
    bool SupportsAudio();

    bool Open(const CodecStream &stream);
    void Close();

    AVFramePtr RetrieveVideo(const RetrieveVideoParams &p);
    RetrieveAudioStatus RetrieveAudio(SampleBuffer &dest, const TimeRange &range,
                                      const AudioParams &params, const QString &cache_path,
                                      LoopMode loop_mode, RenderMode::Mode mode);

    qint64 GetLastAccessedTime() const;
    void IncrementAccessTime(qint64 t);

    bool ConformAudio(const QVector<QString> &output_filenames,
                      const AudioParams &params,
                      CancelAtom *cancelled = nullptr);

    void SetProgressCallback(std::function<void(double)> cb);

    static const rational kAnyTimecode;

    static DecoderProxyPtr CreateFromID(const QString &id);
    static QString TransformImageSequenceFileName(const QString &filename,
                                                  const int64_t &number);
    static int GetImageSequenceDigitCount(const QString &filename);
    static int64_t GetImageSequenceIndex(const QString &filename);
    static QVector<DecoderProxyPtr> ReceiveListOfAllDecoders();

    static FootageDescription ProbeMedia(const QString &filename, const QString &decoder_id,
                                         CancelAtom *cancelled);

private:
    void* handle_;  // OakDecoderHandle
    qint64 last_accessed_;
    std::function<void(double)> progress_cb_;
    CodecStream stream_;

    static void StaticProgressCallback(double progress, void* userdata);
};

uint qHash(const CodecStream &stream, uint seed);

} // namespace olive

#endif // DECODER_PROXY_H
