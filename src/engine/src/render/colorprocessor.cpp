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

#include "colorprocessor.h"

#include "olive/common/define.h"
#include "node/color/colormanager/colormanager.h"
#include "runtime/oak_color_runtime.h"

namespace olive
{

ColorProcessor::ColorProcessor(ColorManager *config, const QString &input,
							   const ColorTransform &transform,
							   Direction direction)
{
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load()) {
		processor_handle_ = nullptr;
		return;
	}

	const QString &output = (transform.output().isEmpty()) ?
								config->GetDefaultDisplay() :
								transform.output();

	int dir = (direction == kNormal) ? 0 : 1;

	if (transform.is_display()) {
		const QString &view = (transform.view().isEmpty()) ?
								  config->GetDefaultView(output) :
								  transform.view();

		processor_handle_ = rt->processor_create_display(
			static_cast<OakColorConfigHandle>(config->GetConfigHandle()),
			input.toUtf8().constData(),
			output.toUtf8().constData(),
			view.toUtf8().constData(),
			transform.look().isEmpty() ? nullptr : transform.look().toUtf8().constData(),
			dir);
	} else {
		processor_handle_ = rt->processor_create_transform(
			static_cast<OakColorConfigHandle>(config->GetConfigHandle()),
			input.toUtf8().constData(),
			output.toUtf8().constData(),
			dir);
	}
}

ColorProcessor::ColorProcessor(void* oak_processor_handle)
{
	processor_handle_ = oak_processor_handle;
}

void ColorProcessor::ConvertFrame(Frame *f)
{
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load() || !processor_handle_) {
		qCritical() << "Tried to color convert frame with no processor";
		return;
	}

	// Allocate output buffer
	int w = f->width();
	int h = f->height();
	int channels = f->channel_count();
	int pix_layout = channels;

	if (f->format() == PixelFormat::F32) {
		// In-place processing for float format
		rt->processor_apply(static_cast<OakColorProcessorHandle>(processor_handle_), w, h,
							reinterpret_cast<const float*>(f->data()),
							reinterpret_cast<float*>(f->data()),
							pix_layout);
	} else {
		// For other formats, convert to float first (simplified path)
		std::vector<float> float_buf(w * h * channels);
		// TODO: implement pixel format conversion to float RGBA
		qCritical() << "Color conversion for non-float formats not yet implemented";
	}
}

Color ColorProcessor::ConvertColor(const Color &in)
{
	auto rt = OakColorRuntime::Instance();
	if (!rt->Load() || !processor_handle_) {
		return in;
	}

	float c_in[4] = { float(in.red()), float(in.green()), float(in.blue()),
					  float(in.alpha()) };
	float c_out[4];

	rt->processor_apply_pixel(static_cast<OakColorProcessorHandle>(processor_handle_), c_in, c_out);

	return Color(c_out[0], c_out[1], c_out[2], c_out[3]);
}

ColorProcessorPtr ColorProcessor::Create(ColorManager *config,
										 const QString &input,
										 const ColorTransform &transform,
										 Direction direction)
{
	return std::make_shared<ColorProcessor>(config, input, transform, direction);
}

ColorProcessorPtr ColorProcessor::Create(void* oak_processor_handle)
{
	return std::make_shared<ColorProcessor>(oak_processor_handle);
}

void* ColorProcessor::GetProcessorHandle()
{
	return processor_handle_;
}

const char *ColorProcessor::id() const
{
	// Return a stable identifier based on the handle pointer
	// (oakcolor.so does not expose cacheID via C API currently)
	static char buf[32];
	snprintf(buf, sizeof(buf), "%p", processor_handle_);
	return buf;
}

void ColorProcessor::ConvertFrame(FramePtr f)
{
	ConvertFrame(f.get());
}

}
