/***

  Oak Video Editor - Footage Description
  Copyright (C) 2025 mikesolar

  Decoupled from node/output/track/track.h for oakcodec.so independence.

***/

#ifndef OAKCODEC_FOOTAGEDESCRIPTION_H
#define OAKCODEC_FOOTAGEDESCRIPTION_H

#include <QVector>
#include <QString>

#include "olive/render/videoparams.h"
#include "olive/core/render/audioparams.h"
#include "olive/render/subtitleparams.h"

namespace olive
{

class FootageDescription {
public:
    enum StreamType { kNone = 0, kVideo, kAudio, kSubtitle };

    FootageDescription(const QString &decoder = QString())
        : decoder_(decoder)
        , total_stream_count_(0)
    {
    }

    bool IsValid() const
    {
        return !decoder_.isEmpty() &&
               (!video_streams_.isEmpty() || !audio_streams_.isEmpty() ||
                !subtitle_streams_.isEmpty());
    }

    const QString &decoder() const { return decoder_; }

    void AddVideoStream(const VideoParams &video_params)
    {
        Q_ASSERT(!HasStreamIndex(video_params.stream_index()));
        video_streams_.append(video_params);
    }

    void AddAudioStream(const AudioParams &audio_params)
    {
        Q_ASSERT(!HasStreamIndex(audio_params.stream_index()));
        audio_streams_.append(audio_params);
    }

    void AddSubtitleStream(const SubtitleParams &sub_params)
    {
        Q_ASSERT(!HasStreamIndex(sub_params.stream_index()));
        subtitle_streams_.append(sub_params);
    }

    StreamType GetTypeOfStream(int index) const
    {
        if (StreamIsVideo(index)) {
            return kVideo;
        } else if (StreamIsAudio(index)) {
            return kAudio;
        } else if (StreamIsSubtitle(index)) {
            return kSubtitle;
        } else {
            return kNone;
        }
    }

    bool StreamIsVideo(int index) const
    {
        foreach (const VideoParams &vp, video_streams_) {
            if (vp.stream_index() == index) return true;
        }
        return false;
    }

    bool StreamIsAudio(int index) const
    {
        foreach (const AudioParams &ap, audio_streams_) {
            if (ap.stream_index() == index) return true;
        }
        return false;
    }

    bool StreamIsSubtitle(int index) const
    {
        foreach (const SubtitleParams &sp, subtitle_streams_) {
            if (sp.stream_index() == index) return true;
        }
        return false;
    }

    bool HasStreamIndex(int index) const
    {
        return StreamIsVideo(index) || StreamIsAudio(index) || StreamIsSubtitle(index);
    }

    int GetStreamCount() const { return total_stream_count_; }
    void SetStreamCount(int s) { total_stream_count_ = s; }

    bool Load(const QString &filename);
    bool Save(const QString &filename) const;

    const QVector<VideoParams> &GetVideoStreams() const { return video_streams_; }
    QVector<VideoParams> &GetVideoStreams() { return video_streams_; }

    const QVector<AudioParams> &GetAudioStreams() const { return audio_streams_; }
    QVector<AudioParams> &GetAudioStreams() { return audio_streams_; }

    const QVector<SubtitleParams> &GetSubtitleStreams() const { return subtitle_streams_; }
    QVector<SubtitleParams> &GetSubtitleStreams() { return subtitle_streams_; }

private:
    static constexpr unsigned kFootageMetaVersion = 6;

    QString decoder_;
    QVector<VideoParams> video_streams_;
    QVector<AudioParams> audio_streams_;
    QVector<SubtitleParams> subtitle_streams_;
    int total_stream_count_;
};

} // namespace olive

#endif // OAKCODEC_FOOTAGEDESCRIPTION_H
