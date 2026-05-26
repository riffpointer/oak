/***  PluginHost C API Tests  ***/

#include <gtest/gtest.h>
#include "oak/pluginhost_api.h"
#include <cstring>

class CAPPluginHostTest : public ::testing::Test {};

/* ------------------------------------------------------------------ */
/*  Host lifecycle                                                    */
/* ------------------------------------------------------------------ */

TEST_F(CAPPluginHostTest, HostCreateDestroy) {
    OakPluginHostHandle host = oak_plugin_host_create("TestHost");
    EXPECT_NE(host, nullptr);
    if (host) oak_plugin_host_destroy(host);
}

TEST_F(CAPPluginHostTest, HostCreateNullName) {
    OakPluginHostHandle host = oak_plugin_host_create(nullptr);
    EXPECT_NE(host, nullptr);
    if (host) oak_plugin_host_destroy(host);
}

TEST_F(CAPPluginHostTest, HostDestroyNull) {
    oak_plugin_host_destroy(nullptr);
}

TEST_F(CAPPluginHostTest, HostSetCapability) {
    OakPluginHostHandle host = oak_plugin_host_create("TestHost");
    ASSERT_NE(host, nullptr);
    // Should not crash even though it's a stub
    oak_plugin_host_set_capability(host, "max_image_size", 8192);
    oak_plugin_host_set_capability(host, "supports_multi_resolution", 1);
    oak_plugin_host_destroy(host);
}

TEST_F(CAPPluginHostTest, HostSetCapabilityNull) {
    // Should not crash
    oak_plugin_host_set_capability(nullptr, "test", 0);
}

/* ------------------------------------------------------------------ */
/*  Plugin loading (stubs — verify graceful failure)                  */
/* ------------------------------------------------------------------ */

TEST_F(CAPPluginHostTest, LoadBundleMissing) {
    OakPluginHostHandle host = oak_plugin_host_create("TestHost");
    ASSERT_NE(host, nullptr);
    int ret = oak_plugin_load_bundle(host, "/nonexistent/plugin.ofx.bundle");
    EXPECT_NE(ret, 0);
    oak_plugin_host_destroy(host);
}

TEST_F(CAPPluginHostTest, LoadFromPathMissing) {
    OakPluginHostHandle host = oak_plugin_host_create("TestHost");
    ASSERT_NE(host, nullptr);
    int ret = oak_plugin_load_from_path(host, "/nonexistent/plugins/");
    // Returns 0 as stub; just verify no crash
    (void)ret;
    oak_plugin_host_destroy(host);
}

TEST_F(CAPPluginHostTest, PluginCount) {
    OakPluginHostHandle host = oak_plugin_host_create("TestHost");
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(oak_plugin_count(host), 0);
    oak_plugin_host_destroy(host);
}

TEST_F(CAPPluginHostTest, PluginGetInfo) {
    OakPluginHostHandle host = oak_plugin_host_create("TestHost");
    ASSERT_NE(host, nullptr);
    OakPluginInfo info{};
    int ret = oak_plugin_get_info(host, 0, &info);
    EXPECT_NE(ret, 0);
    oak_plugin_host_destroy(host);
}

TEST_F(CAPPluginHostTest, PluginFind) {
    OakPluginHostHandle host = oak_plugin_host_create("TestHost");
    ASSERT_NE(host, nullptr);
    OakPluginHandle plugin = oak_plugin_find(host, "com.example.MyPlugin");
    EXPECT_EQ(plugin, nullptr);
    oak_plugin_host_destroy(host);
}

/* ------------------------------------------------------------------ */
/*  Plugin instance lifecycle (stubs)                                 */
/* ------------------------------------------------------------------ */

TEST_F(CAPPluginHostTest, InstanceCreateNullPlugin) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    EXPECT_EQ(inst, nullptr);
}

TEST_F(CAPPluginHostTest, InstanceDestroyNull) {
    // Should not crash
    oak_plugin_instance_destroy(nullptr);
}

TEST_F(CAPPluginHostTest, InstanceTime) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    // Since plugin is null, instance is null; time returns 0 for null
    EXPECT_EQ(oak_plugin_instance_time(inst), 0.0);
    oak_plugin_instance_destroy(inst);
}

TEST_F(CAPPluginHostTest, InstanceSetTime) {
    // Should not crash even with null
    oak_plugin_instance_set_time(nullptr, 1.5);
}

