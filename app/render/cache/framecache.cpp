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