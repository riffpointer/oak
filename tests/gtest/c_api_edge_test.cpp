/***  Edge-case and negative-path tests across all C API modules  ***/

#include <gtest/gtest.h>
#include "oak/codec_api.h"
#include "oak/color_api.h"
#include "oak/engine_api.h"
#include "oak/frame_api.h"
#include <vector>
#include <cstdio>

static std::string TestOcioConfig() {
    return std::string(TEST_SRC_DIR) + "/tests/assets/c_api/test_ocio_config.ocio";
}


/* ------------------------------------------------------------------ */
/*  Codec — Conform (stub/empty paths, verify no crash)               */
/* ------------------------------------------------------------------ */

class CAPCodecEdgeTest : public ::testing::Test {};

TEST_F(CAPCodecEdgeTest, ConformAudioNullDecoder) {
    int ret = oak_decoder_conform_audio(nullptr, "/tmp", 48000, 2, OAK_AUDIO_FMT_FLT);
    EXPECT_NE(ret, 0);
}

TEST_F(CAPCodecEdgeTest, ConformSetReadyCallbackNull) {
    // Should not crash
    oak_conform_set_ready_callback(nullptr, nullptr);
}

TEST_F(CAPCodecEdgeTest, ConformGetNullParams) {
    int count = 0;
    const char** filenames = nullptr;
    int ret = oak_conform_get("/nonexistent.wav", "ffmpeg", "/tmp", 0,
                              48000, 2, OAK_AUDIO_FMT_FLT, false,
                              &filenames, &count);
    // Will likely fail because file doesn't exist; just verify no crash
    (void)ret;
}

TEST_F(CAPCodecEdgeTest, ConformPollNonexistent) {
    int ret = oak_conform_poll("/nonexistent.wav", "/tmp", 0,
                               48000, 2, OAK_AUDIO_FMT_FLT);
    (void)ret;
}

TEST_F(CAPCodecEdgeTest, ConformFreeFilenamesNull) {
    oak_conform_free_filenames(nullptr, 0);
}

TEST_F(CAPCodecEdgeTest, DecoderReadAudioExNull) {
    OakDecoderAudioParams params = {};
    params.start_sample = 0;
    params.sample_count = 1024;
    params.loop_mode = 0;
    params.render_mode = 0;
    params.cache_path = nullptr;
    float* data = nullptr;
    int64_t actual = 0;
    int ret = oak_decoder_read_audio_ex(nullptr, 0, &params, &data, &actual);
    EXPECT_NE(ret, 0);
}

TEST_F(CAPCodecEdgeTest, DecoderReadVideoExNull) {
    OakDecoderVideoParams params = {};
    OakFrame frame = {};
    int ret = oak_decoder_read_video_ex(nullptr, 0, &params, &frame);
    EXPECT_NE(ret, 0);
}

TEST_F(CAPCodecEdgeTest, FrameAllocExtremeSmall) {
    void* frame = oak_frame_alloc(1, 1, 26);
    EXPECT_NE(frame, nullptr);
    if (frame) oak_frame_free(frame);
}

TEST_F(CAPCodecEdgeTest, FrameAllocInvalidAvFormat) {
    void* frame = oak_frame_alloc(64, 64, -1);
    EXPECT_EQ(frame, nullptr);
}

TEST_F(CAPCodecEdgeTest, EncoderWriteAudioNull) {
    OakEncoderHandle enc = oak_encoder_create("/tmp/test_enc_audio_null.mp4", "mp4", "libx264", "aac");
    ASSERT_NE(enc, nullptr);
    int ret = oak_encoder_write_audio(enc, nullptr, 0, 0, 1);
    EXPECT_NE(ret, 0);
    oak_encoder_close(enc);
    std::remove("/tmp/test_enc_audio_null.mp4");
}

/* ------------------------------------------------------------------ */
/*  Color — GPU shader metadata (valid indices when config allows)    */
/* ------------------------------------------------------------------ */

class CAPColorEdgeTest : public ::testing::Test {};

