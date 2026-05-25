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

#include "audioprocessor.h"

#include <QDebug>

#include "runtime/oak_audio_runtime.h"

namespace olive
{

namespace {

void ConvertInterleavedToBuffer(const float* interleaved,
								int64_t samples_per_channel,
								int channels,
								const AudioParams& to_params,
								AudioProcessor::Buffer* output)
{
	int bytes_per_sample = static_cast<int>(sizeof(float));

	if (to_params.format().is_packed()) {
		output->resize(1);
		(*output)[0] = QByteArray(
			reinterpret_cast<const char*>(interleaved),
			static_cast<int>(samples_per_channel * channels * bytes_per_sample));
	} else {
		output->resize(channels);
		for (int c = 0; c < channels; c++) {
			QByteArray& ch_data = (*output)[c];
			ch_data.resize(static_cast<int>(samples_per_channel * bytes_per_sample));
			float* dst = reinterpret_cast<float*>(ch_data.data());
			for (int64_t s = 0; s < samples_per_channel; s++) {
				dst[s] = interleaved[s * channels + c];
			}
		}
	}
}

} // anonymous namespace

AudioProcessor::AudioProcessor()
	: handle_(nullptr)
	, flush_channels_(0)
{
}

AudioProcessor::~AudioProcessor()
{
	Close();
}

bool AudioProcessor::Open(const AudioParams &from, const AudioParams &to,
						  double tempo)
{
	auto rt = OakAudioRuntime::Instance();
	if (!rt->Load()) {
		qWarning() << "Failed to load oakaudio runtime";
		return false;
	}

	OakAudioParams from_p;
	from_p.sample_rate = from.sample_rate();
	from_p.channels = from.channel_count();
	from_p.sample_fmt = OAK_AUDIO_FMT_FLT;
	from_p.channel_layout_mask = from.channel_layout().u.mask;

	OakAudioParams to_p;
	to_p.sample_rate = to.sample_rate();
	to_p.channels = to.channel_count();
	to_p.sample_fmt = OAK_AUDIO_FMT_FLT;
	to_p.channel_layout_mask = to.channel_layout().u.mask;

	handle_ = rt->filter_graph_create(&from_p, &to_p, tempo);
	if (!handle_) {
		qWarning() << "Failed to create audio filter graph";
		return false;
	}

	from_ = from;
	to_ = to;
	return true;
}

void AudioProcessor::Close()
{
	auto rt = OakAudioRuntime::Instance();
	if (handle_) {
		rt->filter_graph_destroy(static_cast<OakAudioFilterGraphHandle>(handle_));
		handle_ = nullptr;
	}
	flush_buffer_.clear();
	flush_channels_ = 0;
}

void AudioProcessor::Flush()
{
	auto rt = OakAudioRuntime::Instance();
	if (!handle_) return;

	float* out_data = nullptr;
	int64_t out_samples = 0;
	int out_channels = 0;

	int ret = rt->filter_graph_flush(static_cast<OakAudioFilterGraphHandle>(handle_), &out_data, &out_samples, &out_channels);
	if (ret == 0 && out_data && out_samples > 0) {
		int64_t total_bytes = out_samples * out_channels * static_cast<int>(sizeof(float));
		flush_buffer_ = QByteArray(
			reinterpret_cast<const char*>(out_data),
			static_cast<int>(total_bytes));
		flush_channels_ = out_channels;
		rt->filter_graph_free_output(out_data);
	}
}

int AudioProcessor::Convert(float **in, int nb_in_samples,
							AudioProcessor::Buffer *output)
{
	auto rt = OakAudioRuntime::Instance();
	if (!handle_) {
		qCritical() << "Tried to convert on closed processor";
		return -1;
	}

	if (in && nb_in_samples) {
		int ret = rt->filter_graph_process(
			static_cast<OakAudioFilterGraphHandle>(handle_),
			const_cast<const float**>(in),
			nb_in_samples,
			nullptr, nullptr, nullptr);
		if (ret < 0) {
			qCritical() << "Failed to process audio filter graph:" << ret;
			return ret;
		}
	}

	if (!output) return 0;

	// Return any pending flush data first
	if (!flush_buffer_.isEmpty()) {
		const float* data = reinterpret_cast<const float*>(flush_buffer_.constData());
		int64_t samples = flush_buffer_.size()
			/ (flush_channels_ * static_cast<int>(sizeof(float)));
		ConvertInterleavedToBuffer(data, samples, flush_channels_, to_, output);
		flush_buffer_.clear();
		return 0;
	}

	float* out_data = nullptr;
	int64_t out_samples = 0;
	int out_channels = 0;

	int ret = rt->filter_graph_process(
		static_cast<OakAudioFilterGraphHandle>(handle_), nullptr, 0,
		&out_data, &out_samples, &out_channels);
	if (ret < 0) {
		qCritical() << "Failed to pull from audio filter graph:" << ret;
		return ret;
	}

	if (out_data && out_samples > 0) {
		ConvertInterleavedToBuffer(out_data, out_samples, out_channels, to_, output);
		rt->filter_graph_free_output(out_data);
	} else {
		output->clear();
	}

	return 0;
}

} // namespace olive
