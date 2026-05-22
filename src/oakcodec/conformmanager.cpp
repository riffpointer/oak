/*
 * Oak Video Editor - Conform Manager
 * Copyright (C) 2025 Olive CE Team
 */

#include "conformmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent>

#include "olive/common/filefunctions.h"

namespace olive
{

ConformManager *ConformManager::instance_ = nullptr;

ConformManager::ConformManager()
{
}

ConformManager::~ConformManager()
{
    QMutexLocker locker(&mutex_);
    for (const ConformData &data : conforming_) {
        if (data.future.isRunning()) {
            // TODO: cancel future gracefully
        }
        if (data.watcher) {
            data.watcher->deleteLater();
        }
    }
}

static bool RunConform(const QString &decoder_id,
                       const Decoder::CodecStream &stream,
                       const AudioParams &params,
                       const QVector<QString> &output_filenames)
{
    DecoderPtr decoder = Decoder::CreateFromID(decoder_id);

    if (!decoder || !decoder->Open(stream)) {
        qDebug() << "Failed to open decoder for audio conform";
        return false;
    }

    qDebug() << "Starting conform of" << stream.filename() << stream.stream();

    bool ret = decoder->ConformAudio(output_filenames, params, nullptr);

    decoder->Close();

    return ret;
}

ConformManager::Conform ConformManager::GetConformState(
    const QString &decoder_id, const QString &cache_path,
    const Decoder::CodecStream &stream, const AudioParams &params, bool wait)
{
    QMutexLocker locker(&mutex_);

    QVector<QString> filenames =
        GetConformedFilename(cache_path, stream, params);
    if (AllConformsExist(filenames)) {
        return { kConformExists, filenames };
    }

    // Check if already conforming
    int existing_index = -1;
    for (int i = 0; i < conforming_.size(); i++) {
        if (conforming_[i].stream == stream && conforming_[i].params == params) {
            existing_index = i;
            break;
        }
    }

    if (existing_index < 0) {
        // Not conforming yet, start a new background task
        QVector<QString> working_filenames = filenames;
        for (int i = 0; i < working_filenames.size(); i++) {
            working_filenames[i].append(QStringLiteral(".working"));
        }

        ConformData data;
        data.stream = stream;
        data.params = params;
        data.working_filename = working_filenames;
        data.finished_filename = filenames;
        data.watcher = new QFutureWatcher<bool>();

        int index = conforming_.size();
        conforming_.append(data);

        // Start async conform
        QFuture<bool> future = QtConcurrent::run(
            RunConform, decoder_id, stream, params, working_filenames);
        conforming_[index].future = future;

        connect(data.watcher, &QFutureWatcher<bool>::finished, this, [this, index]() {
            OnConformFinished(index);
        });
        data.watcher->setFuture(future);
    }

    if (wait) {
        do {
            conform_done_condition_.wait(&mutex_);
        } while (!AllConformsExist(filenames));
        return { kConformExists, filenames };
    }

    return { kConformGenerating, QVector<QString>() };
}

int ConformManager::Poll(const QString &cache_path,
                         const Decoder::CodecStream &stream,
                         const AudioParams &params)
{
    QMutexLocker locker(&mutex_);

    QVector<QString> filenames =
        GetConformedFilename(cache_path, stream, params);
    if (AllConformsExist(filenames)) {
        return 0; // done
    }

    for (const ConformData &data : conforming_) {
        if (data.stream == stream && data.params == params) {
            if (data.future.isRunning()) {
                return 1; // in progress
            } else if (data.future.isFinished()) {
                bool success = data.future.result();
                return success ? 0 : -1;
            }
        }
    }

    return -1; // not found / failed
}

void ConformManager::OnConformFinished(int index)
{
    QMutexLocker locker(&mutex_);

    if (index < 0 || index >= conforming_.size()) return;

    ConformData data = conforming_.takeAt(index);
    bool success = data.future.result();

    if (data.watcher) {
        data.watcher->deleteLater();
    }

    if (success) {
        for (int i = 0; i < data.finished_filename.size(); i++) {
            const QString &finished = data.finished_filename.at(i);
            const QString &working = data.working_filename.at(i);
            QFile::remove(finished);
            QFile::rename(working, finished);
        }
        conform_done_condition_.wakeAll();
        locker.unlock();
        emit ConformReady();
    } else {
        for (int i = 0; i < data.working_filename.size(); i++) {
            QFile::remove(data.working_filename.at(i));
        }
    }
}

QVector<QString>
ConformManager::GetConformedFilename(const QString &cache_path,
                                     const Decoder::CodecStream &stream,
                                     const AudioParams &params)
{
    QVector<QString> filenames(params.channel_count());

    for (int i = 0; i < filenames.size(); i++) {
        QString index_fn =
            QStringLiteral("%1-%2.%3.%4.%5.%6.pcm")
                .arg(FileFunctions::GetUniqueFileIdentifier(stream.filename()),
                     QString::number(stream.stream()),
                     QString::number(params.sample_rate()),
                     QString::number(params.format()),
                     QString::number(params.channel_layout().u.mask),
                     QString::number(i));

        filenames[i] = QDir(cache_path).filePath(index_fn);
    }

    return filenames;
}

bool ConformManager::AllConformsExist(const QVector<QString> &filenames)
{
    foreach (const QString &fn, filenames) {
        if (!QFileInfo::exists(fn)) {
            return false;
        }
    }
    return true;
}

} // namespace olive
