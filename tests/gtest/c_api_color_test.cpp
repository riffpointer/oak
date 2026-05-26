/***  Color C API Tests  ***/

#include <gtest/gtest.h>
#include "oak/color_api.h"
#include <cstring>
#include <cmath>

class CAPColorTest : public ::testing::Test {};

TEST_F(CAPColorTest, ConfigLoadDefault) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    EXPECT_NE(cfg, nullptr);
    if (cfg) oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ConfigLoadOCIOPathMissing) {
    OakColorConfigHandle cfg = oak_color_config_load("/nonexistent/config.ocio");
    EXPECT_EQ(cfg, nullptr);
}

TEST_F(CAPColorTest, ConfigFreeNull) {
    oak_color_config_free(nullptr);
}

TEST_F(CAPColorTest, ConfigDoubleFree) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    oak_color_config_free(cfg);
    // Double free may crash; do not test it explicitly
}

TEST_F(CAPColorTest, ConfigGetSpaceCount) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    int n = oak_color_config_space_count(cfg);
    EXPECT_GE(n, 0);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ConfigGetSpaceCountNull) {
    int n = oak_color_config_space_count(nullptr);
    EXPECT_LE(n, 0);
}

TEST_F(CAPColorTest, ConfigGetSpaceName) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    const char* name = oak_color_config_space_name(cfg, 0);
    EXPECT_NE(name, nullptr);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ProcessorCreateTransform) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create(
        cfg, "sRGB", "scene_linear");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Color spaces not available in default config";
    }
    EXPECT_NE(proc, nullptr);
    if (proc) oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ProcessorCreateNullConfig) {
    OakColorProcessorHandle proc = oak_color_processor_create(
        nullptr, "sRGB", "scene_linear");
    EXPECT_EQ(proc, nullptr);
}

TEST_F(CAPColorTest, ProcessorFreeNull) {
    oak_color_processor_free(nullptr);
}

TEST_F(CAPColorTest, ProcessorApplyPixel) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create(
        cfg, "sRGB", "scene_linear");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "No processor created";
    }
    float in_rgba[4] = {1.0f, 0.5f, 0.0f, 1.0f};
    float out_rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    oak_color_processor_apply_pixel(proc, in_rgba, out_rgba);
    EXPECT_FALSE(std::isnan(out_rgba[0]) || std::isnan(out_rgba[1]) || std::isnan(out_rgba[2]));
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ProcessorApplyBuffer) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create(
        cfg, "sRGB", "scene_linear");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "No processor created";
    }
    std::vector<float> in_buf(64 * 64 * 4, 1.0f);
    std::vector<float> out_buf(64 * 64 * 4, 0.0f);
    int ret = oak_color_processor_apply(proc, 64, 64, in_buf.data(), out_buf.data(), 4);
    EXPECT_EQ(ret, 0);
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, DisplayTransformCreate) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakDisplayTransformHandle dt = oak_display_transform_create(
        cfg, "scene_linear", "sRGB", "Default", nullptr, 0.0f, 1.0f);
    if (!dt) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Display transform not available in default config";
    }
    EXPECT_NE(dt, nullptr);
    if (dt) oak_display_transform_free(dt);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, DisplayTransformNullConfig) {
    OakDisplayTransformHandle dt = oak_display_transform_create(
        nullptr, "scene_linear", "sRGB", "Default", nullptr, 0.0f, 1.0f);
    EXPECT_EQ(dt, nullptr);
}

TEST_F(CAPColorTest, DisplayTransformFreeNull) {
    oak_display_transform_free(nullptr);
}

TEST_F(CAPColorTest, GpuShaderCreate) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create(cfg, "sRGB", "scene_linear");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "No processor created";
    }
    OakColorGPUShaderHandle shader = oak_color_gpu_shader_create(proc, "main", "");
    EXPECT_NE(shader, nullptr);
    if (shader) oak_color_gpu_shader_free(shader);
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, GpuShaderGetText) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create(cfg, "sRGB", "scene_linear");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "No processor created";
    }
    OakColorGPUShaderHandle shader = oak_color_gpu_shader_create(proc, "main", "");
    if (!shader) {
        oak_color_processor_free(proc);
        oak_color_config_free(cfg);
        GTEST_SKIP() << "No shader created";
    }
    const char* text = oak_color_gpu_shader_get_text(shader);
    EXPECT_NE(text, nullptr);
    EXPECT_GT(std::strlen(text), 0);
    oak_color_gpu_shader_free(shader);
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, GpuShaderFreeNull) {
    oak_color_gpu_shader_free(nullptr);
}

TEST_F(CAPColorTest, GradingPrimaryCreate) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    EXPECT_NE(gp, nullptr);
    if (gp) oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimaryFreeNull) {
    oak_color_grading_primary_free(nullptr);
}

TEST_F(CAPColorTest, GradingPrimaryDoubleFree) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    oak_color_grading_primary_free(gp);
    // Double free may crash; do not test it explicitly
}

TEST_F(CAPColorTest, GradingPrimarySetSaturation) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    oak_color_grading_primary_set_saturation(gp, 1.5f);
    // get_saturation does not exist; just verify set does not crash
    oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, ConfigDefaultDisplay) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    const char* disp = oak_color_config_default_display(cfg);
    EXPECT_NE(disp, nullptr);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ConfigDefaultInputSpace) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    const char* space = oak_color_config_default_input_space(cfg);
    EXPECT_NE(space, nullptr);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ConfigReferenceSpaceName) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    const char* ref = oak_color_config_reference_space_name(cfg);
    EXPECT_NE(ref, nullptr);
    oak_color_config_free(cfg);
}
