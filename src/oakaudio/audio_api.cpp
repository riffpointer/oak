/***

  oakaudio.so C API Implementation (v1)
  Copyright (C) 2025 mikesolar

  Audio processing: resampling (FFmpeg swresample), mixing, format conversion.
  Zero Qt dependency. Only links FFmpeg::swresample.

***/

#include "oak/audio_api.h"
#include "oakaudio_internal.h"

#include <cmath>
#include <algorithm>

/* ------------------------------------------------------------------ */
/*  Audio buffer                                                        */
/* ------------------------------------------------------------------ */

OakAudioBufferHandle oak_audio_buffer_create(int channels, int64_t samples,
                                             int sample_rate, bool interleaved)
{
    if (channels <= 0 || samples <= 0 || sample_rate <= 0) return nullptr;
    auto* buf = new OakAudioBuffer();
    buf->params.channels = channels;
    buf->params.duration_samples = samples;
    buf->params.sample_rate = sample_rate;
    buf->params.sample_fmt = OAK_AUDIO_FMT_FLT;
    AVChannelLayout layout;
    av_channel_layout_default(&layout, channels);
    buf->params.channel_layout_mask = layout.u.mask;
    av_channel_layout_uninit(&layout);
    buf->interleaved = interleaved;
    buf->data.resize(static_cast<size_t>(channels) * static_cast<size_t>(samples), 0.0f);

    if (!interleaved) {
        buf->planar_ptrs.resize(channels);
        for (int c = 0; c < channels; c++) {
            buf->planar_ptrs[c] = buf->data.data() + c * samples;
        }
    }
    return buf;
}

void oak_audio_buffer_free(OakAudioBufferHandle buf)
{
    delete buf;
}

void oak_audio_buffer_params(OakAudioBufferHandle buf, OakAudioParams* out_params)
{
    if (buf && out_params) {
        *out_params = buf->params;
    }
}

void oak_audio_buffer_data(OakAudioBufferHandle buf, void** out_data, bool* out_interleaved)
{
    if (!buf) return;
    if (out_data) {
        if (buf->interleaved) {
            *out_data = buf->data.data();
        } else {
            *out_data = buf->planar_ptrs.data();
        }
    }
    if (out_interleaved) *out_interleaved = buf->interleaved;
}

OakAudioBufferHandle oak_audio_buffer_clone(OakAudioBufferHandle buf)
{
    if (!buf) return nullptr;
    auto* clone = new OakAudioBuffer(*buf);
    if (!buf->interleaved) {
        clone->planar_ptrs.resize(buf->params.channels);
        for (int c = 0; c < buf->params.channels; c++) {
            clone->planar_ptrs[c] = clone->data.data() + c * buf->params.duration_samples;
        }
    }
    return clone;
}

/* ------------------------------------------------------------------ */
/*  Resampler (FFmpeg swresample)                                       */
/* ------------------------------------------------------------------ */

OakAudioResamplerHandle oak_audio_resampler_create(int src_channels, int src_sample_rate,
                                                   int dst_channels, int dst_sample_rate)
{
    if (src_channels <= 0 || src_sample_rate <= 0 ||
        dst_channels <= 0 || dst_sample_rate <= 0) {
        return nullptr;
    }

    AVChannelLayout in_layout, out_layout;
    av_channel_layout_default(&in_layout, src_channels);
    av_channel_layout_default(&out_layout, dst_channels);

    SwrContext* swr = nullptr;
    int ret = swr_alloc_set_opts2(&swr,
        &out_layout, AV_SAMPLE_FMT_FLT, dst_sample_rate,
        &in_layout, AV_SAMPLE_FMT_FLT, src_sample_rate,
        0, nullptr);

    av_channel_layout_uninit(&in_layout);
    av_channel_layout_uninit(&out_layout);

    if (ret < 0 || !swr || swr_init(swr) < 0) {
        swr_free(&swr);
        return nullptr;
    }

    auto* r = new OakAudioResampler();
    r->src_channels = src_channels;
    r->src_sample_rate = src_sample_rate;
    r->dst_channels = dst_channels;
    r->dst_sample_rate = dst_sample_rate;
    r->swr = swr;
    return r;
}

