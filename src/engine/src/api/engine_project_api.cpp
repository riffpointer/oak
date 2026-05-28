/*
 *  oakengine.so C API — Project Operations
 *  Create, modify, save, and destroy Project instances.
 */

#include "oak/engine_api.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <cstring>

#include "node/project.h"
#include "node/serializeddata.h"

namespace {

static thread_local std::string g_project_string_buffer;

} // namespace

extern "C" {

OakEngineProjectHandle oak_project_create(void)
{
  return reinterpret_cast<OakEngineProjectHandle>(new olive::Project());
}

void oak_project_destroy(OakEngineProjectHandle h)
{
  if (!h) return;
  auto* p = reinterpret_cast<olive::Project*>(h);
  p->Clear();
  delete p;
}

void oak_project_initialize(OakEngineProjectHandle h)
{
  if (!h) return;
  reinterpret_cast<olive::Project*>(h)->Initialize();
}

void oak_project_clear(OakEngineProjectHandle h)
{
  if (!h) return;
  reinterpret_cast<olive::Project*>(h)->Clear();
}

void oak_project_set_filename(OakEngineProjectHandle h, const char* filename)
{
  if (!h) return;
  reinterpret_cast<olive::Project*>(h)->set_filename(QString::fromUtf8(filename));
}

const char* oak_project_filename(OakEngineProjectHandle h)
{
  if (!h) return "";
  g_project_string_buffer = reinterpret_cast<olive::Project*>(h)->filename().toStdString();
  return g_project_string_buffer.c_str();
}

void oak_project_set_modified(OakEngineProjectHandle h, bool modified)
{
  if (!h) return;
  reinterpret_cast<olive::Project*>(h)->set_modified(modified);
}

bool oak_project_is_modified(OakEngineProjectHandle h)
{
  if (!h) return false;
  return reinterpret_cast<olive::Project*>(h)->is_modified();
}

void oak_project_regenerate_uuid(OakEngineProjectHandle h)
{
  if (!h) return;
  reinterpret_cast<olive::Project*>(h)->RegenerateUuid();
}

const char* oak_project_get_saved_url(OakEngineProjectHandle h)
{
  if (!h) return "";
  g_project_string_buffer = reinterpret_cast<olive::Project*>(h)->GetSavedURL().toStdString();
  return g_project_string_buffer.c_str();
}

int oak_project_save(OakEngineProjectHandle h, const char* filename, bool compress, char** out_error)
{
  if (!h) return -1;

  auto* project = reinterpret_cast<olive::Project*>(h);
  QString fn = QString::fromUtf8(filename);

  QFile file(fn);
  if (!file.open(QFile::WriteOnly)) {
    if (out_error) {
      QByteArray err = QStringLiteral("Failed to open file for writing").toUtf8();
      *out_error = reinterpret_cast<char*>(malloc(err.size() + 1));
      memcpy(*out_error, err.constData(), err.size() + 1);
    }
    return -1;
  }

  if (compress) {
    QBuffer xml_buffer;
    xml_buffer.open(QBuffer::WriteOnly);
    QXmlStreamWriter writer(&xml_buffer);
    project->Save(&writer);
    writer.writeEndDocument();
    xml_buffer.close();

    QByteArray compressed = qCompress(xml_buffer.data());
    file.write(compressed);
  } else {
    QXmlStreamWriter writer(&file);
    project->Save(&writer);
  }

  file.close();
  project->set_filename(fn);
  project->set_modified(false);
  return 0;
}

void oak_string_free(const char* s)
{
  free(const_cast<char*>(s));
}

} // extern "C"
