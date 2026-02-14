/***

  Oak Video Editor - Non-Linear Video Editor
  Copyright (C) 2026 Oak Team

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
#include "framecache.h"
using namespace olive::cache;
size_t FrameCacheEntry::size(){
	int byte_per_channel = texture_->format().byte_count();
	int width = texture_->width();
	int height = texture_->height();
	int channel_count = texture_->channel_count();
	int overhead_factor = 1.3; // std::shared_ptr, metadata
	return byte_per_channel * width * height * channel_count * overhead_factor;
}
size_t qHash(const FrameCacheKey &key, size_t seed)
{
	const olive::VideoParams params = key.params();
	return qHashMulti(
		seed, key.version(), key.time().toDouble(), params.width(),
		params.height(), params.depth(), params.time_base().numerator(),
		params.time_base().denominator(), static_cast<int>(params.format()),
		params.channel_count(), params.pixel_aspect_ratio().numerator(),
		params.pixel_aspect_ratio().denominator(),
		static_cast<int>(params.interlacing()), params.divider(),
		params.effective_width(), params.effective_height(),
		params.effective_depth(), params.square_pixel_width(), params.enabled(),
		params.stream_index(), static_cast<int>(params.video_type()),
		params.frame_rate().numerator(), params.frame_rate().denominator(),
		params.start_time(), params.duration(), params.premultiplied_alpha(),
		params.colorspace(), params.x(), params.y(),
		static_cast<int>(params.color_range()), key.path(), key.uuid());
}