TEST_F(CAPPluginHostTest, InstanceRender) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    int ret = oak_plugin_instance_render(inst, 0.0, 1.0, 1.0, nullptr, 0, nullptr);
    EXPECT_NE(ret, 0);
    oak_plugin_instance_destroy(inst);
}

/* ------------------------------------------------------------------ */
/*  Parameter interaction (stubs)                                     */
/* ------------------------------------------------------------------ */

TEST_F(CAPPluginHostTest, ParamCount) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    EXPECT_EQ(oak_plugin_instance_param_count(inst), 0);
    oak_plugin_instance_destroy(inst);
}

TEST_F(CAPPluginHostTest, ParamInfo) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    const char* name = nullptr;
    const char* type = nullptr;
    int ret = oak_plugin_instance_param_info(inst, 0, &name, &type);
    EXPECT_NE(ret, 0);
    oak_plugin_instance_destroy(inst);
}

TEST_F(CAPPluginHostTest, ParamDouble) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    // Should not crash
    oak_plugin_instance_set_param_double(inst, "blur", 2.5);
    double val = oak_plugin_instance_get_param_double(inst, "blur");
    EXPECT_DOUBLE_EQ(val, 0.0); // stub returns 0
    oak_plugin_instance_destroy(inst);
}

TEST_F(CAPPluginHostTest, ParamInt) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    oak_plugin_instance_set_param_int(inst, "iterations", 3);
    EXPECT_EQ(oak_plugin_instance_get_param_int(inst, "iterations"), 0);
    oak_plugin_instance_destroy(inst);
}

TEST_F(CAPPluginHostTest, ParamString) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    oak_plugin_instance_set_param_string(inst, "label", "MyEffect");
    const char* val = oak_plugin_instance_get_param_string(inst, "label");
    EXPECT_STREQ(val, ""); // stub returns empty
    oak_plugin_instance_destroy(inst);
}

TEST_F(CAPPluginHostTest, ParamBool) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    oak_plugin_instance_set_param_bool(inst, "enabled", true);
    EXPECT_EQ(oak_plugin_instance_get_param_bool(inst, "enabled"), false);
    oak_plugin_instance_destroy(inst);
}

TEST_F(CAPPluginHostTest, ParamRgb) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    float rgb[3] = {1.0f, 0.5f, 0.0f};
    float out_rgb[3] = {0.0f, 0.0f, 0.0f};
    oak_plugin_instance_set_param_rgb(inst, "color", rgb);
    oak_plugin_instance_get_param_rgb(inst, "color", out_rgb);
    EXPECT_FLOAT_EQ(out_rgb[0], 0.0f);
    EXPECT_FLOAT_EQ(out_rgb[1], 0.0f);
    EXPECT_FLOAT_EQ(out_rgb[2], 0.0f);
    oak_plugin_instance_destroy(inst);
}

TEST_F(CAPPluginHostTest, ParamRgba) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    float rgba[4] = {1.0f, 0.5f, 0.0f, 1.0f};
    float out_rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    oak_plugin_instance_set_param_rgba(inst, "tint", rgba);
    oak_plugin_instance_get_param_rgba(inst, "tint", out_rgba);
    EXPECT_FLOAT_EQ(out_rgba[0], 0.0f);
    EXPECT_FLOAT_EQ(out_rgba[1], 0.0f);
    EXPECT_FLOAT_EQ(out_rgba[2], 0.0f);
    EXPECT_FLOAT_EQ(out_rgba[3], 1.0f); // stub returns alpha=1
    oak_plugin_instance_destroy(inst);
}

/* ------------------------------------------------------------------ */
/*  Timeline callbacks                                                */
/* ------------------------------------------------------------------ */

TEST_F(CAPPluginHostTest, SetTimelineCallbacks) {
    OakPluginInstanceHandle inst = oak_plugin_instance_create(
        nullptr, "Filter", 1920, 1080, 1.0, 24, 1);
    OakPluginTimelineCallbacks cbs{};
    oak_plugin_instance_set_timeline_callbacks(inst, &cbs, nullptr);
    oak_plugin_instance_set_timeline_callbacks(inst, nullptr, nullptr);
    oak_plugin_instance_destroy(inst);
}

TEST_F(CAPPluginHostTest, SetTimelineCallbacksNullInstance) {
    OakPluginTimelineCallbacks cbs{};
    // Should not crash
    oak_plugin_instance_set_timeline_callbacks(nullptr, &cbs, nullptr);
}
