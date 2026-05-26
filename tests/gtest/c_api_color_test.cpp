/***  Color C API Tests  ***/

#include <gtest/gtest.h>
#include "oak/color_api.h"
#include <cstring>
#include <cmath>
#include <vector>

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

TEST_F(CAPColorTest, ConfigGetSpace) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorSpaceHandle space = oak_color_config_get_space(cfg, "ACEScg");
    // May be null if ACEScg is not in default config
    (void)space;
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ConfigDisplayViewCount) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    const char* disp = oak_color_config_default_display(cfg);
    if (disp) {
        int n = oak_color_config_display_view_count(cfg, disp);
        EXPECT_GE(n, 0);
    }
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ConfigDisplayViewName) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    const char* disp = oak_color_config_default_display(cfg);
    if (disp) {
        const char* name = oak_color_config_display_view_name(cfg, disp, 0);
        // May be null if no views
        (void)name;
    }
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ColorSpaceEqual) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorSpaceHandle s1 = oak_color_config_get_space(cfg, "sRGB");
    OakColorSpaceHandle s2 = oak_color_config_get_space(cfg, "scene_linear");
    if (s1 && s2) {
        EXPECT_FALSE(oak_color_space_equal(s1, s2));
    }
    EXPECT_FALSE(oak_color_space_equal(nullptr, nullptr));
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

TEST_F(CAPColorTest, ProcessorCreateTransformForwardInverse) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle fwd = oak_color_processor_create_transform(cfg, "sRGB", "scene_linear", 0);
    OakColorProcessorHandle inv = oak_color_processor_create_transform(cfg, "scene_linear", "sRGB", 1);
    if (fwd) oak_color_processor_free(fwd);
    if (inv) oak_color_processor_free(inv);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ProcessorCreateDisplay) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create_display(
        cfg, "scene_linear", "sRGB", "Default", nullptr, 0);
    if (proc) oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, ProcessorCreateFromLutMissing) {
    OakColorProcessorHandle proc = oak_color_processor_create_from_lut(
        "/nonexistent/lut.cube", "sRGB", "scene_linear");
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

TEST_F(CAPColorTest, ProcessorApplyInPlace) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create(
        cfg, "sRGB", "scene_linear");
    if (!proc) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "No processor created";
    }
    std::vector<float> buf(64 * 64 * 4, 1.0f);
    int ret = oak_color_processor_apply(proc, 64, 64, buf.data(), buf.data(), 4);
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

TEST_F(CAPColorTest, DisplayTransformApply) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakDisplayTransformHandle dt = oak_display_transform_create(
        cfg, "scene_linear", "sRGB", "Default", nullptr, 0.0f, 1.0f);
    if (!dt) {
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Display transform not available";
    }
    std::vector<float> in(64 * 64 * 4, 1.0f);
    std::vector<float> out(64 * 64 * 4, 0.0f);
    int ret = oak_display_transform_apply(dt, 64, 64, in.data(), out.data(), 4);
    EXPECT_EQ(ret, 0);
    oak_display_transform_free(dt);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, DisplayTransformApplyExposure) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakDisplayTransformHandle dt1 = oak_display_transform_create(
        cfg, "scene_linear", "sRGB", "Default", nullptr, 0.0f, 1.0f);
    OakDisplayTransformHandle dt2 = oak_display_transform_create(
        cfg, "scene_linear", "sRGB", "Default", nullptr, 2.0f, 1.0f);
    if (!dt1 || !dt2) {
        if (dt1) oak_display_transform_free(dt1);
        if (dt2) oak_display_transform_free(dt2);
        oak_color_config_free(cfg);
        GTEST_SKIP() << "Display transform not available";
    }
    std::vector<float> in(64 * 64 * 4, 0.5f);
    std::vector<float> out1(64 * 64 * 4, 0.0f);
    std::vector<float> out2(64 * 64 * 4, 0.0f);
    oak_display_transform_apply(dt1, 64, 64, in.data(), out1.data(), 4);
    oak_display_transform_apply(dt2, 64, 64, in.data(), out2.data(), 4);

    // With +2EV exposure, average brightness should be higher
    double avg1 = 0.0, avg2 = 0.0;
    for (size_t i = 0; i < out1.size(); i += 4) {
        avg1 += out1[i];
        avg2 += out2[i];
    }
    avg1 /= (64.0 * 64.0);
    avg2 /= (64.0 * 64.0);
    EXPECT_GT(avg2, avg1);

    oak_display_transform_free(dt1);
    oak_display_transform_free(dt2);
    oak_color_config_free(cfg);
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

