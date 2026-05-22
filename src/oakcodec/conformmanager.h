/*
 * Oak Video Editor - Conform Manager
 * Copyright (C) 2025 Olive CE Team
 *
 * Decoupled from task/ module for oakcodec.so independence.
 */

#ifndef OAKCODEC_CONFORMMANAGER_H
#define OAKCODEC_CONFORMMANAGER_H

#include <QMutex>
#include <QWaitCondition>
#include <QVector>
#include <QString>
#include <QFuture>
#include <QFutureWatcher>

#include "decoder.h"

namespace olive
{

class ConformManager : public QObject {
    Q_OBJECT
public:
    static void CreateInstance()
    {
        if (!instance_) {
            instance_ = new ConformManager();
        }
    }

    static void DestroyInstance()
    {
        delete instance_;
        instance_ = nullptr;
    }

    static ConformManager *instance()
    {
        return instance_;
    }

    enum ConformState { kConformExists, kConformGenerating };

    struct Conform {
        ConformState state;
        QVector<QString> filenames;
    };

    /**
     * @brief Get conform state, and start conforming if no conform exists
     *
     * Thread-safe.
     */
    Conform GetConformState(const QString &decoder_id,
                            const QString &cache_path,
                            const Decoder::CodecStream &stream,
                            const AudioParams &params, bool wait);

    int Poll(const QString &cache_path,
             const Decoder::CodecStream &stream,
             const AudioParams &params);

signals:
    void ConformReady();

private:
    ConformManager();
    ~ConformManager();

    static ConformManager *instance_;

    QMutex mutex_;
    QWaitCondition conform_done_condition_;

    struct ConformData {
        Decoder::CodecStream stream;
        AudioParams params;
        QFuture<bool> future;
        QFutureWatcher<bool> *watcher;
        QVector<QString> working_filename;
        QVector<QString> finished_filename;
    };

    QVector<ConformData> conforming_;

    static QVector<QString>
    GetConformedFilename(const QString &cache_path,
                         const Decoder::CodecStream &stream,
                         const AudioParams &params);

    static bool AllConformsExist(const QVector<QString> &filenames);

    void OnConformFinished(int index);
};

} // namespace olive

#endif // OAKCODEC_CONFORMMANAGER_H