void oak_audio_resampler_free(OakAudioResamplerHandle resampler)
{
    if (!resampler) return;
    if (resampler->swr) {
        swr_free(&resampler->swr);
    }
    delete resampler;
}

int oak_audio_resampler_process(OakAudioResamplerHandle resampler,
                                const float* in_data, int64_t in_samples,
                                float* out_data, int64_t out_samples_capacity,
                                int64_t* out_actual_samples)
{
    if (!resampler || !resampler->swr || !in_data || !out_data || in_samples <= 0) return -1;

    const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(in_data);
    uint8_t* out_ptr = reinterpret_cast<uint8_t*>(out_data);

    int ret = swr_convert(resampler->swr,
                          &out_ptr, static_cast<int>(out_samples_capacity),
                          &in_ptr, static_cast<int>(in_samples));
    if (ret < 0) {
        if (out_actual_samples) *out_actual_samples = 0;
        return -1;
    }

    if (out_actual_samples) *out_actual_samples = ret;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Mixer                                                               */
/* ------------------------------------------------------------------ */

OakAudioMixerHandle oak_audio_mixer_create(int channels, int sample_rate)
{
    if (channels <= 0 || sample_rate <= 0) return nullptr;
    auto* m = new OakAudioMixer();
    m->channels = channels;
    m->sample_rate = sample_rate;
    return m;
}

void oak_audio_mixer_free(OakAudioMixerHandle mixer)
{
    delete mixer;
}

void oak_audio_mixer_add_source(OakAudioMixerHandle mixer,
                                OakAudioBufferHandle buf,
                                int64_t start_sample,
                                double volume,
                                double pan)
{
    if (!mixer || !buf) return;
    OakAudioMixerSource src;
    src.buf = buf;
    src.start_sample = start_sample;
    src.volume = std::clamp(volume, 0.0, 10.0);
    src.pan = std::clamp(pan, -1.0, 1.0);
    mixer->sources.push_back(src);
}

void oak_audio_mixer_clear_sources(OakAudioMixerHandle mixer)
{
    if (!mixer) return;
    mixer->sources.clear();
}

static inline float pan_gain(int channel, int channels, double pan)
{
    if (channels == 1) return 1.0f;
    if (channels == 2) {
        if (channel == 0) return static_cast<float>(std::clamp(1.0 - pan, 0.0, 1.0));
        if (channel == 1) return static_cast<float>(std::clamp(1.0 + pan, 0.0, 1.0));
    }
    // For >2 channels, pan only affects first two
    if (channel == 0) return static_cast<float>(std::clamp(1.0 - pan, 0.0, 1.0));
    if (channel == 1) return static_cast<float>(std::clamp(1.0 + pan, 0.0, 1.0));
    return 1.0f;
}

int oak_audio_mixer_mix(OakAudioMixerHandle mixer,
                        int64_t start_sample, int64_t sample_count,
                        float* out_data)
{
    if (!mixer || !out_data || sample_count <= 0) return -1;

    int ch = mixer->channels;
    size_t total = static_cast<size_t>(ch) * static_cast<size_t>(sample_count);
    std::memset(out_data, 0, total * sizeof(float));

    for (const auto& src : mixer->sources) {
        if (!src.buf) continue;
        const OakAudioBuffer* buf = src.buf;
        int buf_ch = buf->params.channels;
        int64_t buf_samples = buf->params.duration_samples;

        int64_t src_start = src.start_sample;
        int64_t mix_start = start_sample;
        int64_t offset = mix_start - src_start;

        if (offset >= buf_samples) continue;
        if (-offset >= sample_count) continue;

        int64_t s_off = std::max<int64_t>(0, offset);
        int64_t d_off = std::max<int64_t>(0, -offset);
        int64_t count = std::min(sample_count - d_off, buf_samples - s_off);
        if (count <= 0) continue;

        if (buf->interleaved) {
            const float* src_data = buf->data.data();
            for (int64_t s = 0; s < count; s++) {
                for (int c = 0; c < ch; c++) {
                    int src_c = (c < buf_ch) ? c : 0;
                    float gain = static_cast<float>(src.volume) * pan_gain(c, ch, src.pan);
                    float sample = src_data[(s_off + s) * buf_ch + src_c] * gain;
                    out_data[(d_off + s) * ch + c] += sample;
                }
            }
        } else {
            const float* const* src_data = reinterpret_cast<const float* const*>(buf->planar_ptrs.data());
            for (int64_t s = 0; s < count; s++) {
                for (int c = 0; c < ch; c++) {
                    int src_c = (c < buf_ch) ? c : 0;
                    float gain = static_cast<float>(src.volume) * pan_gain(c, ch, src.pan);
                    float sample = src_data[src_c][s_off + s] * gain;
                    out_data[(d_off + s) * ch + c] += sample;
                }
            }
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Utility: interleaved <-> planar + channel remap                     */
/* ------------------------------------------------------------------ */

int oak_audio_convert_layout(const float* in_data, int in_channels, int64_t in_samples, bool in_interleaved,
                             float* out_data, int out_channels, int64_t out_samples, bool out_interleaved)
{
    if (!in_data || !out_data || in_channels <= 0 || out_channels <= 0 ||
        in_samples <= 0 || out_samples <= 0) {
        return -1;
    }

    int64_t count = std::min(in_samples, out_samples);

    if (in_interleaved && out_interleaved) {
        // Interleaved → interleaved (possible channel remap)
        for (int64_t s = 0; s < count; s++) {
            for (int c = 0; c < out_channels; c++) {
                int src_c = (c < in_channels) ? c : 0;
                out_data[s * out_channels + c] = in_data[s * in_channels + src_c];
            }
        }
    } else if (!in_interleaved && !out_interleaved) {
        // Planar → planar (possible channel remap)
        const float* const* in_planar = reinterpret_cast<const float* const*>(in_data);
        float** out_planar = reinterpret_cast<float**>(out_data);
        for (int c = 0; c < out_channels; c++) {
            int src_c = (c < in_channels) ? c : 0;
            const float* src_ptr = in_planar[src_c];
            float* dst_ptr = out_planar[c];
            for (int64_t s = 0; s < count; s++) {
                dst_ptr[s] = src_ptr[s];
            }
        }
    } else if (in_interleaved && !out_interleaved) {
        // Interleaved → planar
        float** out_planar = reinterpret_cast<float**>(out_data);
        for (int c = 0; c < out_channels; c++) {
            int src_c = (c < in_channels) ? c : 0;
            float* dst_ptr = out_planar[c];
            for (int64_t s = 0; s < count; s++) {
                dst_ptr[s] = in_data[s * in_channels + src_c];
            }
        }
    } else {
        // Planar → interleaved
        const float* const* in_planar = reinterpret_cast<const float* const*>(in_data);
        for (int64_t s = 0; s < count; s++) {
            for (int c = 0; c < out_channels; c++) {
                int src_c = (c < in_channels) ? c : 0;
                out_data[s * out_channels + c] = in_planar[src_c][s];
            }
        }
    }

    return 0;
}

/* ================================================================ */
/*  Audio Filter Graph (FFmpeg avfilter)                                */
/* ================================================================ */

static AVSampleFormat OakAudioFmtToAV(OakAudioFormat fmt)
{
    switch (fmt) {
    case OAK_AUDIO_FMT_U8:  return AV_SAMPLE_FMT_U8;
    case OAK_AUDIO_FMT_S16: return AV_SAMPLE_FMT_S16;
    case OAK_AUDIO_FMT_S32: return AV_SAMPLE_FMT_S32;
    case OAK_AUDIO_FMT_FLT: return AV_SAMPLE_FMT_FLT;
    case OAK_AUDIO_FMT_DBL: return AV_SAMPLE_FMT_DBL;
    default:                return AV_SAMPLE_FMT_NONE;
    }
}

static const char* AVSampleFmtNameShort(AVSampleFormat fmt)
{
    return av_get_sample_fmt_name(fmt);
}

OakAudioFilterGraphHandle oak_audio_filter_graph_create(const OakAudioParams* from,
                                                        const OakAudioParams* to,
                                                        double tempo)
{
    if (!from || !to || from->channels <= 0 || from->sample_rate <= 0 ||
        to->channels <= 0 || to->sample_rate <= 0) {
        return nullptr;
    }

    OakAudioFilterGraph* graph = new OakAudioFilterGraph();
    graph->from_params = *from;
    graph->to_params = *to;
    graph->from_avfmt = OakAudioFmtToAV(from->sample_fmt);
    graph->to_avfmt = OakAudioFmtToAV(to->sample_fmt);
    if (graph->from_avfmt == AV_SAMPLE_FMT_NONE) graph->from_avfmt = AV_SAMPLE_FMT_FLT;
    if (graph->to_avfmt == AV_SAMPLE_FMT_NONE)   graph->to_avfmt = AV_SAMPLE_FMT_FLT;

    graph->graph = avfilter_graph_alloc();
    if (!graph->graph) {
        delete graph;
        return nullptr;
    }

    char filter_args[512];
    snprintf(filter_args, sizeof(filter_args),
             "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
             from->sample_rate, from->sample_rate,
             AVSampleFmtNameShort(graph->from_avfmt),
             static_cast<uint64_t>(from->channel_layout_mask));

    int r = avfilter_graph_create_filter(&graph->buffersrc_ctx,
                                         avfilter_get_by_name("abuffer"), "in",
                                         filter_args, nullptr, graph->graph);
    if (r < 0) {
        avfilter_graph_free(&graph->graph);
        delete graph;
        return nullptr;
    }

    AVFilterContext* previous = graph->buffersrc_ctx;

    // Tempo filters
    if (std::abs(tempo - 1.0) > 0.0001) {
        double base = (tempo > 1.0) ? 2.0 : 0.5;
        double speed_log = std::log(tempo) / std::log(base);
        int whole = static_cast<int>(std::floor(speed_log));
        speed_log -= whole;

        for (int i = 0; i <= whole; i++) {
            double filter_tempo = (i == whole) ? std::pow(base, speed_log) : base;
            if (std::abs(filter_tempo - 1.0) < 0.0001) continue;

            char speed_param[64];
            snprintf(speed_param, sizeof(speed_param), "%f", filter_tempo);

            AVFilterContext* tempo_ctx = nullptr;
            r = avfilter_graph_create_filter(&tempo_ctx,
                                             avfilter_get_by_name("atempo"), "atempo",
                                             speed_param, nullptr, graph->graph);
            if (r < 0 || avfilter_link(previous, 0, tempo_ctx, 0) < 0) {
                avfilter_graph_free(&graph->graph);
                delete graph;
                return nullptr;
            }
            previous = tempo_ctx;
        }
    }

    // aformat filter if conversion needed
    if (from->sample_rate != to->sample_rate ||
        from->channels != to->channels ||
        graph->from_avfmt != graph->to_avfmt) {
        snprintf(filter_args, sizeof(filter_args),
                 "sample_fmts=%s:sample_rates=%d:channel_layouts=0x%" PRIx64,
                 AVSampleFmtNameShort(graph->to_avfmt),
                 to->sample_rate,
                 static_cast<uint64_t>(to->channel_layout_mask));

        AVFilterContext* fmt_ctx = nullptr;
        r = avfilter_graph_create_filter(&fmt_ctx,
                                         avfilter_get_by_name("aformat"), "fmt",
                                         filter_args, nullptr, graph->graph);
        if (r < 0 || avfilter_link(previous, 0, fmt_ctx, 0) < 0) {
            avfilter_graph_free(&graph->graph);
            delete graph;
            return nullptr;
        }
        previous = fmt_ctx;
    }

    r = avfilter_graph_create_filter(&graph->buffersink_ctx,
                                     avfilter_get_by_name("abuffersink"), "out",
                                     nullptr, nullptr, graph->graph);
    if (r < 0 || avfilter_link(previous, 0, graph->buffersink_ctx, 0) < 0) {
        avfilter_graph_free(&graph->graph);
        delete graph;
        return nullptr;
    }

    r = avfilter_graph_config(graph->graph, nullptr);
    if (r < 0) {
        avfilter_graph_free(&graph->graph);
        delete graph;
        return nullptr;
    }

    graph->in_frame = av_frame_alloc();
    graph->out_frame = av_frame_alloc();
    if (!graph->in_frame || !graph->out_frame) {
        avfilter_graph_free(&graph->graph);
        delete graph;
        return nullptr;
    }

    return graph;
}

void oak_audio_filter_graph_destroy(OakAudioFilterGraphHandle graph)
{
    if (!graph) return;
    if (graph->in_frame)  av_frame_free(&graph->in_frame);
    if (graph->out_frame) av_frame_free(&graph->out_frame);
    if (graph->graph)     avfilter_graph_free(&graph->graph);
    delete graph;
}

int oak_audio_filter_graph_process(OakAudioFilterGraphHandle graph,
                                   const float** in_data, int64_t in_samples,
                                   float** out_data, int64_t* out_samples,
                                   int* out_channels)
{
    if (!graph || !graph->graph || !graph->buffersrc_ctx || !graph->buffersink_ctx) return -1;

    if (in_data && in_samples > 0) {
        AVFrame* frame = graph->in_frame;
        av_frame_unref(frame);
        frame->sample_rate = graph->from_params.sample_rate;
        frame->format = graph->from_avfmt;
        av_channel_layout_from_mask(&frame->ch_layout, graph->from_params.channel_layout_mask);
        frame->nb_samples = static_cast<int>(in_samples);
        frame->pts = 0;

        int ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) return -1;

        int ch = graph->from_params.channels;
        if (av_sample_fmt_is_planar(graph->from_avfmt)) {
            for (int c = 0; c < ch; c++) {
                std::memcpy(frame->data[c], in_data[c], in_samples * sizeof(float));
            }
        } else {
            std::memcpy(frame->data[0], in_data[0], in_samples * ch * sizeof(float));
        }

        ret = av_buffersrc_add_frame_flags(graph->buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        av_frame_unref(frame);
        if (ret < 0) return -1;
    }

    if (!out_data || !out_samples || !out_channels) return 0;

    *out_channels = graph->to_params.channels;
    int64_t total_out_samples = 0;
    std::vector<float> output_buffer;

    while (true) {
        av_frame_unref(graph->out_frame);
        int ret = av_buffersink_get_frame(graph->buffersink_ctx, graph->out_frame);
        if (ret == AVERROR(EAGAIN)) break;
        if (ret < 0) {
            if (ret != AVERROR_EOF) return -1;
            break;
        }

        int nb_samples = graph->out_frame->nb_samples;
        int out_ch = graph->to_params.channels;
        int64_t old_size = static_cast<int64_t>(output_buffer.size());
        int64_t new_size = old_size + static_cast<int64_t>(nb_samples) * out_ch;
        output_buffer.resize(static_cast<size_t>(new_size));

        if (av_sample_fmt_is_planar(graph->to_avfmt)) {
            for (int s = 0; s < nb_samples; s++) {
                for (int c = 0; c < out_ch; c++) {
                    output_buffer[static_cast<size_t>(old_size) + s * out_ch + c] =
                        static_cast<float*>(static_cast<void*>(graph->out_frame->data[c]))[s];
                }
            }
        } else {
            int bytes = nb_samples * out_ch * sizeof(float);
            std::memcpy(output_buffer.data() + static_cast<size_t>(old_size),
                        graph->out_frame->data[0], bytes);
        }
        av_frame_unref(graph->out_frame);
    }

    if (output_buffer.empty()) {
        *out_data = nullptr;
        *out_samples = 0;
        return 0;
    }

    float* data = new float[output_buffer.size()];
    std::memcpy(data, output_buffer.data(), output_buffer.size() * sizeof(float));
    *out_data = data;
    *out_samples = static_cast<int64_t>(output_buffer.size()) / *out_channels;
    return 0;
}

int oak_audio_filter_graph_flush(OakAudioFilterGraphHandle graph,
                                 float** out_data, int64_t* out_samples,
                                 int* out_channels)
{
    if (!graph || !graph->buffersrc_ctx) return -1;
    int ret = av_buffersrc_add_frame_flags(graph->buffersrc_ctx, nullptr, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) return -1;
    return oak_audio_filter_graph_process(graph, nullptr, 0, out_data, out_samples, out_channels);
}

void oak_audio_filter_graph_free_output(float* data)
{
    delete[] data;
}