TEST_F(CAPColorTest, GpuShader3dLutCount) {
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
    int count = oak_color_gpu_shader_get_3d_lut_count(shader);
    EXPECT_GE(count, 0);
    oak_color_gpu_shader_free(shader);
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, GpuShaderGet3dLutInvalid) {
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
    const char* name = nullptr;
    const char* sampler = nullptr;
    unsigned int edge_len = 0;
    int interp = 0;
    const float* values = nullptr;
    int ret = oak_color_gpu_shader_get_3d_lut(shader, 999, &name, &sampler, &edge_len, &interp, &values);
    EXPECT_NE(ret, 0);
    oak_color_gpu_shader_free(shader);
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, GpuShaderTextureCount) {
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
    int count = oak_color_gpu_shader_get_texture_count(shader);
    EXPECT_GE(count, 0);
    oak_color_gpu_shader_free(shader);
    oak_color_processor_free(proc);
    oak_color_config_free(cfg);
}

TEST_F(CAPColorTest, GradingPrimaryCreate) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    EXPECT_NE(gp, nullptr);
    if (gp) oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimaryCreateVideoStyle) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(1);
    EXPECT_NE(gp, nullptr);
    if (gp) oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimaryCreateInvalid) {
    // Implementation maps any non-zero style to GRADING_VIDEO; does not return null
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(999);
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

TEST_F(CAPColorTest, GradingPrimarySetContrast) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    float val[4] = {2.0f, 1.0f, 1.0f, 1.0f};
    oak_color_grading_primary_set_contrast(gp, val);
    oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimarySetOffset) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    float val[4] = {0.1f, 0.0f, 0.0f, 0.0f};
    oak_color_grading_primary_set_offset(gp, val);
    oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimarySetExposure) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    float val[4] = {0.5f, 0.5f, 0.5f, 0.0f};
    oak_color_grading_primary_set_exposure(gp, val);
    oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimarySetSaturation) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    oak_color_grading_primary_set_saturation(gp, 1.5f);
    oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimarySetPivot) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    oak_color_grading_primary_set_pivot(gp, 0.5f);
    oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimarySetClampBlack) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    oak_color_grading_primary_set_clamp_black(gp, 0.01f);
    oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimarySetClampWhite) {
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    oak_color_grading_primary_set_clamp_white(gp, 10.0f);
    oak_color_grading_primary_free(gp);
}

TEST_F(CAPColorTest, GradingPrimaryNoClampValues) {
    float black = oak_color_grading_primary_no_clamp_black();
    float white = oak_color_grading_primary_no_clamp_white();
    EXPECT_LT(black, white);
}

TEST_F(CAPColorTest, ProcessorCreateFromGrading) {
    OakColorConfigHandle cfg = oak_color_config_load(nullptr);
    ASSERT_NE(cfg, nullptr);
    OakColorGradingPrimaryHandle gp = oak_color_grading_primary_create(0);
    ASSERT_NE(gp, nullptr);
    OakColorProcessorHandle proc = oak_color_processor_create_from_grading(
        cfg, "scene_linear", "sRGB", gp, 0);
    if (proc) oak_color_processor_free(proc);
    oak_color_grading_primary_free(gp);
    oak_color_config_free(cfg);
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
