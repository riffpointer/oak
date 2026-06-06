/***

  Olive - Non-Linear Video Editor
  Copyright (C) 2022 Olive Team
  Modifications Copyright (C) 2026 mikesolar

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

#include "vectorscope.h"

#include <QPainter>
#include <QtMath>
#include <QVector2D>
#include <QVector3D>

#include "common/qtutils.h"
#include "node/node.h"

namespace olive
{

#define super ScopeBase

VectorscopeScope::VectorscopeScope(QWidget *parent)
	: super(parent)
{
}

ShaderCode VectorscopeScope::GenerateShaderCode()
{
	return ShaderCode(
		FileFunctions::ReadFileAsString(":/shaders/rgbvectorscope.frag"),
		FileFunctions::ReadFileAsString(":/shaders/rgbvectorscope.vert"));
}

void VectorscopeScope::DrawScope(TexturePtr managed_tex, QVariant pipeline)
{
	float vectorscope_scale = 0.80f;
	float vectorscope_gain = 1.45f;
	float vectorscope_point_radius = 1.75f;
	float vectorscope_intensity = 0.035f;
	float vectorscope_sample_grid = 28.0f;

	ShaderJob job;

	job.Insert(QStringLiteral("viewport"),
			   NodeValue(NodeValue::kVec2, QVector2D(width(), height())));

	double luma_coeffs[3] = { 0.0f, 0.0f, 0.0f };
	color_manager()->GetDefaultLumaCoefs(luma_coeffs);
	job.Insert(
		QStringLiteral("luma_coeffs"),
		NodeValue(NodeValue::kVec3,
				  QVector3D(luma_coeffs[0], luma_coeffs[1], luma_coeffs[2])));

	job.Insert(QStringLiteral("vectorscope_scale"),
			   NodeValue(NodeValue::kFloat, vectorscope_scale));
	job.Insert(QStringLiteral("vectorscope_gain"),
			   NodeValue(NodeValue::kFloat, vectorscope_gain));
	job.Insert(QStringLiteral("vectorscope_point_radius"),
			   NodeValue(NodeValue::kFloat, vectorscope_point_radius));
	job.Insert(QStringLiteral("vectorscope_intensity"),
			   NodeValue(NodeValue::kFloat, vectorscope_intensity));
	job.Insert(QStringLiteral("vectorscope_sample_grid"),
			   NodeValue(NodeValue::kFloat, vectorscope_sample_grid));

	job.Insert(QStringLiteral("ove_maintex"),
			   NodeValue(NodeValue::kTexture,
						 QVariant::fromValue(managed_tex)));

	renderer()->Blit(pipeline, job, GetViewportParams());

	QPainter p(paint_device());
	QFont font = p.font();
	font.setPixelSize(10);
	QFontMetrics font_metrics = QFontMetrics(font);

	p.setCompositionMode(QPainter::CompositionMode_Plus);
	p.setPen(QColor(0, 153, 0));
	p.setFont(font);

	float scope_size = qMin(width(), height()) * vectorscope_scale;
	QPointF center(width() * 0.5, height() * 0.5);
	float radius = scope_size * 0.5;

	p.drawEllipse(center, radius, radius);
	p.drawLine(QPointF(center.x() - radius, center.y()),
			   QPointF(center.x() + radius, center.y()));
	p.drawLine(QPointF(center.x(), center.y() - radius),
			   QPointF(center.x(), center.y() + radius));

	struct Target {
		const char *label;
		float angle;
	};
	const Target targets[] = {
		{ "R", 0.0f },	  { "Mg", 60.0f }, { "B", 120.0f },
		{ "Cy", 180.0f }, { "G", 240.0f }, { "Yl", 300.0f },
	};

	const float label_radius = radius + 12.0f;
	const float marker_radius = radius * 0.72f;
	constexpr float kPi = 3.14159265358979323846f;

	for (const Target &target : targets) {
		float radians = target.angle * kPi / 180.0f;
		QPointF direction(qCos(radians), -qSin(radians));
		QPointF marker = center + direction * marker_radius;
		QPointF label_pos = center + direction * label_radius;
		QString label = QString::fromUtf8(target.label);

		p.drawEllipse(marker, 3.0, 3.0);
		p.drawText(label_pos.x() -
					   QtUtils::QFontMetricsWidth(font_metrics, label) * 0.5,
				   label_pos.y() + font_metrics.capHeight() * 0.5, label);
	}
}

}
