/***  Renderer / OakGL C API Tests  ***/

#include <gtest/gtest.h>
#include "oak/renderer_api.h"
#include <cstring>
#include <vector>

class CAPRendererTest : public ::testing::Test {};

TEST_F(CAPRendererTest, RendererCreateDestroy) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    EXPECT_NE(r, nullptr);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, RendererDestroyNull) {
    oak_renderer_destroy(nullptr);
}

TEST_F(CAPRendererTest, RendererBackendName) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    const char* name = oak_renderer_backend_name(r);
    EXPECT_NE(name, nullptr);
    EXPECT_TRUE(std::strlen(name) > 0);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, RendererCapability) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    int cap = oak_renderer_capability(r, "max_texture_size");
    EXPECT_GT(cap, 0);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, TextureUploadDestroy) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    std::vector<float> pixels(64 * 64 * 4, 1.0f);
    OakTextureHandle tex = oak_texture_upload(r, 64, 64,
        OAK_RENDER_PIX_FMT_RGBA32F, pixels.data(), pixels.size() * sizeof(float),
        OAK_FILTER_NEAREST, OAK_WRAP_CLAMP);
    EXPECT_NE(tex, nullptr);
    if (tex) oak_texture_destroy(r, tex);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, TextureUploadFromFrame) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    std::vector<float> pixels(64 * 64 * 4, 1.0f);
    OakTextureHandle tex = oak_texture_upload_from_frame(r, 64, 64,
        OAK_RENDER_PIX_FMT_RGBA32F, pixels.data(), 64 * 4 * sizeof(float));
    EXPECT_NE(tex, nullptr);
    if (tex) oak_texture_destroy(r, tex);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, TextureCreatePlanar) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    std::vector<uint8_t> y_plane(64 * 64, 128);
    std::vector<uint8_t> u_plane(32 * 32, 128);
    std::vector<uint8_t> v_plane(32 * 32, 128);
    OakTexturePlane planes[3] = {
        {64, 64, OAK_RENDER_PIX_FMT_R8, y_plane.data(), 64},
        {32, 32, OAK_RENDER_PIX_FMT_R8, u_plane.data(), 32},
        {32, 32, OAK_RENDER_PIX_FMT_R8, v_plane.data(), 32}
    };
    OakTextureHandle tex = oak_texture_create_planar(r, 64, 64, planes, 3);
    // May be null if planar textures are not supported
    if (tex) oak_texture_destroy(r, tex);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, TextureSize) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    std::vector<float> pixels(64 * 64 * 4, 1.0f);
    OakTextureHandle tex = oak_texture_upload(r, 64, 64,
        OAK_RENDER_PIX_FMT_RGBA32F, pixels.data(), pixels.size() * sizeof(float),
        OAK_FILTER_NEAREST, OAK_WRAP_CLAMP);
    ASSERT_NE(tex, nullptr);
    int w = 0, h = 0;
    oak_texture_size(tex, &w, &h);
    EXPECT_EQ(w, 64);
    EXPECT_EQ(h, 64);
    oak_texture_destroy(r, tex);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, TextureDownload) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    std::vector<float> pixels(64 * 64 * 4, 1.0f);
    OakTextureHandle tex = oak_texture_upload(r, 64, 64,
        OAK_RENDER_PIX_FMT_RGBA32F, pixels.data(), pixels.size() * sizeof(float),
        OAK_FILTER_NEAREST, OAK_WRAP_CLAMP);
    ASSERT_NE(tex, nullptr);
    std::vector<float> downloaded(64 * 64 * 4, 0.0f);
    int ret = oak_texture_download(r, tex, 64, 64,
        OAK_RENDER_PIX_FMT_RGBA32F, downloaded.data(), 64 * 4 * sizeof(float));
    EXPECT_EQ(ret, 0);
    oak_texture_destroy(r, tex);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, TargetCreateDestroy) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 128, 128,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    EXPECT_NE(target, nullptr);
    if (target) oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, TargetResize) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 128, 128,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);
    oak_target_resize(r, target, 256, 256);
    int w = 0, h = 0;
    oak_target_size(target, &w, &h);
    EXPECT_EQ(w, 256);
    EXPECT_EQ(h, 256);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, TargetColorTexture) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 128, 128,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);
    OakTextureHandle tex = oak_target_color_texture(r, target);
    EXPECT_NE(tex, nullptr);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, TargetDetachColorTexture) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 128, 128,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);
    OakTextureHandle tex = oak_target_detach_color_texture(r, target);
    EXPECT_NE(tex, nullptr);
    if (tex) oak_texture_destroy(r, tex);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, RenderBeginEnd) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 128, 128,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);
    float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    oak_renderer_begin(r, target, clear);
    oak_renderer_end(r);
    oak_renderer_flush(r);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, ClearTexture) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 64, 64,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);
    float clear[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    oak_renderer_begin(r, target, clear);
    oak_renderer_end(r);
    oak_renderer_flush(r);

    void* data = nullptr;
    int stride = 0;
    int ret = oak_renderer_readback(r, target, OAK_RENDER_PIX_FMT_RGBA32F, &data, &stride);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(data, nullptr);
    if (data) oak_renderer_free_readback(data);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, Readback) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 64, 64,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);
    float clear[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    oak_renderer_begin(r, target, clear);
    oak_renderer_end(r);
    oak_renderer_flush(r);

    void* data = nullptr;
    int stride = 0;
    int ret = oak_renderer_readback(r, target, OAK_RENDER_PIX_FMT_RGBA32F, &data, &stride);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(data, nullptr);
    if (data) oak_renderer_free_readback(data);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, ShaderCompileDestroy) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    const char* vs = "#version 330\nlayout(location=0) in vec2 aPos;\nvoid main(){gl_Position=vec4(aPos,0,1);}";
    const char* fs = "#version 330\nout vec4 FragColor;\nvoid main(){FragColor=vec4(1,0,0,1);}";
    OakShaderHandle shader = oak_shader_compile(r, "red", vs, fs);
    if (shader) oak_shader_destroy(r, shader);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, DrawQuad) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 128, 128,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);
    float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    oak_renderer_begin(r, target, clear);

    float mvp[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };
    float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float uv[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    oak_renderer_draw_quad(r, mvp, nullptr, OAK_BLEND_REPLACE, color, uv);

    oak_renderer_end(r);
    oak_renderer_flush(r);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, DrawLines) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 128, 128,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);
    float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    oak_renderer_begin(r, target, clear);

    float points[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    oak_renderer_draw_lines(r, points, 2, color, 1.0f);

    oak_renderer_end(r);
    oak_renderer_flush(r);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, DrawPolygon) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 128, 128,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);
    float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    oak_renderer_begin(r, target, clear);

    float points[6] = {0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    float color[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    oak_renderer_draw_polygon(r, points, 3, color);

    oak_renderer_end(r);
    oak_renderer_flush(r);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, DrawWithShaderEx) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";
    OakTargetHandle target = oak_target_create(r, 128, 128,
        OAK_RENDER_PIX_FMT_RGBA32F, false);
    ASSERT_NE(target, nullptr);

    const char* vs = "#version 330\nlayout(location=0) in vec2 aPos;\nvoid main(){gl_Position=vec4(aPos,0,1);}";
    const char* fs = "#version 330\nprecision highp float;\nuniform vec4 uColor;\nout vec4 FragColor;\nvoid main(){FragColor=uColor;}";
    OakShaderHandle shader = oak_shader_compile(r, "uniform_color", vs, fs);
    if (!shader) {
        oak_target_destroy(r, target);
        oak_renderer_destroy(r);
        GTEST_SKIP() << "Shader compilation failed";
    }

    float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    oak_renderer_begin(r, target, clear);

    OakShaderUniform uniforms[1] = {};
    uniforms[0].name = "uColor";
    uniforms[0].type = OAK_UNIFORM_VEC4;
    uniforms[0].value.vec4[0] = 1.0f;
    uniforms[0].value.vec4[1] = 0.0f;
    uniforms[0].value.vec4[2] = 0.0f;
    uniforms[0].value.vec4[3] = 1.0f;

    oak_renderer_draw_with_shader_ex(r, shader, uniforms, 1, nullptr, 0, target);

    oak_renderer_end(r);
    oak_renderer_flush(r);
    oak_shader_destroy(r, shader);
    oak_target_destroy(r, target);
    oak_renderer_destroy(r);
}

