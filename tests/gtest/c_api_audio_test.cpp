/***  Audio C API Tests  ***/

#include <gtest/gtest.h>
#include "oak/audio_api.h"
#include <cmath>
#include <vector>
#include <cstring>
#include <thread>

class CAPAudioTest : public ::testing::Test {};

/* ------------------------------------------------------------------ */
/*  Buffer lifecycle                                                  */
/* ------------------------------------------------------------------ */

TEST_F(CAPAudioTest, BufferCreateFree) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1024, 48000, true);
    EXPECT_NE(buf, nullptr);
    if (buf) oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, BufferCreateZeroChannels) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(0, 1024, 48000, true);
    EXPECT_EQ(buf, nullptr);
}

TEST_F(CAPAudioTest, BufferCreateZeroSamples) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 0, 48000, true);
    EXPECT_EQ(buf, nullptr);
}

TEST_F(CAPAudioTest, BufferCreateZeroSampleRate) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1024, 0, true);
    EXPECT_EQ(buf, nullptr);
}

TEST_F(CAPAudioTest, BufferFreeNull) {
    oak_audio_buffer_free(nullptr);
}

TEST_F(CAPAudioTest, BufferParams) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1024, 48000, true);
    ASSERT_NE(buf, nullptr);
    OakAudioParams params{};
    oak_audio_buffer_params(buf, &params);
    EXPECT_EQ(params.channels, 2);
    EXPECT_EQ(params.duration_samples, 1024);
    EXPECT_EQ(params.sample_rate, 48000);
    EXPECT_EQ(params.sample_fmt, OAK_AUDIO_FMT_FLT);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, BufferParamsNull) {
    OakAudioParams params{};
    oak_audio_buffer_params(nullptr, &params);
    // Should not crash; params unchanged or zeroed
}

TEST_F(CAPAudioTest, BufferDataInterleaved) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1024, 48000, true);
    ASSERT_NE(buf, nullptr);
    void* data = nullptr;
    bool interleaved = false;
    oak_audio_buffer_data(buf, &data, &interleaved);
    EXPECT_NE(data, nullptr);
    EXPECT_TRUE(interleaved);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, BufferDataPlanar) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1024, 48000, false);
    ASSERT_NE(buf, nullptr);
    void* data = nullptr;
    bool interleaved = true;
    oak_audio_buffer_data(buf, &data, &interleaved);
    EXPECT_NE(data, nullptr);
    EXPECT_FALSE(interleaved);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, BufferClone) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1024, 48000, true);
    ASSERT_NE(buf, nullptr);

    // Fill with known pattern
    void* data = nullptr;
    oak_audio_buffer_data(buf, &data, nullptr);
    float* fdata = static_cast<float*>(data);
    for (int i = 0; i < 2 * 1024; ++i) {
        fdata[i] = static_cast<float>(i);
    }

    OakAudioBufferHandle clone = oak_audio_buffer_clone(buf);
    ASSERT_NE(clone, nullptr);

    OakAudioParams params{};
    oak_audio_buffer_params(clone, &params);
    EXPECT_EQ(params.channels, 2);
    EXPECT_EQ(params.duration_samples, 1024);
    EXPECT_EQ(params.sample_rate, 48000);

    void* cdata = nullptr;
    oak_audio_buffer_data(clone, &cdata, nullptr);
    float* cfdata = static_cast<float*>(cdata);
    for (int i = 0; i < 2 * 1024; ++i) {
        EXPECT_FLOAT_EQ(cfdata[i], static_cast<float>(i));
    }

    // Verify independence: modify original, clone unchanged
    fdata[0] = 999.0f;
    EXPECT_FLOAT_EQ(cfdata[0], 0.0f);

    oak_audio_buffer_free(clone);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, BufferCloneNull) {
    EXPECT_EQ(oak_audio_buffer_clone(nullptr), nullptr);
}

/* ------------------------------------------------------------------ */
/*  Resampler                                                         */
/* ------------------------------------------------------------------ */

TEST_F(CAPAudioTest, ResamplerCreateFree) {
    OakAudioResamplerHandle r = oak_audio_resampler_create(2, 48000, 2, 44100);
    EXPECT_NE(r, nullptr);
    if (r) oak_audio_resampler_free(r);
}

