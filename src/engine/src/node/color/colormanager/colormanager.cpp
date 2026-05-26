/***

  Olive - Non-Linear Video Editor
  Copyright (C) 2022 Olive Team
  Modifications Copyright (C) 2025 mikesolar

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

***/

#include "colormanager.h"

#include <QDir>
#include <QStandardPaths>

#include "olive/common/define.h"
#include "olive/common/filefunctions.h"
#include "config/config.h"
#include "node/project.h"
#include "runtime/oak_color_runtime.h"

namespace olive
{

#define super Node

void* ColorManager::default_config_ = nullptr;

ColorManager::ColorManager(Project *project)
	: QObject(project)
	, config_(nullptr)
{
}

void ColorManager::Init()
{
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load()) return;

	// Set config to our built-in default
	config_ = GetDefaultConfigHandle();
	const char* default_cs = rt->config_default_input_space(static_cast<OakColorConfigHandle>(config_));
	if (default_cs) {
		SetDefaultInputColorSpace(QString::fromUtf8(default_cs));
	}
	project()->SetColorReferenceSpace(QStringLiteral("scene_linear"));
}

void* ColorManager::GetConfigHandle() const
{
	return config_;
}

void* ColorManager::CreateConfigFromFile(const QString &filename)
{
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load()) return nullptr;
	return rt->config_load(filename.toUtf8().constData());
}

QString ColorManager::GetConfigFilename() const
{
	return project()->GetColorConfigFilename();
}

void* ColorManager::GetDefaultConfigHandle()
{
	return default_config_;
}

void ColorManager::SetUpDefaultConfig()
{
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load()) return;

	if (!qEnvironmentVariableIsEmpty("OCIO")) {
		// Attempt to set config from "OCIO" environment variable
		default_config_ = rt->config_load(nullptr);
		if (default_config_) {
			return;
		}
		qWarning() << "Failed to load config from OCIO environment variable";
	}

	// Extract OCIO config - kind of hacky, but it'll work
	QString dir =
		QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
			.filePath(QStringLiteral("ocioconf"));

	FileFunctions::CopyDirectory(QStringLiteral(":/ocioconf"), dir, true);

	qDebug() << "Extracting default OCIO config to" << dir;

	default_config_ =
		CreateConfigFromFile(QDir(dir).filePath(QStringLiteral("config.ocio")));
}

void ColorManager::SetConfigFilename(const QString &filename)
{
	project()->SetColorConfigFilename(filename);
}

QStringList ColorManager::ListAvailableDisplays()
{
	QStringList displays;
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load() || !config_) return displays;

	// oakcolor.so C API does not expose display enumeration directly.
	// For now, return empty list or query via default display.
	// TODO: extend oakcolor.so C API with display enumeration if needed.
	QString def = GetDefaultDisplay();
	if (!def.isEmpty()) {
		displays.append(def);
	}
	return displays;
}

QString ColorManager::GetDefaultDisplay()
{
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load() || !config_) return QString();
	const char* d = rt->config_default_display(static_cast<OakColorConfigHandle>(config_));
	return d ? QString::fromUtf8(d) : QString();
}

QStringList ColorManager::ListAvailableViews(QString display)
{
	QStringList views;
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load() || !config_) return views;

	int n = rt->config_display_view_count(static_cast<OakColorConfigHandle>(config_), display.toUtf8().constData());
	for (int i = 0; i < n; i++) {
		const char* v = rt->config_display_view_name(static_cast<OakColorConfigHandle>(config_), display.toUtf8().constData(), i);
		if (v) views.append(QString::fromUtf8(v));
	}
	return views;
}

QString ColorManager::GetDefaultView(const QString &display)
{
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load() || !config_) return QString();

	// Return first view as default
	const char* v = rt->config_display_view_name(static_cast<OakColorConfigHandle>(config_), display.toUtf8().constData(), 0);
	return v ? QString::fromUtf8(v) : QString();
}

QStringList ColorManager::ListAvailableLooks()
{
	// oakcolor.so C API does not expose looks enumeration.
	// TODO: extend C API if needed.
	return QStringList();
}

QStringList ColorManager::ListAvailableColorspaces() const
{
	return ListAvailableColorspaces(config_);
}

QString ColorManager::GetDefaultInputColorSpace() const
{
	return project()->GetDefaultInputColorSpace();
}

void ColorManager::SetDefaultInputColorSpace(const QString &s)
{
	project()->SetDefaultInputColorSpace(s);
}

QString ColorManager::GetReferenceColorSpace() const
{
	return project()->GetColorReferenceSpace();
}

QString ColorManager::GetCompliantColorSpace(const QString &s)
{
	if (ListAvailableColorspaces().contains(s)) {
		return s;
	} else {
		return GetDefaultInputColorSpace();
	}
}

ColorTransform
ColorManager::GetCompliantColorSpace(const ColorTransform &transform,
									 bool force_display)
{
	if (transform.is_display() || force_display) {
		// Get display information
		QString display = transform.display();
		QString view = transform.view();
		QString look = transform.look();

		// Check if display still exists in config
		if (!ListAvailableDisplays().contains(display)) {
			display = GetDefaultDisplay();
		}

		// Check if view still exists in display
		if (!ListAvailableViews(display).contains(view)) {
			view = GetDefaultView(display);
		}

		// Check if looks still exists
		if (!ListAvailableLooks().contains(look)) {
			look.clear();
		}

		return ColorTransform(display, view, look);

	} else {
		QString output = transform.output();

		if (!ListAvailableColorspaces().contains(output)) {
			output = GetDefaultInputColorSpace();
		}

		return ColorTransform(output);
	}
}

QStringList
ColorManager::ListAvailableColorspaces(void* config_handle)
{
	QStringList spaces;
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load() || !config_handle) return spaces;

	int n = rt->config_space_count(static_cast<OakColorConfigHandle>(config_handle));
	for (int i = 0; i < n; i++) {
		const char* name = rt->config_space_name(static_cast<OakColorConfigHandle>(config_handle), i);
		if (name) spaces.append(QString::fromUtf8(name));
	}
	return spaces;
}

void ColorManager::GetDefaultLumaCoefs(double *rgb) const
{
	// oakcolor.so C API does not expose luma coefficients.
	// Fallback to Rec.709 defaults.
	rgb[0] = 0.2126;
	rgb[1] = 0.7152;
	rgb[2] = 0.0722;
}

Project *ColorManager::project() const
{
	return static_cast<Project *>(parent());
}

void ColorManager::UpdateConfigFromFilename()
{
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load()) return;

	QString config_filename = GetConfigFilename();
	QString old_default_cs = GetDefaultInputColorSpace();

	if (config_) {
		rt->config_free(static_cast<OakColorConfigHandle>(config_));
	}
	config_ = CreateConfigFromFile(config_filename);

	// Set new default colorspace appropriately
	QString new_default = old_default_cs;
	QStringList available_cs = ListAvailableColorspaces();
	for (int i = 0; i < available_cs.size(); i++) {
		const QString &c = available_cs.at(i);
		if (c.compare(old_default_cs, Qt::CaseInsensitive)) {
			new_default = c;
			break;
		}
	}
	SetDefaultInputColorSpace(new_default);

	emit ConfigChanged(config_filename);
}

}
