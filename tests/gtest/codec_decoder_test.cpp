#include <gtest/gtest.h>

#include <QFileInfo>

#include "codec/decoder.h"

TEST(CodecDecoder, RetrieveVideoFrameFromDemoMp4)
{
	const QString path = QStringLiteral("tests/demo.mp4");
	ASSERT_TRUE(QFileInfo::exists(path));

	olive::DecoderPtr decoder = olive::Decoder::CreateFromID(QStringLiteral("ffmpeg"));
	ASSERT_TRUE(decoder);

	ASSERT_TRUE(decoder->Open(olive::Decoder::CodecStream(path, 0, nullptr)));

	olive::Decoder::RetrieveVideoParams params;
	params.time = olive::rational(0);
	params.maximum_format = olive::core::PixelFormat::U16;

	olive::FramePtr frame = decoder->RetrieveVideoFrame(params);
	ASSERT_TRUE(frame);
	ASSERT_TRUE(frame->is_allocated());
	EXPECT_EQ(frame->width(), 1920);
	EXPECT_EQ(frame->height(), 1080);
	EXPECT_EQ(frame->format(), olive::core::PixelFormat::U16);
	EXPECT_EQ(frame->channel_count(), 4);
	EXPECT_GT(frame->allocated_size(), 0);
	EXPECT_GT(frame->linesize_bytes(), 0);

	bool has_nonzero_byte = false;
	const char *data = frame->const_data();
	for (int i = 0; i < frame->allocated_size(); i++) {
		if (data[i] != 0) {
			has_nonzero_byte = true;
			break;
		}
	}
	EXPECT_TRUE(has_nonzero_byte);
}
