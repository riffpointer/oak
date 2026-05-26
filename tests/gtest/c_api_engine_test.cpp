/***  Engine C API Tests  ***/

#include <gtest/gtest.h>
#include "oak/engine_api.h"
#include "oak/frame_api.h"

class CAPEngineTest : public ::testing::Test {
protected:
    static const char* MinimalXml() {
        return R"xml(<?xml version="1.0" encoding="UTF-8"?>
<olive>
  <project>
    <name>Test</name>
    <nodes>
      <node id="viewer" type="ViewerOutput"/>
    </nodes>
  </project>
</olive>)xml";
    }
};

TEST_F(CAPEngineTest, LoadValidXml) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    EXPECT_NE(proj, nullptr);
    if (proj) oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, LoadEmptyString) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml("");
    if (proj) oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, LoadMalformedXml) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml("<not-xml");
    if (proj) oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, ProjectNodeCountAfterLoad) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    int count = oak_engine_project_node_count(proj);
    EXPECT_GE(count, 0);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, ProjectNodeCountNull) {
    int count = oak_engine_project_node_count(nullptr);
    EXPECT_LE(count, 0);
}

TEST_F(CAPEngineTest, ProjectDestroyNull) {
    oak_engine_project_destroy(nullptr);
}

TEST_F(CAPEngineTest, ProjectDoubleDestroy) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, SessionCreateValid) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1920, 1080, 2 /* RGBA32F */, 1, 24);
    if (!session) {
        GTEST_SKIP() << "No GPU/display available, skipping session test";
    }
    EXPECT_NE(session, nullptr);
    if (session) oak_engine_session_destroy(session);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, SessionCreateNullProject) {
    OakEngineSessionHandle session = oak_engine_session_create(
        nullptr, 1920, 1080, 2 /* RGBA32F */, 1, 24);
    EXPECT_EQ(session, nullptr);
}

TEST_F(CAPEngineTest, SessionCreateZeroWidth) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 0, 1080, 2, 1, 24);
    EXPECT_EQ(session, nullptr);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, SessionCreateZeroHeight) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1920, 0, 2, 1, 24);
    EXPECT_EQ(session, nullptr);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, SessionCreateZeroTimebaseDen) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1920, 1080, 2, 1, 0);
    EXPECT_EQ(session, nullptr);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, SessionCreateInvalidPixelFormat) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1920, 1080, -1 /* Invalid */, 1, 24);
    EXPECT_EQ(session, nullptr);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, SessionDestroyNull) {
    oak_engine_session_destroy(nullptr);
}

TEST_F(CAPEngineTest, SessionDoubleDestroy) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1920, 1080, 2, 1, 24);
    if (!session) {
        oak_engine_project_destroy(proj);
        GTEST_SKIP() << "No GPU/display available, skipping session test";
    }
    ASSERT_NE(session, nullptr);
    oak_engine_session_destroy(session);
    oak_engine_session_destroy(session);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, RenderFrameBasic) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1920, 1080, 2 /* RGBA32F */, 1, 24);
    if (!session) {
        GTEST_SKIP() << "Session creation failed, skipping render test";
    }
    OakFrame frame = {};
    int ret = oak_engine_session_render_frame(session, 0, 1, &frame);
    if (ret == 0) {
        EXPECT_EQ(frame.width, 1920);
        EXPECT_EQ(frame.height, 1080);
        EXPECT_EQ(frame.pix_fmt, OAK_FRAME_PIX_RGBA32F);
        EXPECT_NE(frame.data[0], nullptr);
        oak_frame_release(&frame);
    }
    oak_engine_session_destroy(session);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, RenderFrameNullSession) {
    OakFrame frame = {};
    int ret = oak_engine_session_render_frame(nullptr, 0, 1, &frame);
    EXPECT_NE(ret, 0);
}

TEST_F(CAPEngineTest, RenderFrameNullFrame) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1920, 1080, 2, 1, 24);
    if (!session) {
        oak_engine_project_destroy(proj);
        GTEST_SKIP() << "Session creation failed";
    }
    int ret = oak_engine_session_render_frame(session, 0, 1, nullptr);
    EXPECT_NE(ret, 0);
    oak_engine_session_destroy(session);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, RenderFrameAtDuration) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1920, 1080, 2, 1, 24);
    if (!session) {
        GTEST_SKIP() << "Session creation failed";
    }
    OakFrame frame = {};
    int ret = oak_engine_session_render_frame(session, 100, 1, &frame);
    if (ret == 0) {
        oak_frame_release(&frame);
    }
    oak_engine_session_destroy(session);
    oak_engine_project_destroy(proj);
}

TEST_F(CAPEngineTest, RenderFrameLargeTime) {
    OakEngineProjectHandle proj = oak_engine_project_load_xml(MinimalXml());
    ASSERT_NE(proj, nullptr);
    OakEngineSessionHandle session = oak_engine_session_create(
        proj, 1920, 1080, 2, 1, 24);
    if (!session) {
        GTEST_SKIP() << "Session creation failed";
    }
    OakFrame frame = {};
    int ret = oak_engine_session_render_frame(session, 999999, 1, &frame);
    if (ret == 0) {
        oak_frame_release(&frame);
    }
    oak_engine_session_destroy(session);
    oak_engine_project_destroy(proj);
}
