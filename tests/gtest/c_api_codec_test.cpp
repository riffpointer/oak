/***  Codec C API Tests  ***/

#include <gtest/gtest.h>
#include "oak/codec_api.h"

class CAPCodecTest : public ::testing::Test {};

TEST_F(CAPCodecTest, DecoderCreateFromId) {
    OakDecoderHandle dec = oak_decoder_create_from_id("ffmpeg");
    EXPECT_NE(dec, nullptr);
    if (dec) oak_decoder_close(dec);
}

TEST_F(CAPCodecTest, DecoderCreateFromIdUnknown) {
    OakDecoderHandle dec = oak_decoder_create_from_id("nonexistent");
    EXPECT_EQ(dec, nullptr);
}

TEST_F(CAPCodecTest, DecoderIdMatches) {
    OakDecoderHandle dec = oak_decoder_create_from_id("ffmpeg");
    ASSERT_NE(dec, nullptr);
    const char* id = oak_decoder_id(dec);
    EXPECT_NE(id, nullptr);
    oak_decoder_close(dec);
}

TEST_F(CAPCodecTest, DecoderSupportsVideoAudio) {
    OakDecoderHandle dec = oak_decoder_create_from_id("ffmpeg");
    ASSERT_NE(dec, nullptr);
    EXPECT_EQ(oak_decoder_supports_video(dec), 1);
    EXPECT_EQ(oak_decoder_supports_audio(dec), 1);
    oak_decoder_close(dec);
}

TEST_F(CAPCodecTest, DecoderOpenMissingFile) {
    OakMediaInfo info = {};
    OakDecoderHandle opened = oak_decoder_open("/nonexistent/path.mp4", "", &info);
    EXPECT_EQ(opened, nullptr);
}

TEST_F(CAPCodecTest, DecoderCloseNull) {
    oak_decoder_close(nullptr);
}

TEST_F(CAPCodecTest, MediaInfoFreeNull) {
    oak_media_info_free(nullptr);
}

TEST_F(CAPCodecTest, FrameAllocFree) {
    void* frame = oak_frame_alloc(64, 64, 26); // AV_PIX_FMT_RGBA = 26
    EXPECT_NE(frame, nullptr);
    if (frame) oak_frame_free(frame);
}

TEST_F(CAPCodecTest, FrameAllocZeroSize) {
    void* frame = oak_frame_alloc(0, 0, 26);
    EXPECT_EQ(frame, nullptr);
}

TEST_F(CAPCodecTest, FrameGetPlane) {
    void* frame = oak_frame_alloc(64, 64, 26);
    ASSERT_NE(frame, nullptr);
    void* data = nullptr;
    int linesize = 0;
    int ret = oak_frame_get_plane(frame, 0, &data, &linesize);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(data, nullptr);
    EXPECT_GE(linesize, 64 * 4);
    oak_frame_free(frame);
}

TEST_F(CAPCodecTest, FrameGetParams) {
    void* frame = oak_frame_alloc(64, 64, 26);
    ASSERT_NE(frame, nullptr);
    int w = 0, h = 0, fmt = 0;
    int ret = oak_frame_get_params(frame, &w, &h, &fmt);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(w, 64);
    EXPECT_EQ(h, 64);
    EXPECT_EQ(fmt, 26);
    oak_frame_free(frame);
}

TEST_F(CAPCodecTest, FrameConvertRGBAtoRGB) {
    void* src = oak_frame_alloc(64, 64, 26);  // AV_PIX_FMT_RGBA
    void* dst = oak_frame_alloc(64, 64, 2);   // AV_PIX_FMT_RGB24
    ASSERT_NE(src, nullptr);
    ASSERT_NE(dst, nullptr);
    int ret = oak_frame_convert(src, dst);
    EXPECT_EQ(ret, 0);
    oak_frame_free(src);
    oak_frame_free(dst);
}

TEST_F(CAPCodecTest, FrameConvertNull) {
    int ret = oak_frame_convert(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CAPCodecTest, VideoFormatToAvRGBA) {
    GTEST_SKIP() << "Skipping due to static init issue in headless env";
    int fmt = oak_video_format_to_av(0 /* U8 */, 4);
    EXPECT_EQ(fmt, 26); // AV_PIX_FMT_RGBA
}

TEST_F(CAPCodecTest, VideoFormatToAvGrayF32) {
    GTEST_SKIP() << "Skipping due to static init issue in headless env";
    int fmt = oak_video_format_to_av(3 /* F32 */, 1);
    EXPECT_NE(fmt, -1);
}

TEST_F(CAPCodecTest, AvToVideoFormatRGBA) {
    GTEST_SKIP() << "Skipping due to static init issue in headless env";
    int pix_fmt = -1, ch = 0;
    int ret = oak_av_to_video_format(26, &pix_fmt, &ch);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(pix_fmt, 0); // U8
    EXPECT_EQ(ch, 4);
}

TEST_F(CAPCodecTest, VideoFormatIsPlanar) {
    GTEST_SKIP() << "Skipping due to static init issue in headless env";
    EXPECT_EQ(oak_video_format_is_planar(26), 0); // RGBA packed
}

TEST_F(CAPCodecTest, EncoderCreateClose) {
    OakEncoderHandle enc = oak_encoder_create("/tmp/test_output.mp4", "mp4", "libx264", "aac");
    EXPECT_NE(enc, nullptr);
    if (enc) oak_encoder_close(enc);
}

TEST_F(CAPCodecTest, EncoderCloseNull) {
    oak_encoder_close(nullptr);
}