TEST_F(CAPColorEdgeTest, GpuShaderGet3dLutValid) {
    OakColorConfigHandle cfg = oak_color_config_load(TestOcioConfig().c_str());
    ASSERT_NE(cfg, nullptr);
    // Use a display transform that typically generates LUTs
    OakColorProcessorHandle proc = oak_color_processor_create(
        cfg, "scene_linear", "sRGB");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Processor not available";
    }
    OakColorGPUShaderHandle shader = oak_color_gpu_shader_create(proc, "main", "");
    if (!shader) {
        oak_color_processor_free(proc);
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Shader not available";
    }
    int count = oak_color_gpu_shader_get_3d_lut_count(shader);
    for (int i = 0; i < count; ++i) {
        const char* name = nullptr;
        const char* sampler = nullptr;
        unsigned int edge_len = 0;
        int interp = 0;
        const float* values = nullptr;
        int ret = oak_color_gpu_shader_get_3d_lut(shader, i, &name, &sampler, &edge_len, &interp, &values);
        EXPECT_EQ(ret, 0);
        EXPECT_NE(name, nullptr);
        EXPECT_NE(sampler, nullptr);
        EXPECT_GT(edge_len, 0u);
        EXPECT_NE(values, nullptr);
    }
    oak_color_gpu_shader_free(shader);
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorEdgeTest, GpuShaderGetTextureValid) {
    OakColorConfigHandle cfg = oak_color_config_load(TestOcioConfig().c_str());
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create(
        cfg, "scene_linear", "sRGB");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Processor not available";
    }
    OakColorGPUShaderHandle shader = oak_color_gpu_shader_create(proc, "main", "");
    if (!shader) {
        oak_color_processor_free(proc);
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Shader not available";
    }
    int count = oak_color_gpu_shader_get_texture_count(shader);
    for (int i = 0; i < count; ++i) {
        const char* name = nullptr;
        const char* sampler = nullptr;
        unsigned int w = 0, h = 0;
        int ch = 0, dims = 0, interp = 0;
        const float* values = nullptr;
        int ret = oak_color_gpu_shader_get_texture(shader, i, &name, &sampler,
                                                   &w, &h, &ch, &dims, &interp, &values);
        EXPECT_EQ(ret, 0);
        EXPECT_NE(name, nullptr);
        EXPECT_NE(sampler, nullptr);
    }
    oak_color_gpu_shader_free(shader);
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorEdgeTest, ProcessorApplyZeroSize) {
    OakColorConfigHandle cfg = oak_color_config_load(TestOcioConfig().c_str());
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create(cfg, "sRGB", "scene_linear");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Processor not available";
    }
    float pixel[4] = {1.0f, 0.5f, 0.0f, 1.0f};
    int ret = oak_color_processor_apply(proc, 0, 0, pixel, pixel, 4);
    // Zero-size should be a no-op or gracefully fail
    (void)ret;
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorEdgeTest, ProcessorApplyPixelNull) {
    // Null processor should not crash
    float in[4] = {1,0,0,1};
    float out[4] = {0,0,0,0};
    oak_color_processor_apply_pixel(nullptr, in, out);
}

TEST_F(CAPColorEdgeTest, DisplayTransformApplyNull) {
    float buf[4] = {1,0,0,1};
    int ret = oak_display_transform_apply(nullptr, 1, 1, buf, buf, 4);
    EXPECT_NE(ret, 0);
}

/* ------------------------------------------------------------------ */
/*  Engine — null / edge                                              */
/* ------------------------------------------------------------------ */

class CAPEngineEdgeTest : public ::testing::Test {};

TEST_F(CAPEngineEdgeTest, LoadHugeXml) {
    // Very large but well-formed XML should not crash
    std::string xml = "<?xml version=\"1.0\"?><olive><project><name>";
    xml.append(10000, 'X');
    xml += "</name></project></olive>";
    OakEngineProjectHandle proj = oak_engine_project_load_xml(xml.c_str());
    if (proj) oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineEdgeTest, SessionCreateHugeDimensions) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(
        "<?xml version=\"1.0\"?><olive><project><name>H</name></project></olive>");
    ASSERT_NE(proj, nullptr);
    // Use 1024x1024 to avoid Metal GPU driver memory corruption on macOS
    // while still testing large-dimension handling
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1024, 1024, 2, 1, 24);
    // May return null due to GPU memory limits; just verify no crash
    if (session) oak_engine_session_destroy(session);
    oak_engine_project_destroy(proj);
}

/* ------------------------------------------------------------------ */
/*  Cross-module stress                                               */
/* ------------------------------------------------------------------ */

class CAPStressTest : public ::testing::Test {};

TEST_F(CAPStressTest, RapidCodecCreateDestroy) {
    for (int i = 0; i < 100; ++i) {
        OakEncoderHandle enc = oak_encoder_create("/tmp/test_rapid.mp4", "mp4", "libx264", "aac");
        if (enc) oak_encoder_close(enc);
    }
    std::remove("/tmp/test_rapid.mp4");
}