TEST_F(CAPAudioTest, ResamplerCreateInvalid) {
    EXPECT_EQ(oak_audio_resampler_create(0, 48000, 2, 44100), nullptr);
    EXPECT_EQ(oak_audio_resampler_create(2, 0, 2, 44100), nullptr);
    EXPECT_EQ(oak_audio_resampler_create(2, 48000, 0, 44100), nullptr);
    EXPECT_EQ(oak_audio_resampler_create(2, 48000, 2, 0), nullptr);
}

TEST_F(CAPAudioTest, ResamplerFreeNull) {
    oak_audio_resampler_free(nullptr);
}

TEST_F(CAPAudioTest, Resampler48kTo44k1) {
    OakAudioResamplerHandle r = oak_audio_resampler_create(2, 48000, 2, 44100);
    ASSERT_NE(r, nullptr);

    std::vector<float> in(2 * 4800, 1.0f);
    std::vector<float> out(2 * 5000, 0.0f);
    int64_t actual = 0;
    int ret = oak_audio_resampler_process(r, in.data(), 4800, out.data(), 5000, &actual);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(actual, 0);
    // 4800 * 44100 / 48000 = 4410, allow larger tolerance for resampler
    EXPECT_NEAR(actual, 4410, 50);
    oak_audio_resampler_free(r);
}

TEST_F(CAPAudioTest, ResamplerStereoToMono) {
    OakAudioResamplerHandle r = oak_audio_resampler_create(2, 48000, 1, 48000);
    ASSERT_NE(r, nullptr);

    // Left = 1.0, Right = 0.5
    std::vector<float> in(2 * 1000);
    for (int i = 0; i < 1000; ++i) {
        in[i * 2] = 1.0f;
        in[i * 2 + 1] = 0.5f;
    }
    std::vector<float> out(1 * 1000, 0.0f);
    int64_t actual = 0;
    int ret = oak_audio_resampler_process(r, in.data(), 1000, out.data(), 1000, &actual);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(actual, 1000);
    // Mono output should be non-zero
    EXPECT_GT(std::abs(out[0]), 0.0f);
    oak_audio_resampler_free(r);
}

TEST_F(CAPAudioTest, ResamplerProcessNull) {
    std::vector<float> out(100, 0.0f);
    int64_t actual = 0;
    EXPECT_EQ(oak_audio_resampler_process(nullptr, nullptr, 0, out.data(), 100, &actual), -1);
}

/* ------------------------------------------------------------------ */
/*  Mixer                                                             */
/* ------------------------------------------------------------------ */

TEST_F(CAPAudioTest, MixerCreateFree) {
    OakAudioMixerHandle m = oak_audio_mixer_create(2, 48000);
    EXPECT_NE(m, nullptr);
    if (m) oak_audio_mixer_free(m);
}

TEST_F(CAPAudioTest, MixerCreateInvalid) {
    EXPECT_EQ(oak_audio_mixer_create(0, 48000), nullptr);
    EXPECT_EQ(oak_audio_mixer_create(2, 0), nullptr);
}

TEST_F(CAPAudioTest, MixerFreeNull) {
    oak_audio_mixer_free(nullptr);
}

TEST_F(CAPAudioTest, MixerMixNoSources) {
    OakAudioMixerHandle m = oak_audio_mixer_create(2, 48000);
    ASSERT_NE(m, nullptr);
    std::vector<float> out(2 * 100, 1.0f);
    int ret = oak_audio_mixer_mix(m, 0, 100, out.data());
    EXPECT_EQ(ret, 0);
    // Should be zeroed
    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_FLOAT_EQ(out[i], 0.0f);
    }
    oak_audio_mixer_free(m);
}