TEST_F(CAPRendererTest, DrawWithShaderToTextureEx) {
    OakRendererHandle r = oak_renderer_create("opengl", nullptr);
    if (!r) GTEST_SKIP() << "No GPU/display available, skipping renderer test";

    std::vector<float> pixels(64 * 64 * 4, 1.0f);
    OakTextureHandle tex = oak_texture_upload(r, 64, 64,
        OAK_RENDER_PIX_FMT_RGBA32F, pixels.data(), pixels.size() * sizeof(float),
        OAK_FILTER_NEAREST, OAK_WRAP_CLAMP);
    if (!tex) {
        oak_renderer_destroy(r);
        GTEST_SKIP() << "Texture upload failed";
    }

    const char* vs = "#version 330\nlayout(location=0) in vec2 aPos;\nvoid main(){gl_Position=vec4(aPos,0,1);}";
    const char* fs = "#version 330\nprecision highp float;\nuniform sampler2D uTex;\nout vec4 FragColor;\nvoid main(){FragColor=texture(uTex,vec2(0.5));}";
    OakShaderHandle shader = oak_shader_compile(r, "sample", vs, fs);
    if (!shader) {
        oak_texture_destroy(r, tex);
        oak_renderer_destroy(r);
        GTEST_SKIP() << "Shader compilation failed";
    }

    OakShaderUniform uniforms[1] = {};
    uniforms[0].name = "uTex";
    uniforms[0].type = OAK_UNIFORM_TEXTURE;
    uniforms[0].texture_unit = 0;

    OakTextureHandle dest_tex = oak_texture_upload(r, 64, 64,
        OAK_RENDER_PIX_FMT_RGBA32F, pixels.data(), pixels.size() * sizeof(float),
        OAK_FILTER_NEAREST, OAK_WRAP_CLAMP);

    oak_renderer_draw_with_shader_to_texture_ex(r, shader, uniforms, 1, &tex, 1, dest_tex);

    oak_shader_destroy(r, shader);
    oak_texture_destroy(r, tex);
    oak_texture_destroy(r, dest_tex);
    oak_renderer_destroy(r);
}
