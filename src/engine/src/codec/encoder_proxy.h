/***  Oak Video Editor - Encoder Proxy  Copyright (C) 2025 mikesolar  ***/

#ifndef ENCODER_PROXY_H
#define ENCODER_PROXY_H

#include <QString>
#include <QVector>
#include "encoder.h"
#include "olive/render/videoparams.h"
#include "olive/core/render/audioparams.h"
#include "oak/codec_api.h"
#include "oak/frame_api.h"

namespace olive {

class EncoderProxy {
public:
    EncoderProxy();
    ~EncoderProxy();

    bool Create(const QString &filepath, const QString &container,
                const QString &video_codec, const QString &audio_codec);
    bool Open(const EncodingParams &params);
    void Close();

    const EncodingParams &params() const { return params_; }

    void SetVideoParams(const VideoParams &params);
    void SetAudioParams(const AudioParams &params);

    bool WriteFrame(const OakFrame *frame);
    bool WriteAudio(const float *data, int64_t samples,
                    const rational &pts);
    bool WriteAudioData(const AudioParams &audio_params,
                        const uint8_t **data,
                        int64_t frames);
    bool Finalize();

private:
    OakEncoderHandle handle_; // OakEncoderHandle
    EncodingParams params_;
};

} // namespace olive

#endif // ENCODER_PROXY_H