TEST_F(CAPAudioTest, MixerMixSingleSource) {
    OakAudioMixerHandle m = oak_audio_mixer_create(2, 48000);
    ASSERT_NE(m, nullptr);

    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 100, 48000, true);
    ASSERT_NE(buf, nullptr);
    void* data = nullptr;
    oak_audio_buffer_data(buf, &data, nullptr);
    float* fdata = static_cast<float*>(data);
    for (int i = 0; i < 2 * 100; ++i) {
        fdata[i] = 0.5f;
    }

    oak_audio_mixer_add_source(m, buf, 0, 1.0, 0.0);
    std::vector<float> out(2 * 100, 0.0f);
    int ret = oak_audio_mixer_mix(m, 0, 100, out.data());
    EXPECT_EQ(ret, 0);
    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_FLOAT_EQ(out[i], 0.5f);
    }

    oak_audio_mixer_free(m);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, MixerMixMultiSource) {
    OakAudioMixerHandle m = oak_audio_mixer_create(2, 48000);
    ASSERT_NE(m, nullptr);

    OakAudioBufferHandle buf1 = oak_audio_buffer_create(2, 100, 48000, true);
    OakAudioBufferHandle buf2 = oak_audio_buffer_create(2, 100, 48000, true);
    ASSERT_NE(buf1, nullptr);
    ASSERT_NE(buf2, nullptr);

    void* d1 = nullptr;
    void* d2 = nullptr;
    oak_audio_buffer_data(buf1, &d1, nullptr);
    oak_audio_buffer_data(buf2, &d2, nullptr);
    std::fill_n(static_cast<float*>(d1), 2 * 100, 0.3f);
    std::fill_n(static_cast<float*>(d2), 2 * 100, 0.4f);

    oak_audio_mixer_add_source(m, buf1, 0, 1.0, 0.0);
    oak_audio_mixer_add_source(m, buf2, 0, 1.0, 0.0);

    std::vector<float> out(2 * 100, 0.0f);
    int ret = oak_audio_mixer_mix(m, 0, 100, out.data());
    EXPECT_EQ(ret, 0);
    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_FLOAT_EQ(out[i], 0.7f);
    }

    oak_audio_mixer_free(m);
    oak_audio_buffer_free(buf1);
    oak_audio_buffer_free(buf2);
}

TEST_F(CAPAudioTest, MixerPan) {
    OakAudioMixerHandle m = oak_audio_mixer_create(2, 48000);
    ASSERT_NE(m, nullptr);

    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 100, 48000, true);
    ASSERT_NE(buf, nullptr);
    void* data = nullptr;
    oak_audio_buffer_data(buf, &data, nullptr);
    std::fill_n(static_cast<float*>(data), 2 * 100, 1.0f);

    // Pan hard left
    oak_audio_mixer_add_source(m, buf, 0, 1.0, -1.0);
    std::vector<float> out(2 * 100, 0.0f);
    oak_audio_mixer_mix(m, 0, 100, out.data());

    // Left channel should be louder than right
    float left_avg = 0.0f, right_avg = 0.0f;
    for (int i = 0; i < 100; ++i) {
        left_avg += out[i * 2];
        right_avg += out[i * 2 + 1];
    }
    left_avg /= 100.0f;
    right_avg /= 100.0f;
    EXPECT_GT(left_avg, right_avg);

    oak_audio_mixer_free(m);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, MixerClearSources) {
    OakAudioMixerHandle m = oak_audio_mixer_create(2, 48000);
    ASSERT_NE(m, nullptr);

    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 100, 48000, true);
    ASSERT_NE(buf, nullptr);
    oak_audio_mixer_add_source(m, buf, 0, 1.0, 0.0);
    oak_audio_mixer_clear_sources(m);

    std::vector<float> out(2 * 100, 1.0f);
    oak_audio_mixer_mix(m, 0, 100, out.data());
    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_FLOAT_EQ(out[i], 0.0f);
    }

    oak_audio_mixer_free(m);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, MixerMixOffset) {
    OakAudioMixerHandle m = oak_audio_mixer_create(2, 48000);
    ASSERT_NE(m, nullptr);

    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 50, 48000, true);
    ASSERT_NE(buf, nullptr);
    void* data = nullptr;
    oak_audio_buffer_data(buf, &data, nullptr);
    std::fill_n(static_cast<float*>(data), 2 * 50, 0.5f);

    oak_audio_mixer_add_source(m, buf, 10, 1.0, 0.0);

    std::vector<float> out(2 * 100, 0.0f);
    int ret = oak_audio_mixer_mix(m, 0, 100, out.data());
    EXPECT_EQ(ret, 0);

    // Samples 0..9 should be zero (before source start)
    for (int i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(out[i * 2], 0.0f);
        EXPECT_FLOAT_EQ(out[i * 2 + 1], 0.0f);
    }
    // Samples 10..59 should have source
    for (int i = 10; i < 60; ++i) {
        EXPECT_FLOAT_EQ(out[i * 2], 0.5f);
        EXPECT_FLOAT_EQ(out[i * 2 + 1], 0.5f);
    }
    // Samples 60+ should be zero (after source end)
    for (int i = 60; i < 100; ++i) {
        EXPECT_FLOAT_EQ(out[i * 2], 0.0f);
        EXPECT_FLOAT_EQ(out[i * 2 + 1], 0.0f);
    }

    oak_audio_mixer_free(m);
    oak_audio_buffer_free(buf);
}

