/***

  Oak Video Editor - Footage Description
  Copyright (C) 2025 mikesolar

***/

#include "footagedescription.h"

#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "olive/common/xmlutils.h"

namespace olive
{

static AudioParams LoadAudioParams(QXmlStreamReader *reader)
{
    AudioParams a;
    while (XMLReadNextStartElement(reader)) {
        if (reader->name() == QStringLiteral("samplerate")) {
            a.set_sample_rate(reader->readElementText().toInt());
        } else if (reader->name() == QStringLiteral("channellayout")) {
            a.set_channel_layout(reader->readElementText().toULongLong());
        } else if (reader->name() == QStringLiteral("format")) {
            a.set_format(SampleFormat::from_string(
                reader->readElementText().toStdString()));
        } else if (reader->name() == QStringLiteral("enabled")) {
            a.set_enabled(reader->readElementText().toInt());
        } else if (reader->name() == QStringLiteral("streamindex")) {
            a.set_stream_index(reader->readElementText().toInt());
        } else if (reader->name() == QStringLiteral("duration")) {
            a.set_duration(reader->readElementText().toLongLong());
        } else if (reader->name() == QStringLiteral("timebase")) {
            a.set_time_base(
                rational::fromString(reader->readElementText().toStdString()));
        } else {
            reader->skipCurrentElement();
        }
    }
    return a;
}

static void SaveAudioParams(QXmlStreamWriter *writer, const AudioParams &a)
{
    writer->writeTextElement(QStringLiteral("samplerate"),
                             QString::number(a.sample_rate()));
    writer->writeTextElement(QStringLiteral("channellayout"),
                             QString::number(a.channel_layout_mask()));
    writer->writeTextElement(QStringLiteral("format"),
                             QString::fromStdString(a.format().to_string()));
    writer->writeTextElement(QStringLiteral("enabled"),
                             QString::number(a.enabled()));
    writer->writeTextElement(QStringLiteral("streamindex"),
                             QString::number(a.stream_index()));
    writer->writeTextElement(QStringLiteral("duration"),
                             QString::number(a.duration()));
    writer->writeTextElement(QStringLiteral("timebase"),
                             QString::fromStdString(a.time_base().toString()));
}

bool FootageDescription::Load(const QString &filename)
{
    *this = FootageDescription();

    QFile file(filename);
    if (file.open(QFile::ReadOnly)) {
        QXmlStreamReader reader(&file);

        while (XMLReadNextStartElement(&reader)) {
            if (reader.name() == QStringLiteral("streamcache")) {
                unsigned version = 1;
                {
                    XMLAttributeLoop((&reader), attr) {
                        if (attr.name() == QStringLiteral("version")) {
                            version = attr.value().toUInt();
                        }
                    }
                }

                if (version != kFootageMetaVersion) {
                    return false;
                }

                while (XMLReadNextStartElement(&reader)) {
                    if (reader.name() == QStringLiteral("decoder")) {
                        decoder_ = reader.readElementText();
                    } else if (reader.name() == QStringLiteral("streams")) {
                        {
                            XMLAttributeLoop((&reader), attr) {
                                if (attr.name() == QStringLiteral("count")) {
                                    total_stream_count_ = attr.value().toInt();
                                }
                            }
                        }

                        while (XMLReadNextStartElement(&reader)) {
                            if (reader.name() == QStringLiteral("video")) {
                                VideoParams vp;
                                vp.Load(&reader);
                                AddVideoStream(vp);
                            } else if (reader.name() == QStringLiteral("audio")) {
                                AudioParams ap = LoadAudioParams(&reader);
                                AddAudioStream(ap);
                            } else if (reader.name() == QStringLiteral("subtitle")) {
                                SubtitleParams sp;
                                sp.Load(&reader);
                                AddSubtitleStream(sp);
                            } else {
                                reader.skipCurrentElement();
                            }
                        }
                    } else {
                        reader.skipCurrentElement();
                    }
                }
            } else {
                reader.skipCurrentElement();
            }
        }

        file.close();

        if (reader.hasError()) {
            qWarning() << "Failed to load footage description for" << filename
                       << reader.errorString();
        } else {
            return true;
        }
    }

    return false;
}

bool FootageDescription::Save(const QString &filename) const
{
    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        return false;
    }

    QXmlStreamWriter writer(&file);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("streamcache"));
    writer.writeAttribute(QStringLiteral("version"),
                          QString::number(kFootageMetaVersion));
    writer.writeTextElement(QStringLiteral("decoder"), decoder_);
    writer.writeStartElement(QStringLiteral("streams"));
    writer.writeAttribute(QStringLiteral("count"),
                          QString::number(total_stream_count_));

    foreach (const VideoParams &vp, video_streams_) {
        writer.writeStartElement(QStringLiteral("video"));
        vp.Save(&writer);
        writer.writeEndElement();
    }

    foreach (const AudioParams &ap, audio_streams_) {
        writer.writeStartElement(QStringLiteral("audio"));
        SaveAudioParams(&writer, ap);
        writer.writeEndElement();
    }

    foreach (const SubtitleParams &sp, subtitle_streams_) {
        writer.writeStartElement(QStringLiteral("subtitle"));
        sp.Save(&writer);
        writer.writeEndElement();
    }

    writer.writeEndElement(); // streams
    writer.writeEndElement(); // streamcache
    writer.writeEndDocument();
    file.close();

    return true;
}

} // namespace olive
