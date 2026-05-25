/***  Oak Video Editor - CodecStream (engine-local copy)  Copyright (C) 2025 mikesolar  ***/

#ifndef CODEC_STREAM_H
#define CODEC_STREAM_H

#include <QFileInfo>
#include <QString>
#include <QHash>

namespace olive {

class CodecStream {
public:
    CodecStream() : stream_(-1) {}
    CodecStream(const QString &filename, int stream)
        : filename_(filename), stream_(stream) {}
    CodecStream(const QString &filename, int stream, void* /*unused*/)
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

inline uint qHash(const CodecStream &stream, uint seed = 0)
{
    return qHash(stream.filename(), seed) ^ static_cast<uint>(stream.stream());
}

} // namespace olive

#endif // CODEC_STREAM_H