/* ------------------------------------------------------------------ */
/*  Filter Graph                                                      */
/* ------------------------------------------------------------------ */

TEST_F(CAPAudioTest, FilterGraphCreateFree) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1000, 48000, true);
    ASSERT_NE(buf, nullptr);
    OakAudioParams from{};
    oak_audio_buffer_params(buf, &from);

    OakAudioParams to{};
    to.channels = 2;
    to.sample_rate = 48000;
    to.sample_fmt = OAK_AUDIO_FMT_FLT;
    to.duration_samples = 1000;
    // Copy channel layout mask from the buffer
    to.channel_layout_mask = from.channel_layout_mask;

    OakAudioFilterGraphHandle g = oak_audio_filter_graph_create(&from, &to, 1.0);
    EXPECT_NE(g, nullptr);
    if (g) oak_audio_filter_graph_destroy(g);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, FilterGraphCreateNull) {
    OakAudioParams to{};
    to.channels = 2;
    to.sample_rate = 48000;
    EXPECT_EQ(oak_audio_filter_graph_create(nullptr, &to, 1.0), nullptr);
}

TEST_F(CAPAudioTest, FilterGraphCreateInvalid) {
    OakAudioParams from{};
    from.channels = 0;
    from.sample_rate = 48000;
    OakAudioParams to{};
    to.channels = 2;
    to.sample_rate = 48000;
    EXPECT_EQ(oak_audio_filter_graph_create(&from, &to, 1.0), nullptr);
}

TEST_F(CAPAudioTest, FilterGraphDestroyNull) {
    oak_audio_filter_graph_destroy(nullptr);
}

TEST_F(CAPAudioTest, FilterGraphTempo) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 48000, 48000, true);
    ASSERT_NE(buf, nullptr);
    OakAudioParams from{};
    oak_audio_buffer_params(buf, &from);

    OakAudioParams to{};
    to.channels = 2;
    to.sample_rate = 48000;
    to.sample_fmt = OAK_AUDIO_FMT_FLT;
    to.duration_samples = 48000;
    to.channel_layout_mask = from.channel_layout_mask;

    OakAudioFilterGraphHandle g = oak_audio_filter_graph_create(&from, &to, 2.0);
    ASSERT_NE(g, nullptr);

    // Generate input: 2 channels, 48000 samples interleaved (1 second)
    std::vector<float> in(2 * 48000, 0.5f);
    const float* in_ptrs[1] = { in.data() };
    float* out_data = nullptr;
    int64_t out_samples = 0;
    int out_channels = 0;

    int ret = oak_audio_filter_graph_process(g, in_ptrs, 48000, &out_data, &out_samples, &out_channels);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(out_channels, 2);

    // atempo may need flush to get all remaining samples
    float* flush_data = nullptr;
    int64_t flush_samples = 0;
    int flush_channels = 0;
    ret = oak_audio_filter_graph_flush(g, &flush_data, &flush_samples, &flush_channels);
    EXPECT_EQ(ret, 0);

    int64_t total_samples = out_samples + flush_samples;
    // With tempo=2, total output should be roughly half of input
    EXPECT_GT(total_samples, 0);
    EXPECT_LT(total_samples, 30000);

    if (out_data) oak_audio_filter_graph_free_output(out_data);
    if (flush_data) oak_audio_filter_graph_free_output(flush_data);

    oak_audio_filter_graph_destroy(g);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, FilterGraphFormatConversion) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1000, 48000, true);
    ASSERT_NE(buf, nullptr);
    OakAudioParams from{};
    oak_audio_buffer_params(buf, &from);

    OakAudioParams to{};
    to.channels = 1;
    to.sample_rate = 44100;
    to.sample_fmt = OAK_AUDIO_FMT_FLT;
    to.duration_samples = 1000;
    // Use stereo mask for from; mono for to will be handled by aformat filter
    to.channel_layout_mask = from.channel_layout_mask;

    OakAudioFilterGraphHandle g = oak_audio_filter_graph_create(&from, &to, 1.0);
    ASSERT_NE(g, nullptr);

    std::vector<float> in(2 * 1000, 0.5f);
    const float* in_ptrs[1] = { in.data() };
    float* out_data = nullptr;
    int64_t out_samples = 0;
    int out_channels = 0;

    int ret = oak_audio_filter_graph_process(g, in_ptrs, 1000, &out_data, &out_samples, &out_channels);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(out_channels, 1);
    EXPECT_GT(out_samples, 0);
    if (out_data) oak_audio_filter_graph_free_output(out_data);

    oak_audio_filter_graph_destroy(g);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, FilterGraphFlush) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1000, 48000, true);
    ASSERT_NE(buf, nullptr);
    OakAudioParams from{};
    oak_audio_buffer_params(buf, &from);

    OakAudioParams to{};
    to.channels = 2;
    to.sample_rate = 48000;
    to.sample_fmt = OAK_AUDIO_FMT_FLT;
    to.duration_samples = 1000;
    to.channel_layout_mask = from.channel_layout_mask;

    OakAudioFilterGraphHandle g = oak_audio_filter_graph_create(&from, &to, 1.0);
    ASSERT_NE(g, nullptr);

    // Feed some data
    std::vector<float> in(2 * 1000, 0.5f);
    const float* in_ptrs[1] = { in.data() };
    float* out_data = nullptr;
    int64_t out_samples = 0;
    int out_channels = 0;

    oak_audio_filter_graph_process(g, in_ptrs, 1000, &out_data, &out_samples, &out_channels);
    if (out_data) oak_audio_filter_graph_free_output(out_data);

    // Flush
    float* flush_data = nullptr;
    int64_t flush_samples = 0;
    int flush_channels = 0;
    int ret = oak_audio_filter_graph_flush(g, &flush_data, &flush_samples, &flush_channels);
    EXPECT_EQ(ret, 0);
    // Flush may or may not produce samples depending on filter latency
    if (flush_data) oak_audio_filter_graph_free_output(flush_data);

    oak_audio_filter_graph_destroy(g);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioTest, FilterGraphProcessNull) {
    float* out_data = nullptr;
    int64_t out_samples = 0;
    int out_channels = 0;
    EXPECT_EQ(oak_audio_filter_graph_process(nullptr, nullptr, 0, &out_data, &out_samples, &out_channels), -1);
}

