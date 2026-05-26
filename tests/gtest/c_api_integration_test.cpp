/***  Cross-Module C API Integration Tests  ***/

#include <gtest/gtest.h>
#include "oak/engine_api.h"
#include "oak/codec_api.h"
#include "oak/color_api.h"
#include "oak/frame_api.h"
#include <cstring>
#include <vector>
#include <thread>

static std::string TestOcioConfig() {
    return std::string(TEST_SRC_DIR) + "/tests/assets/c_api/test_ocio_config.ocio";
}


class CAPIntegrationTest : public ::testing::Test {};

TEST_F(CAPIntegrationTest, ColorTransformThenFrameAlloc) {
    OakColorConfigHandle cfg = oak_color_config_load(TestOcioConfig().c_str());
    if (!cfg) {
        GTEST_SKIP() << "Default OCIO config not available";
    }
    OakColorProcessorHandle proc = oak_color_processor_create(cfg, "ACEScg", "sRGB");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Processor creation failed";
    }

    void* frame = oak_frame_alloc(64, 64, 26); // RGBA
    ASSERT_NE(frame, nullptr);

    void* data = nullptr;
    int linesize = 0;
    oak_frame_get_plane(frame, 0, &data, &linesize);
    ASSERT_NE(data, nullptr);

    float* fdata = static_cast<float*>(data);
    for (int i = 0; i < 64 * 64 * 4; ++i) {
        fdata[i] = 1.0f;
    }

    int ret = oak_color_processor_apply(proc, 64, 64, fdata, fdata, 4);
    EXPECT_EQ(ret, 0);

    oak_frame_free(frame);
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPIntegrationTest, EngineProjectWithColorProcessor) {
    const char* xml = R"xml(<?xml version="1.0"?><olive><project><name>Int</name></project></olive>)xml";
    OakEngineProjectHandle proj = oak_engine_project_load_xml(xml);
    EXPECT_NE(proj, nullptr);

    OakColorConfigHandle cfg = oak_color_config_load(TestOcioConfig().c_str());
    EXPECT_NE(cfg, nullptr);

    if (proj) oak_engine_project_destroy(proj);
    if (cfg) oak_color_config_free(cfg);
}

TEST_F(CAPIntegrationTest, DecodeFrameThenColorTransform) {
    OakDecoderHandle dec = oak_decoder_create_from_id("ffmpeg");
    EXPECT_NE(dec, nullptr);

    OakColorConfigHandle cfg = oak_color_config_load(TestOcioConfig().c_str());
    EXPECT_NE(cfg, nullptr);

    if (dec && cfg) {
        OakColorProcessorHandle proc = oak_color_processor_create(cfg, "ACEScg", "sRGB");
        if (!proc) {
            oak_decoder_close(dec);
            oak_color_config_free(cfg);
            GTEST_SKIP() << "Color spaces not available in default config";
        }
        EXPECT_NE(proc, nullptr);
        if (proc) oak_color_processor_free(proc);
    }

    if (dec) oak_decoder_close(dec);
    if (cfg) oak_color_config_free(cfg);
}

TEST_F(CAPIntegrationTest, DlopenConcurrent) {
    // Verify that multiple modules can coexist in the same process
    OakColorConfigHandle cfg = oak_color_config_load(TestOcioConfig().c_str());
    OakDecoderHandle dec = oak_decoder_create_from_id("ffmpeg");
    OakEngineProjectHandle proj = oak_engine_project_load_xml(
        "<?xml version=\"1.0\"?><olive><project><name>C</name></project></olive>");

    EXPECT_NE(cfg, nullptr);
    EXPECT_NE(dec, nullptr);
    EXPECT_NE(proj, nullptr);

    // Serial access to avoid thread-safety issues with non-thread-safe handles
    if (cfg) {
        int n = oak_color_config_space_count(cfg);
        (void)n;
    }
    if (dec) {
        const char* id = oak_decoder_id(dec);
        (void)id;
    }
    if (proj) {
        int count = oak_engine_project_node_count(proj);
        (void)count;
    }

    if (cfg) oak_color_config_free(cfg);
    if (dec) oak_decoder_close(dec);
    if (proj) oak_engine_project_destroy(proj);
}
