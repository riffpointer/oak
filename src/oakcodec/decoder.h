/***

  Oak Video Editor - Decoder Base
  Copyright (C) 2025 mikesolar

  Decoupled from node/, render/, and task/ modules for oakcodec.so.

***/

#ifndef OAKCODEC_DECODER_H
#define OAKCODEC_DECODER_H

extern "C" {
#include <libswresample/swresample.h>
}

#include <QFileInfo>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <cstdint>
#include <memory>

#include "footagedescription.h"
#include "loopmode.h"
#include "rendermode.h"
#include "olive/common/cancelatom.h"
#include "olive/common/ffmpegutils.h"

namespace olive
{

class Decoder;
using DecoderPtr = std::shared_ptr<Decoder>;

#define DECODER_DEFAULT_DESTRUCTOR(x) \
    virtual ~x() override             \
    {                                 \
        CloseInternal();              \
    }

class Decoder : public QObject {
	Q_OBJECT
public:
    enum RetrieveState { kReady, kFailedToOpen, kIndexUnavailable };

    Decoder();
    virtual ~Decoder();

    virtual QString id() const = 0;

    virtual bool SupportsVideo() { return false; }
    virtual bool SupportsAudio() { return false; }

    void IncrementAccessTime(qint64 t);

    class CodecStream {
    public:
        CodecStream() : stream_(-1) {}
        CodecStream(const QString &filename, int stream)
            : filename_(filename), stream_(stream) {}

        bool IsValid() const { return !filename_.isEmpty() && stream_ >= 0; }
        bool Exists() const { return QFileInfo::exists(filename_); }
        void Reset() { *this = CodecStream(); }

        bool operator==(const CodecStream &rhs) const
        {
            return filename_ == rhs.filename_ && stream_ == rhs.stream_;
        }

        const QString &filename() const { return filename_; }
        int stream() const { return stream_; }

    private:
        QString filename_;
        int stream_;
    };

    bool Open(const CodecStream &stream);

    static const rational kAnyTimecode;

    struct RetrieveVideoParams {
        rational time;
        int divider = 1;
        PixelFormat maximum_format = PixelFormat::INVALID;
        CancelAtom *cancelled = nullptr;
        VideoParams::ColorRange force_range = VideoParams::kColorRangeDefault;
        VideoParams::Interlacing src_interlacing = VideoParams::kInterlaceNone;
    };

    /**
     * @brief Retrieve a video frame as raw AVFrame.
     *
     * Returns nullptr on fatal error. The caller owns the returned frame
     * and must use it through the C API (e.g. upload to oakgl via
     * oak_texture_upload_from_frame).
     */
    AVFramePtr RetrieveVideo(const RetrieveVideoParams &p);

    enum RetrieveAudioStatus {
        kInvalid = -1,
        kOK,
        kWaitingForConform,
        kUnknownError
    };

    RetrieveAudioStatus
    RetrieveAudio(SampleBuffer &dest, const TimeRange &range,
                  const AudioParams &params, const QString &cache_path,
                  LoopMode loop_mode, RenderMode::Mode mode);

    qint64 GetLastAccessedTime();

    virtual FootageDescription Probe(const QString &filename,
                                     CancelAtom *cancelled) const = 0;

    void Close();

    bool ConformAudio(const QVector<QString> &output_filenames,
                      const AudioParams &params,
                      CancelAtom *cancelled = nullptr);

    static DecoderPtr CreateFromID(const QString &id);

    static QString TransformImageSequenceFileName(const QString &filename,
                                                  const int64_t &number);

    static int GetImageSequenceDigitCount(const QString &filename);
    static int64_t GetImageSequenceIndex(const QString &filename);

    static QVector<DecoderPtr> ReceiveListOfAllDecoders();

protected:
    virtual bool OpenInternal() = 0;
    virtual void CloseInternal() = 0;
    virtual AVFramePtr RetrieveVideoInternal(const RetrieveVideoParams &p);
    virtual bool ConformAudioInternal(const QVector<QString> &filenames,
                                      const AudioParams &params,
                                      CancelAtom *cancelled);

    void SignalProcessingProgress(int64_t ts, int64_t duration);

    const CodecStream &stream() const { return stream_; }

    virtual rational GetAudioStartOffset() const { return 0; }

signals:
    void IndexProgress(double progress);

private:
    void UpdateLastAccessed();

    bool RetrieveAudioFromConform(SampleBuffer &sample_buffer,
                                  const QVector<QString> &conform_filenames,
                                  TimeRange range, LoopMode loop_mode,
                                  const AudioParams &params);

    CodecStream stream_;
    QMutex mutex_;
    std::atomic_int64_t last_accessed_;

    AVFramePtr cached_frame_;
    rational cached_time_;
    int cached_divider_;
};

uint qHash(Decoder::CodecStream stream, uint seed = 0);

} // namespace olive

#endif // OAKCODEC_DECODER_H