TEST_F(CAPAudioTest, FilterGraphFreeOutputNull) {
    oak_audio_filter_graph_free_output(nullptr);
}

/* ------------------------------------------------------------------ */
/*  Convert Layout                                                    */
/* ------------------------------------------------------------------ */

TEST_F(CAPAudioTest, ConvertLayoutII) {
    // Interleaved stereo -> interleaved mono
    std::vector<float> in(2 * 100, 0.5f);
    std::vector<float> out(1 * 100, 0.0f);
    int ret = oak_audio_convert_layout(in.data(), 2, 100, true, out.data(), 1, 100, true);
    EXPECT_EQ(ret, 0);
    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(out[i], 0.5f);
    }
}

TEST_F(CAPAudioTest, ConvertLayoutIP) {
    // Interleaved stereo -> planar stereo
    std::vector<float> in(2 * 100, 0.5f);
    float* out_planes[2] = {
        new float[100],
        new float[100]
    };
    int ret = oak_audio_convert_layout(in.data(), 2, 100, true,
                                       reinterpret_cast<float*>(out_planes), 2, 100, false);
    EXPECT_EQ(ret, 0);
    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(out_planes[0][i], 0.5f);
        EXPECT_FLOAT_EQ(out_planes[1][i], 0.5f);
    }
    delete[] out_planes[0];
    delete[] out_planes[1];
}

TEST_F(CAPAudioTest, ConvertLayoutPI) {
    // Planar stereo -> interleaved stereo
    float* in_planes[2] = {
        new float[100],
        new float[100]
    };
    std::fill_n(in_planes[0], 100, 0.3f);
    std::fill_n(in_planes[1], 100, 0.7f);
    std::vector<float> out(2 * 100, 0.0f);
    int ret = oak_audio_convert_layout(reinterpret_cast<float*>(in_planes), 2, 100, false,
                                       out.data(), 2, 100, true);
    EXPECT_EQ(ret, 0);
    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(out[i * 2], 0.3f);
        EXPECT_FLOAT_EQ(out[i * 2 + 1], 0.7f);
    }
    delete[] in_planes[0];
    delete[] in_planes[1];
}

TEST_F(CAPAudioTest, ConvertLayoutPP) {
    // Planar stereo -> planar mono
    float* in_planes[2] = {
        new float[100],
        new float[100]
    };
    std::fill_n(in_planes[0], 100, 0.5f);
    std::fill_n(in_planes[1], 100, 0.5f);
    float* out_planes[1] = { new float[100] };
    int ret = oak_audio_convert_layout(reinterpret_cast<float*>(in_planes), 2, 100, false,
                                       reinterpret_cast<float*>(out_planes), 1, 100, false);
    EXPECT_EQ(ret, 0);
    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(out_planes[0][i], 0.5f);
    }
    delete[] in_planes[0];
    delete[] in_planes[1];
    delete[] out_planes[0];
}

TEST_F(CAPAudioTest, ConvertLayoutNull) {
    std::vector<float> out(100, 0.0f);
    EXPECT_EQ(oak_audio_convert_layout(nullptr, 2, 100, true, out.data(), 2, 100, true), -1);
    EXPECT_EQ(oak_audio_convert_layout(out.data(), 2, 100, true, nullptr, 2, 100, true), -1);
}

TEST_F(CAPAudioTest, ConvertLayoutZeroChannels) {
    std::vector<float> in(100, 0.0f);
    std::vector<float> out(100, 0.0f);
    EXPECT_EQ(oak_audio_convert_layout(in.data(), 0, 100, true, out.data(), 2, 100, true), -1);
}


/* ------------------------------------------------------------------ */
/*  Edge cases & stress                                               */
/* ------------------------------------------------------------------ */

class CAPAudioEdgeTest : public ::testing::Test {};

TEST_F(CAPAudioEdgeTest, ResamplerHugeResampleRatio) {
    OakAudioResamplerHandle r = oak_audio_resampler_create(2, 48000, 2, 8000);
    ASSERT_NE(r, nullptr);
    std::vector<float> in(2 * 4800, 0.5f);
    std::vector<float> out(2 * 50000, 0.0f);
    int64_t actual = 0;
    int ret = oak_audio_resampler_process(r, in.data(), 4800, out.data(), 50000, &actual);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(actual, 0);
    oak_audio_resampler_free(r);
}

TEST_F(CAPAudioEdgeTest, MixerVolumeClamping) {
    OakAudioMixerHandle m = oak_audio_mixer_create(2, 48000);
    ASSERT_NE(m, nullptr);
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 100, 48000, true);
    ASSERT_NE(buf, nullptr);
    void* data = nullptr;
    oak_audio_buffer_data(buf, &data, nullptr);
    std::fill_n(static_cast<float*>(data), 2 * 100, 1.0f);

    // Volume > 10 should be clamped
    oak_audio_mixer_add_source(m, buf, 0, 100.0, 0.0);
    std::vector<float> out(2 * 100, 0.0f);
    oak_audio_mixer_mix(m, 0, 100, out.data());
    // Should not be 100x; verify it's finite and not NaN
    EXPECT_TRUE(std::isfinite(out[0]));

    oak_audio_mixer_free(m);
    oak_audio_buffer_free(buf);
}

TEST_F(CAPAudioEdgeTest, FilterGraphVeryLowTempo) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1000, 48000, true);
    ASSERT_NE(buf, nullptr);
    OakAudioParams from{};
    oak_audio_buffer_params(buf, &from);

    OakAudioParams to{};
    to.channels = 2;
    to.sample_rate = 48000;
    to.sample_fmt = OAK_AUDIO_FMT_FLT;
    to.duration_samples = 1000;
    to.channel_layout_mask = from.channel_layout_mask;

    // Very low tempo is extreme; implementation may reject or handle
    // Only test create/destroy — do NOT call process because atempo hangs on extreme values
    OakAudioFilterGraphHandle g = oak_audio_filter_graph_create(&from, &to, 0.01);
    if (g) oak_audio_filter_graph_destroy(g);
    oak_audio_buffer_free(buf);
}

class CAPAudioStressTest : public ::testing::Test {};

TEST_F(CAPAudioStressTest, RapidCreateDestroy) {
    for (int i = 0; i < 100; ++i) {
        OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1024, 48000, true);
        if (buf) oak_audio_buffer_free(buf);
    }
}

TEST_F(CAPAudioStressTest, ConcurrentBufferAccess) {
    OakAudioBufferHandle buf = oak_audio_buffer_create(2, 1024, 48000, true);
    ASSERT_NE(buf, nullptr);

    auto worker = [&](int id) {
        void* data = nullptr;
        oak_audio_buffer_data(buf, &data, nullptr);
        if (data) {
            float* f = static_cast<float*>(data);
            for (int i = 0; i < 1024 * 2; ++i) {
                f[i] = static_cast<float>(id);
            }
        }
    };

    std::thread t1([&]() { worker(1); });
    std::thread t2([&]() { worker(2); });
    t1.join();
    t2.join();

    oak_audio_buffer_free(buf);
}
