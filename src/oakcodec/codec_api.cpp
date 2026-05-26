/***

  oakcodec.so C API Implementation (v2 — OakFrame + ACEScg)
  Copyright (C) 2025 mikesolar

***/

#include "oak/codec_api.h"
#include "oak/renderer_api.h"
#include "oakcodec_internal.h"

#include <cstring>
#include <cstdlib>

#if defined(__APPLE__)
#include <dlfcn.h>
#elif defined(__linux__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include "decoder.h"
#include "encoder.h"
#include "conformmanager.h"
#include "ffmpeg/ffmpegdecoder.h"
#include "oiio/oiiodecoder.h"
#include "ffmpeg/ffmpegencoder.h"
#include "oiio/oiioencoder.h"
#include "ffmpeg_utils.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
}

/* ================================================================ */
/*  Conform callback bridge                                           */
/* ================================================================ */

static OakConformReadyCallback g_conform_ready_cb = nullptr;
static void* g_conform_ready_userdata = nullptr;
static bool g_conform_callback_connected = false;

static void EnsureConformCallbackConnected()
{
    if (g_conform_callback_connected) return;
    olive::ConformManager *cm = olive::ConformManager::instance();
    if (!cm) return;
    QObject::connect(cm, &olive::ConformManager::ConformReady, [cm]() {
        (void)cm;
        if (g_conform_ready_cb) {
            g_conform_ready_cb(g_conform_ready_userdata);
        }
    });
    g_conform_callback_connected = true;
}

/* ================================================================ */
/*  Helpers                                                          */
/* ================================================================ */

static olive::PixelFormat OakFramePixFmtToOlive(OakFramePixelFormat fmt)
{
    switch (fmt) {
    case OAK_FRAME_PIX_RGBA8:    return olive::PixelFormat::U8;
    case OAK_FRAME_PIX_RGBA16:   return olive::PixelFormat::U16;
    case OAK_FRAME_PIX_RGBA32F:  return olive::PixelFormat::F32;
    default:                     return olive::PixelFormat::INVALID;
    }
}

static OakFramePixelFormat OlivePixFmtToOakFrame(olive::PixelFormat fmt, int channels)
{
    if (channels == 4) {
        switch (fmt) {
        case olive::PixelFormat::U8:  return OAK_FRAME_PIX_RGBA8;
        case olive::PixelFormat::U16: return OAK_FRAME_PIX_RGBA16;
        case olive::PixelFormat::F32: return OAK_FRAME_PIX_RGBA32F;
        default: break;
        }
    }
    return OAK_FRAME_PIX_INVALID;
}

static olive::SampleFormat OakAudioFmtToOlive(OakAudioFormat fmt)
{
    switch (fmt) {
    case OAK_AUDIO_FMT_U8:  return olive::SampleFormat::U8;
    case OAK_AUDIO_FMT_S16: return olive::SampleFormat::S16;
    case OAK_AUDIO_FMT_S32: return olive::SampleFormat::S32;
    case OAK_AUDIO_FMT_FLT: return olive::SampleFormat::F32;
    case OAK_AUDIO_FMT_DBL: return olive::SampleFormat::F64;
    default:                return olive::SampleFormat::INVALID;
    }
}

static OakAudioFormat OliveAudioFmtToOak(olive::SampleFormat fmt)
{
    switch (fmt) {
    case olive::SampleFormat::U8:  return OAK_AUDIO_FMT_U8;
    case olive::SampleFormat::S16: return OAK_AUDIO_FMT_S16;
    case olive::SampleFormat::S32: return OAK_AUDIO_FMT_S32;
    case olive::SampleFormat::F32: return OAK_AUDIO_FMT_FLT;
    case olive::SampleFormat::F64: return OAK_AUDIO_FMT_DBL;
    default:                       return OAK_AUDIO_FMT_INVALID;
    }
}

/* ================================================================ */
/*  Runtime oakgl loader (for GPU zero-copy)                         */
/* ================================================================ */

struct OakGLFunctions {
    void* handle = nullptr;

    typedef void* (*tex_upload_fn)(void* renderer, int w, int h, int fmt,
                                   const void* data, int stride);
    typedef void* (*tex_create_planar_fn)(void* renderer, int w, int h,
                                          void* planes, int plane_count);
    typedef void* (*tex_wrap_ext_fn)(void* renderer, int w, int h, int fmt,
                                     void* ext_handle, const char* ext_type);
    typedef void  (*tex_destroy_fn)(void* renderer, void* tex);
    typedef void  (*blit_yuv_fn)(void* renderer, void* y, void* u, void* v,
                                 void* dest, int w, int h,
                                 const float* mat, bool full, int pix_fmt);
    typedef int   (*readback_frame_fn)(void* renderer, void* src, int src_type,
                                       int out_fmt, OakFrame* out_frame);
    typedef void  (*free_readback_fn)(void* data);

    tex_upload_fn       tex_upload = nullptr;
    tex_create_planar_fn tex_planar = nullptr;
    tex_wrap_ext_fn     tex_wrap = nullptr;
    tex_destroy_fn      tex_destroy = nullptr;
    blit_yuv_fn         blit_yuv = nullptr;
    readback_frame_fn   readback_frame = nullptr;
    free_readback_fn    free_readback = nullptr;

    bool Load() {
        if (handle) return true;
#if defined(__APPLE__)
        handle = dlopen("liboakgl.dylib", RTLD_NOW | RTLD_LOCAL);
#elif defined(__linux__)
        handle = dlopen("liboakgl.so", RTLD_NOW | RTLD_LOCAL);
#elif defined(_WIN32)
        handle = (void*)LoadLibraryA("oakgl.dll");
#endif
        if (!handle) return false;

#define LOAD_SYM(name, type, field) \
    field = reinterpret_cast<type>(dlsym(handle, name));

        LOAD_SYM("oak_texture_upload_from_frame", tex_upload_fn, tex_upload);
        LOAD_SYM("oak_texture_create_planar", tex_create_planar_fn, tex_planar);
        LOAD_SYM("oak_texture_wrap_external", tex_wrap_ext_fn, tex_wrap);
        LOAD_SYM("oak_texture_destroy", tex_destroy_fn, tex_destroy);
        LOAD_SYM("oak_renderer_blit_yuv_to_rgba", blit_yuv_fn, blit_yuv);
        LOAD_SYM("oak_renderer_readback_frame", readback_frame_fn, readback_frame);
        LOAD_SYM("oak_renderer_free_readback", free_readback_fn, free_readback);
#undef LOAD_SYM

        // Only tex_upload is mandatory for basic GPU path
        return tex_upload != nullptr;
    }

    ~OakGLFunctions() {
        if (handle) {
#if defined(__APPLE__) || defined(__linux__)
            dlclose(handle);
#elif defined(_WIN32)
            FreeLibrary((HMODULE)handle);
#endif
        }
    }
};

static OakGLFunctions* GetOakGL() {
    static OakGLFunctions g_oakgl;
    if (!g_oakgl.handle) {
        g_oakgl.Load();
    }
    return &g_oakgl;
}

/* ================================================================ */
/*  OakFrame lifecycle                                                 */
/* ================================================================ */

void oak_frame_release(OakFrame* frame)
{
    if (!frame) return;

    if (frame->storage == OAK_FRAME_GPU) {
        OakGLFunctions* gl = GetOakGL();
        if (gl && gl->tex_destroy) {
            for (int i = 0; i < frame->planes; i++) {
                if (frame->data[i]) {
                    gl->tex_destroy(nullptr, frame->data[i]);
                }
            }
        }
    }

    if (frame->internal) {
        auto* avp = reinterpret_cast<olive::AVFramePtr*>(frame->internal);
        delete avp;
        frame->internal = nullptr;
    }

    std::memset(frame, 0, sizeof(OakFrame));
}

void oak_frame_release_internal_only(OakFrame* frame)
{
    if (!frame || !frame->internal) return;
    auto* avp = reinterpret_cast<olive::AVFramePtr*>(frame->internal);
    delete avp;
    frame->internal = nullptr;
}

/* ================================================================ */
/*  Decoder lifecycle                                                 */
/* ================================================================ */

OakDecoderHandle oak_decoder_open(const char *filepath, const char *codec_hint,
                                  OakMediaInfo *out_info)
{
    (void)codec_hint;
    if (!filepath) return nullptr;

    QString fn = QString::fromUtf8(filepath);
    olive::CancelAtom cancel;

    QVector<olive::DecoderPtr> all_decoders = olive::Decoder::ReceiveListOfAllDecoders();
    olive::DecoderPtr selected;
    olive::FootageDescription desc;

    for (const olive::DecoderPtr &d : all_decoders) {
        desc = d->Probe(fn, &cancel);
        if (desc.IsValid()) {
            selected = d;
            break;
        }
    }

    if (!selected) return nullptr;

    auto* wrapper = new oakcodec::OakDecoderWrapper();
    wrapper->filepath = fn;
    wrapper->desc = desc;
    wrapper->decoder = selected;

    bool opened = false;
    if (!desc.GetVideoStreams().isEmpty()) {
        const olive::VideoParams &vp = desc.GetVideoStreams().first();
        olive::Decoder::CodecStream stream(fn, vp.stream_index());
        opened = wrapper->decoder->Open(stream);
    } else if (!desc.GetAudioStreams().isEmpty()) {
        const olive::AudioParams &ap = desc.GetAudioStreams().first();
        olive::Decoder::CodecStream stream(fn, ap.stream_index());
        opened = wrapper->decoder->Open(stream);
    }

    if (!opened) {
        delete wrapper;
        return nullptr;
    }
    wrapper->opened = true;

    if (out_info) {
        out_info->video_stream_count = desc.GetVideoStreams().size();
        out_info->audio_stream_count = desc.GetAudioStreams().size();
        out_info->subtitle_stream_count = desc.GetSubtitleStreams().size();

        if (out_info->video_stream_count > 0) {
            out_info->video_streams = new OakVideoStreamInfo[out_info->video_stream_count];
            for (int i = 0; i < out_info->video_stream_count; i++) {
                const olive::VideoParams &vp = desc.GetVideoStreams().at(i);
                out_info->video_streams[i] = {
                    vp.width(), vp.height(),
                    OlivePixFmtToOakFrame(vp.format(), vp.channel_count()),
                    vp.time_base().numerator(), vp.time_base().denominator(),
                    vp.frame_rate().toDouble(), vp.duration()
                };
            }
        } else {
            out_info->video_streams = nullptr;
        }

        if (out_info->audio_stream_count > 0) {
            out_info->audio_streams = new OakAudioStreamInfo[out_info->audio_stream_count];
            for (int i = 0; i < out_info->audio_stream_count; i++) {
                const olive::AudioParams &ap = desc.GetAudioStreams().at(i);
                out_info->audio_streams[i] = {
                    ap.sample_rate(), ap.channel_count(),
                    OliveAudioFmtToOak(ap.format()),
                    ap.time_base().numerator(), ap.time_base().denominator(),
                    ap.duration()
                };
            }
        } else {
            out_info->audio_streams = nullptr;
        }
    }

    return reinterpret_cast<OakDecoderHandle>(wrapper);
}

void oak_decoder_close(OakDecoderHandle decoder)
{
    if (!decoder) return;
    auto *wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper *>(decoder);
    if (wrapper->decoder) {
        wrapper->decoder->Close();
    }
    if (wrapper->color_processor) {
        oak_color_processor_free(wrapper->color_processor);
    }
    if (wrapper->color_config) {
        oak_color_config_free(wrapper->color_config);
    }
    wrapper->opened = false;
    delete wrapper;
}

void oak_media_info_free(OakMediaInfo *info)
{
    if (!info) return;
    delete[] info->video_streams;
    info->video_streams = nullptr;
    delete[] info->audio_streams;
    info->audio_streams = nullptr;
    info->video_stream_count = 0;
    info->audio_stream_count = 0;
}

/* ================================================================ */
/*  Planar YUV helpers for GPU zero-copy                               */
/* ================================================================ */

static const char* AVColorSpaceToOCIOName(AVColorSpace cs)
{
    switch (cs) {
    case AVCOL_SPC_BT709:       return "Rec.709";
    case AVCOL_SPC_BT470BG:     return "Rec.601 (PAL)";
    case AVCOL_SPC_SMPTE170M:   return "Rec.601 (NTSC)";
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:   return "Rec.2020";
    case AVCOL_SPC_SMPTE240M:   return "SMPTE 240M";
    case AVCOL_SPC_FCC:         return "FCC";
    case AVCOL_SPC_RGB:         return "Linear";
    default:                    return nullptr;
    }
}

static bool IsPlanarYUVFormat(AVPixelFormat fmt)
{
    return fmt == AV_PIX_FMT_YUV420P || fmt == AV_PIX_FMT_YUV422P ||
           fmt == AV_PIX_FMT_YUV444P || fmt == AV_PIX_FMT_YUVJ420P ||
           fmt == AV_PIX_FMT_YUVJ422P || fmt == AV_PIX_FMT_YUVJ444P ||
           fmt == AV_PIX_FMT_YUVA420P || fmt == AV_PIX_FMT_YUVA422P ||
           fmt == AV_PIX_FMT_YUVA444P || fmt == AV_PIX_FMT_YUV420P10LE ||
           fmt == AV_PIX_FMT_YUV422P10LE || fmt == AV_PIX_FMT_YUV444P10LE ||
           fmt == AV_PIX_FMT_YUV420P12LE || fmt == AV_PIX_FMT_YUV422P12LE ||
           fmt == AV_PIX_FMT_YUV444P12LE || fmt == AV_PIX_FMT_YUV420P16LE ||
           fmt == AV_PIX_FMT_YUV422P16LE || fmt == AV_PIX_FMT_YUV444P16LE;
}

static void GetYUVColorMatrix(AVColorSpace cs, AVColorRange range, float mat3x3[9], bool* full_range)
{
    *full_range = (range == AVCOL_RANGE_JPEG);

    // Default BT.709 limited
    float m[9] = {
        1.0f,  0.0f,      1.5748f,
        1.0f, -0.1873f,  -0.4681f,
        1.0f,  1.8556f,   0.0f
    };

    switch (cs) {
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        // BT.601
        m[2] =  1.4020f;  m[3] = 1.0f;
        m[4] = -0.3441f;  m[5] = -0.7141f;
        m[7] =  1.7720f;  m[8] = 0.0f;
        break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        // BT.2020
        m[2] =  1.4746f;  m[3] = 1.0f;
        m[4] = -0.1645f;  m[5] = -0.5713f;
        m[7] =  1.8814f;  m[8] = 0.0f;
        break;
    case AVCOL_SPC_BT709:
    default:
        // BT.709 (default)
        break;
    }
    std::memcpy(mat3x3, m, sizeof(m));
}

/* ================================================================ */
/*  Video decode (v2 — OakFrame)                                      */
/* ================================================================ */

int oak_decoder_read_video(OakDecoderHandle decoder, int stream_index,
                           int64_t time_num, int64_t time_den,
                           void* renderer_hint,
                           OakFrame* out_frame)
{
    (void)stream_index;
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder || !out_frame) return -1;

    olive::Decoder::RetrieveVideoParams p;
    p.time = olive::core::rational(time_num, time_den);
    p.maximum_format = olive::PixelFormat::F32;  // default full F32
    p.renderer = renderer_hint;

    olive::AVFramePtr frame = wrapper->decoder->RetrieveVideo(p);
    if (!frame) return -1;

    std::memset(out_frame, 0, sizeof(OakFrame));
    out_frame->width = frame->width;
    out_frame->height = frame->height;
    out_frame->colorspace = "ACES - ACEScg";
    out_frame->pts_num = time_num;
    out_frame->pts_den = time_den;

    // Store AVFramePtr on heap so OakFrame can reference it
    auto* frame_holder = new olive::AVFramePtr(frame);
    out_frame->internal = frame_holder;

    if (frame->format == AV_PIX_FMT_RGBAF32LE ||
        frame->format == AV_PIX_FMT_RGBAF32BE) {
        out_frame->pix_fmt = OAK_FRAME_PIX_RGBA32F;
    } else if (frame->format == AV_PIX_FMT_RGBA) {
        out_frame->pix_fmt = OAK_FRAME_PIX_RGBA8;
    } else {
        out_frame->pix_fmt = OAK_FRAME_PIX_INVALID;
    }

    if (renderer_hint) {
        OakGLFunctions* gl = GetOakGL();
        if (gl && gl->tex_upload && gl->blit_yuv) {
            AVPixelFormat avfmt = static_cast<AVPixelFormat>(frame->format);
            if (IsPlanarYUVFormat(avfmt)) {
                // Upload Y/U/V as individual R8 textures
                int uv_w = frame->width;
                int uv_h = frame->height;
                if (avfmt == AV_PIX_FMT_YUV420P || avfmt == AV_PIX_FMT_YUVJ420P ||
                    avfmt == AV_PIX_FMT_YUVA420P || avfmt == AV_PIX_FMT_YUV420P10LE ||
                    avfmt == AV_PIX_FMT_YUV420P12LE || avfmt == AV_PIX_FMT_YUV420P16LE) {
                    uv_w = (frame->width + 1) / 2;
                    uv_h = (frame->height + 1) / 2;
                } else if (avfmt == AV_PIX_FMT_YUV422P || avfmt == AV_PIX_FMT_YUVJ422P ||
                           avfmt == AV_PIX_FMT_YUVA422P || avfmt == AV_PIX_FMT_YUV422P10LE ||
                           avfmt == AV_PIX_FMT_YUV422P12LE || avfmt == AV_PIX_FMT_YUV422P16LE) {
                    uv_w = (frame->width + 1) / 2;
                }

                void* y_tex = gl->tex_upload(renderer_hint, frame->width, frame->height,
                                             OAK_RENDER_PIX_FMT_R8,
                                             frame->data[0], frame->linesize[0]);
                void* u_tex = gl->tex_upload(renderer_hint, uv_w, uv_h,
                                             OAK_RENDER_PIX_FMT_R8,
                                             frame->data[1], frame->linesize[1]);
                void* v_tex = gl->tex_upload(renderer_hint, uv_w, uv_h,
                                             OAK_RENDER_PIX_FMT_R8,
                                             frame->data[2], frame->linesize[2]);

                if (y_tex && u_tex && v_tex) {
                    // Create target for blit output
                    OakTargetHandle target = oak_target_create(
                        reinterpret_cast<OakRendererHandle>(renderer_hint),
                        frame->width, frame->height,
                        OAK_RENDER_PIX_FMT_RGBA32F, false);

                    if (target) {
                        float mat3x3[9];
                        bool full_range;
                        GetYUVColorMatrix(frame->colorspace, frame->color_range, mat3x3, &full_range);

                        gl->blit_yuv(renderer_hint, y_tex, u_tex, v_tex, target,
                                     frame->width, frame->height,
                                     mat3x3, full_range, OAK_FRAME_PIX_INVALID);

                        // Detach color texture so we can destroy target without losing it
                        OakTextureHandle color_tex = oak_target_detach_color_texture(
                            reinterpret_cast<OakRendererHandle>(renderer_hint), target);

                        oak_target_destroy(reinterpret_cast<OakRendererHandle>(renderer_hint), target);

                        // Destroy planar input textures
                        gl->tex_destroy(renderer_hint, y_tex);
                        gl->tex_destroy(renderer_hint, u_tex);
                        gl->tex_destroy(renderer_hint, v_tex);

                        if (color_tex) {
                            out_frame->storage = OAK_FRAME_GPU;
                            out_frame->planes = 1;
                            out_frame->pix_fmt = OAK_FRAME_PIX_RGBA32F;
                            out_frame->data[0] = color_tex;
                            out_frame->stride[0] = 0;
                            return 0;
                        }
                    }
                }

                // Cleanup on failure
                if (y_tex) gl->tex_destroy(renderer_hint, y_tex);
                if (u_tex) gl->tex_destroy(renderer_hint, u_tex);
                if (v_tex) gl->tex_destroy(renderer_hint, v_tex);
                // Fall through to CPU path
            } else {
                // Packed RGBA path
                OakRenderPixelFormat render_fmt = OAK_RENDER_PIX_FMT_RGBA32F;
                if (out_frame->pix_fmt == OAK_FRAME_PIX_RGBA8)  render_fmt = OAK_RENDER_PIX_FMT_RGBA8;
                else if (out_frame->pix_fmt == OAK_FRAME_PIX_RGBA16) render_fmt = OAK_RENDER_PIX_FMT_RGBA16;

                void* tex = gl->tex_upload(renderer_hint, frame->width, frame->height,
                                           render_fmt,
                                           frame->data[0], frame->linesize[0]);
                if (tex) {
                    out_frame->storage = OAK_FRAME_GPU;
                    out_frame->planes = 1;
                    out_frame->data[0] = tex;
                    out_frame->stride[0] = 0;
                    return 0;
                }
                // Upload failed, fall through to CPU path
            }
        }
    }

    // CPU path
    out_frame->storage = OAK_FRAME_CPU;
    out_frame->planes = 1;
    out_frame->data[0] = frame->data[0];
    out_frame->stride[0] = frame->linesize[0];

    // IDT: convert source colorspace to ACEScg if needed
    const char* src_cs_name = AVColorSpaceToOCIOName(frame->colorspace);
    if (!src_cs_name) {
        // Fallback: try to infer from color primaries
        if (frame->color_primaries == AVCOL_PRI_BT709) src_cs_name = "Rec.709";
        else if (frame->color_primaries == AVCOL_PRI_BT2020) src_cs_name = "Rec.2020";
        else if (frame->color_primaries == AVCOL_PRI_SMPTE170M) src_cs_name = "Rec.601 (NTSC)";
        else if (frame->color_primaries == AVCOL_PRI_SMPTE240M) src_cs_name = "SMPTE 240M";
        else src_cs_name = "Rec.709"; // safest default
    }

    if (std::strcmp(src_cs_name, "ACES - ACEScg") != 0) {
        if (!wrapper->color_config) {
            wrapper->color_config = oak_color_config_load(nullptr);
        }
        if (wrapper->color_config) {
            if (!wrapper->color_processor ||
                wrapper->cached_src_colorspace != QString::fromUtf8(src_cs_name)) {
                if (wrapper->color_processor) {
                    oak_color_processor_free(wrapper->color_processor);
                    wrapper->color_processor = nullptr;
                }
                wrapper->color_processor = oak_color_processor_create(
                    wrapper->color_config, src_cs_name, "ACES - ACEScg");
                wrapper->cached_src_colorspace = src_cs_name;
            }
            if (wrapper->color_processor &&
                (frame->format == AV_PIX_FMT_RGBAF32LE ||
                 frame->format == AV_PIX_FMT_RGBAF32BE)) {
                oak_color_processor_apply(
                    wrapper->color_processor,
                    frame->width, frame->height,
                    reinterpret_cast<const float*>(frame->data[0]),
                    reinterpret_cast<float*>(frame->data[0]),
                    0);
            }
        }
    }

    return 0;
}

int oak_decoder_thumbnail(OakDecoderHandle decoder, int stream_index,
                          int max_size,
                          OakFrame* out_frame)
{
    (void)stream_index;
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder || !out_frame) return -1;

    std::memset(out_frame, 0, sizeof(OakFrame));

    olive::Decoder::RetrieveVideoParams p;
    p.time = olive::core::rational(0, 1);
    p.maximum_format = olive::PixelFormat::U8;
    p.divider = 1;

    olive::AVFramePtr src = wrapper->decoder->RetrieveVideo(p);
    if (!src) return -1;

    int src_w = src->width;
    int src_h = src->height;
    int dst_w = src_w;
    int dst_h = src_h;

    if (max_size > 0) {
        if (src_w > src_h) {
            if (src_w > max_size) {
                dst_w = max_size;
                dst_h = static_cast<int>(static_cast<double>(src_h) * max_size / src_w);
            }
        } else {
            if (src_h > max_size) {
                dst_h = max_size;
                dst_w = static_cast<int>(static_cast<double>(src_w) * max_size / src_h);
            }
        }
    }

    // Always convert to RGBA8 for thumbnails
    AVPixelFormat dst_fmt = AV_PIX_FMT_RGBA;
    olive::AVFramePtr thumb = olive::CreateAVFramePtr();
    thumb->width = dst_w;
    thumb->height = dst_h;
    thumb->format = dst_fmt;
    if (av_frame_get_buffer(thumb.get(), 0) < 0) return -1;

    SwsContext *sws = sws_getContext(src_w, src_h, static_cast<AVPixelFormat>(src->format),
                                     dst_w, dst_h, dst_fmt,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws) return -1;

    sws_scale(sws, src->data, src->linesize, 0, src_h,
              thumb->data, thumb->linesize);
    sws_freeContext(sws);

    out_frame->width = dst_w;
    out_frame->height = dst_h;
    out_frame->pix_fmt = OAK_FRAME_PIX_RGBA8;
    out_frame->colorspace = "sRGB";
    out_frame->storage = OAK_FRAME_CPU;
    out_frame->planes = 1;
    out_frame->data[0] = thumb->data[0];
    out_frame->stride[0] = thumb->linesize[0];
    out_frame->pts_num = 0;
    out_frame->pts_den = 1;

    auto* holder = new olive::AVFramePtr(thumb);
    out_frame->internal = holder;
    return 0;
}

/* ================================================================ */
/*  Audio decode & conform                                            */
/* ================================================================ */

int oak_decoder_read_audio(OakDecoderHandle decoder, int stream_index,
                           int64_t start_sample, int64_t sample_count,
                           float **out_data, int64_t *out_actual_samples)
{
    (void)stream_index;
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder) return -1;

    if (wrapper->desc.GetAudioStreams().isEmpty()) return -1;
    const olive::AudioParams &ap = wrapper->desc.GetAudioStreams().first();

    olive::SampleBuffer buffer(ap, static_cast<size_t>(sample_count));
    buffer.allocate();

    olive::TimeRange range(
        olive::core::rational(start_sample, ap.sample_rate()),
        olive::core::rational(sample_count, ap.sample_rate()));

    auto status = wrapper->decoder->RetrieveAudio(
        buffer, range, ap, QString(),
        olive::LoopMode::kLoopModeOff, olive::RenderMode::kOffline);

    if (status != olive::Decoder::kOK) {
        if (out_data) *out_data = nullptr;
        if (out_actual_samples) *out_actual_samples = 0;
        return -1;
    }

    int channels = buffer.channel_count();
    size_t samples = buffer.sample_count();
    size_t total = channels * samples;
    auto* data = new float[total];

    for (size_t s = 0; s < samples; s++) {
        for (int c = 0; c < channels; c++) {
            data[s * channels + c] = buffer.data(c)[s];
        }
    }

    if (out_data) *out_data = data;
    if (out_actual_samples) *out_actual_samples = static_cast<int64_t>(samples);
    return 0;
}

void oak_audio_buffer_free(float *data)
{
    delete[] data;
}

/* ---- Conform ---- */

int oak_conform_get(const char *filename, const char *decoder_id,
                    const char *cache_path, int stream_index,
                    int target_sample_rate, int target_channels,
                    OakAudioFormat target_sample_fmt,
                    bool wait,
                    const char ***out_filenames, int *out_count)
{
    (void)decoder_id;
    if (!filename || !cache_path) return -1;

    olive::ConformManager::CreateInstance();
    EnsureConformCallbackConnected();
    olive::ConformManager *cm = olive::ConformManager::instance();

    uint64_t layout_mask = (target_channels == 1) ? AV_CH_LAYOUT_MONO :
                           (target_channels == 2) ? AV_CH_LAYOUT_STEREO :
                           (target_channels == 6) ? AV_CH_LAYOUT_5POINT1 :
                           (target_channels == 8) ? AV_CH_LAYOUT_7POINT1 :
                           AV_CH_LAYOUT_STEREO;
    olive::AudioParams ap(target_sample_rate, layout_mask,
                          OakAudioFmtToOlive(target_sample_fmt));

    olive::Decoder::CodecStream stream(QString::fromUtf8(filename), stream_index);
    olive::ConformManager::Conform conform =
        cm->GetConformState(QString::fromUtf8(filename),
                            QString::fromUtf8(cache_path),
                            stream, ap, wait);

    if (out_count) *out_count = conform.filenames.size();
    if (out_filenames) {
        if (conform.filenames.isEmpty()) {
            *out_filenames = nullptr;
        } else {
            auto arr = new const char*[conform.filenames.size()];
            for (int i = 0; i < conform.filenames.size(); i++) {
                QByteArray ba = conform.filenames.at(i).toUtf8();
                auto s = new char[ba.size() + 1];
                std::memcpy(s, ba.constData(), ba.size() + 1);
                arr[i] = s;
            }
            *out_filenames = arr;
        }
    }

    return (conform.state == olive::ConformManager::kConformExists) ? 0 : 1;
}

int oak_conform_poll(const char *filename, const char *cache_path,
                     int stream_index,
                     int target_sample_rate, int target_channels,
                     OakAudioFormat target_sample_fmt)
{
    if (!filename || !cache_path) return -1;

    olive::ConformManager::CreateInstance();
    EnsureConformCallbackConnected();
    olive::ConformManager *cm = olive::ConformManager::instance();

    uint64_t layout_mask = (target_channels == 1) ? AV_CH_LAYOUT_MONO :
                           (target_channels == 2) ? AV_CH_LAYOUT_STEREO :
                           (target_channels == 6) ? AV_CH_LAYOUT_5POINT1 :
                           (target_channels == 8) ? AV_CH_LAYOUT_7POINT1 :
                           AV_CH_LAYOUT_STEREO;
    olive::AudioParams ap(target_sample_rate, layout_mask,
                          OakAudioFmtToOlive(target_sample_fmt));

    olive::Decoder::CodecStream stream(QString::fromUtf8(filename), stream_index);
    return cm->Poll(QString::fromUtf8(cache_path), stream, ap);
}

void oak_conform_set_ready_callback(OakConformReadyCallback cb, void* userdata)
{
    g_conform_ready_cb = cb;
    g_conform_ready_userdata = userdata;
    EnsureConformCallbackConnected();
}

void oak_conform_free_filenames(const char **filenames, int count)
{
    if (!filenames) return;
    for (int i = 0; i < count; i++) {
        delete[] filenames[i];
    }
    delete[] filenames;
}

/* ================================================================ */
/*  Encoder (v2 — OakFrame)                                           */
/* ================================================================ */

OakEncoderHandle oak_encoder_create(const char *filepath,
                                    const char *container_format,
                                    const char *video_codec,
                                    const char *audio_codec)
{
    (void)container_format;
    (void)video_codec;
    (void)audio_codec;

    if (!filepath) return nullptr;

    olive::EncodingParams params;
    params.SetFilename(QString::fromUtf8(filepath));

    auto *wrapper = new oakcodec::OakEncoderWrapper();
    wrapper->params = params;

    return reinterpret_cast<OakEncoderHandle>(wrapper);
}

void oak_encoder_close(OakEncoderHandle encoder)
{
    if (!encoder) return;
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (wrapper->encoder) {
        wrapper->encoder->Close();
        delete wrapper->encoder;
    }
    if (wrapper->color_processor) {
        oak_color_processor_free(wrapper->color_processor);
    }
    if (wrapper->color_config) {
        oak_color_config_free(wrapper->color_config);
    }
    delete wrapper;
}

void oak_encoder_set_video_params(OakEncoderHandle encoder,
                                  int width, int height, OakFramePixelFormat pix_fmt,
                                  int64_t timebase_num, int64_t timebase_den,
                                  double frame_rate)
{
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper) return;

    olive::VideoParams vp;
    vp.set_width(width);
    vp.set_height(height);
    vp.set_format(OakFramePixFmtToOlive(pix_fmt));
    vp.set_channel_count(4);
    vp.set_time_base(olive::core::rational(timebase_num, timebase_den));
    vp.set_frame_rate(olive::core::rational::fromDouble(frame_rate));

    wrapper->params.EnableVideo(vp, olive::ExportCodec::kCodecH264);
}

void oak_encoder_set_video_output_format(OakEncoderHandle encoder,
                                         OakFramePixelFormat output_pix_fmt)
{
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper) return;
    wrapper->output_pix_fmt = output_pix_fmt;
}

void oak_encoder_set_video_output_colorspace(OakEncoderHandle encoder,
                                             const char *output_colorspace)
{
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper || !output_colorspace) return;
    wrapper->output_colorspace = QString::fromUtf8(output_colorspace);
}

void oak_encoder_set_audio_params(OakEncoderHandle encoder,
                                  int sample_rate, int channels,
                                  OakAudioFormat sample_fmt,
                                  int64_t timebase_num, int64_t timebase_den)
{
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper) return;

    uint64_t layout_mask = (channels == 1) ? AV_CH_LAYOUT_MONO :
                           (channels == 2) ? AV_CH_LAYOUT_STEREO :
                           (channels == 6) ? AV_CH_LAYOUT_5POINT1 :
                           (channels == 8) ? AV_CH_LAYOUT_7POINT1 :
                           AV_CH_LAYOUT_STEREO;
    olive::AudioParams ap(sample_rate, layout_mask, OakAudioFmtToOlive(sample_fmt));
    ap.set_time_base(olive::core::rational(timebase_num, timebase_den));

    wrapper->params.EnableAudio(ap, olive::ExportCodec::kCodecAAC);
}

int oak_encoder_write_video(OakEncoderHandle encoder, const OakFrame *frame)
{
    auto *wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper *>(encoder);
    if (!wrapper || !frame) return -1;

    if (!wrapper->encoder) {
        wrapper->encoder = olive::Encoder::CreateFromParams(wrapper->params);
        if (!wrapper->encoder) return -1;
        if (!wrapper->encoder->Open()) return -1;
    }

    const OakFrame *src = frame;
    OakFrame cpu_frame = {0};

    if (frame->storage != OAK_FRAME_CPU) {
        OakGLFunctions* gl = GetOakGL();
        if (gl && gl->readback_frame) {
            int src_type = (frame->storage == OAK_FRAME_GPU) ? 0 : 2;
            if (gl->readback_frame(nullptr, frame->data[0], src_type,
                                   OAK_FRAME_PIX_RGBA32F, &cpu_frame) != 0) {
                return -1;
            }
            src = &cpu_frame;
        } else {
            return -1; /* no readback available */
        }
    }

    // CPU path: wrap into olive::Frame
    olive::VideoParams vp = wrapper->params.video_params();
    olive::FramePtr oframe = olive::Frame::Create();
    oframe->set_video_params(vp);
    if (!oframe->allocate()) {
        if (src == &cpu_frame) oak_frame_release(&cpu_frame);
        return -1;
    }

    int src_stride = src->stride[0];
    int dst_stride = oframe->linesize_bytes();
    int row_bytes = std::min(src_stride, dst_stride);
    const char *src_data = static_cast<const char *>(src->data[0]);
    char *dst_data = oframe->data();

    for (int y = 0; y < src->height; y++) {
        std::memcpy(dst_data + y * dst_stride,
                    src_data + y * src_stride,
                    row_bytes);
    }

    // ODT: apply colorspace conversion if output_colorspace differs from frame's colorspace
    if (!wrapper->output_colorspace.isEmpty()) {
        const char* src_cs = src->colorspace;
        if (!src_cs || !src_cs[0]) src_cs = "ACES - ACEScg";

        if (wrapper->output_colorspace != QString::fromUtf8(src_cs)) {
            if (!wrapper->color_config) {
                wrapper->color_config = oak_color_config_load(nullptr);
            }
            if (wrapper->color_config) {
                if (!wrapper->color_processor ||
                    wrapper->cached_src_colorspace != QString::fromUtf8(src_cs)) {
                    if (wrapper->color_processor) {
                        oak_color_processor_free(wrapper->color_processor);
                        wrapper->color_processor = nullptr;
                    }
                    QByteArray dst_cs = wrapper->output_colorspace.toUtf8();
                    wrapper->color_processor = oak_color_processor_create(
                        wrapper->color_config, src_cs, dst_cs.constData());
                    wrapper->cached_src_colorspace = src_cs;
                }
                if (wrapper->color_processor) {
                    oak_color_processor_apply(
                        wrapper->color_processor,
                        src->width, src->height,
                        reinterpret_cast<const float*>(oframe->data()),
                        reinterpret_cast<float*>(oframe->data()),
                        0);
                }
            }
        }
    }

    olive::core::rational ts(src->pts_num, src->pts_den);
    bool ok = wrapper->encoder->WriteFrame(oframe, ts);

    if (src == &cpu_frame) oak_frame_release(&cpu_frame);
    return ok ? 0 : -1;
}

int oak_encoder_write_audio(OakEncoderHandle encoder,
                            const float *data, int64_t samples,
                            int64_t pts_num, int64_t pts_den)
{
    (void)pts_num;
    (void)pts_den;
    auto* wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper*>(encoder);
    if (!wrapper || !data || samples <= 0) return -1;

    if (!wrapper->encoder) {
        wrapper->encoder = olive::Encoder::CreateFromParams(wrapper->params);
        if (!wrapper->encoder) return -1;
        if (!wrapper->encoder->Open()) return -1;
    }

    const olive::AudioParams &ap = wrapper->params.audio_params();
    olive::core::SampleBuffer buffer(ap, static_cast<size_t>(samples));
    buffer.allocate();

    int channels = ap.channel_count();
    for (size_t s = 0; s < static_cast<size_t>(samples); s++) {
        for (int c = 0; c < channels; c++) {
            buffer.data(c)[s] = data[s * channels + c];
        }
    }

    bool ok = wrapper->encoder->WriteAudio(buffer);
    return ok ? 0 : -1;
}

int oak_encoder_finalize(OakEncoderHandle encoder)
{
    auto* wrapper = reinterpret_cast<oakcodec::OakEncoderWrapper*>(encoder);
    if (!wrapper || !wrapper->encoder) return -1;

    wrapper->encoder->Close();
    return 0;
}


/* ================================================================ */
/*  Decoder creation & capability queries                               */
/* ================================================================ */

OakDecoderHandle oak_decoder_create_from_id(const char* id)
{
    if (!id || !id[0]) return nullptr;
    olive::DecoderPtr decoder = olive::Decoder::CreateFromID(QString::fromUtf8(id));
    if (!decoder) return nullptr;

    auto* wrapper = new oakcodec::OakDecoderWrapper();
    wrapper->decoder = decoder;
    return reinterpret_cast<OakDecoderHandle>(wrapper);
}

const char* oak_decoder_id(OakDecoderHandle decoder)
{
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder) return nullptr;
    wrapper->cached_id = wrapper->decoder->id().toUtf8();
    return wrapper->cached_id.constData();
}

void oak_decoder_set_progress_callback(OakDecoderHandle decoder,
                                       OakDecoderProgressCallback cb,
                                       void* userdata)
{
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper) return;
    wrapper->progress_cb = cb;
    wrapper->progress_userdata = userdata;
    if (wrapper->decoder) {
        if (cb) {
            QObject::connect(wrapper->decoder.get(), &olive::Decoder::IndexProgress,
                             [wrapper](double p) {
                                 if (wrapper->progress_cb) {
                                     wrapper->progress_cb(p, wrapper->progress_userdata);
                                 }
                             });
        } else {
            QObject::disconnect(wrapper->decoder.get(), &olive::Decoder::IndexProgress,
                                nullptr, nullptr);
        }
    }
}

int oak_decoder_supports_video(OakDecoderHandle decoder)
{
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder) return 0;
    return wrapper->decoder->SupportsVideo() ? 1 : 0;
}

int oak_decoder_supports_audio(OakDecoderHandle decoder)
{
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder) return 0;
    return wrapper->decoder->SupportsAudio() ? 1 : 0;
}

int oak_decoder_is_open(OakDecoderHandle decoder)
{
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder) return 0;
    return wrapper->opened ? 1 : 0;
}

/* ================================================================ */
/*  File probe                                                          */
/* ================================================================ */

OakMediaInfo* oak_decoder_probe_file(OakDecoderHandle decoder, const char* filepath)
{
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder || !filepath) return nullptr;

    olive::CancelAtom cancel;
    olive::FootageDescription desc = wrapper->decoder->Probe(QString::fromUtf8(filepath), &cancel);
    if (!desc.IsValid()) return nullptr;

    wrapper->desc = desc;

    auto* out_info = new OakMediaInfo();
    out_info->video_stream_count = desc.GetVideoStreams().size();
    out_info->audio_stream_count = desc.GetAudioStreams().size();
    out_info->subtitle_stream_count = desc.GetSubtitleStreams().size();

    if (out_info->video_stream_count > 0) {
        out_info->video_streams = new OakVideoStreamInfo[out_info->video_stream_count];
        for (int i = 0; i < out_info->video_stream_count; i++) {
            const olive::VideoParams& vp = desc.GetVideoStreams().at(i);
            out_info->video_streams[i] = {
                vp.width(), vp.height(),
                OlivePixFmtToOakFrame(vp.format(), vp.channel_count()),
                vp.time_base().numerator(), vp.time_base().denominator(),
                vp.frame_rate().toDouble(), vp.duration()
            };
        }
    } else {
        out_info->video_streams = nullptr;
    }

    if (out_info->audio_stream_count > 0) {
        out_info->audio_streams = new OakAudioStreamInfo[out_info->audio_stream_count];
        for (int i = 0; i < out_info->audio_stream_count; i++) {
            const olive::AudioParams& ap = desc.GetAudioStreams().at(i);
            out_info->audio_streams[i] = {
                ap.sample_rate(), ap.channel_count(),
                OliveAudioFmtToOak(ap.format()),
                ap.time_base().numerator(), ap.time_base().denominator(),
                ap.duration()
            };
        }
    } else {
        out_info->audio_streams = nullptr;
    }

    return out_info;
}

/* ================================================================ */
/*  Stream open                                                         */
/* ================================================================ */

int oak_decoder_open_stream(OakDecoderHandle decoder, const char* filepath, int stream_index)
{
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder || !filepath) return -1;

    olive::Decoder::CodecStream stream(QString::fromUtf8(filepath), stream_index);
    wrapper->opened = wrapper->decoder->Open(stream);
    return wrapper->opened ? 0 : -1;
}

/* ================================================================ */
/*  Extended video decode                                               */
/* ================================================================ */

int oak_decoder_read_video_ex(OakDecoderHandle decoder, int stream_index,
                              const OakDecoderVideoParams* params,
                              OakFrame* out_frame)
{
    (void)stream_index;
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder || !params || !out_frame) return -1;

    olive::Decoder::RetrieveVideoParams p;
    p.time = olive::core::rational(params->time_num, params->time_den);
    p.divider = params->divider;
    p.maximum_format = OakFramePixFmtToOlive(static_cast<OakFramePixelFormat>(params->maximum_format));
    p.renderer = params->renderer_hint;
    p.cancelled = static_cast<olive::CancelAtom*>(params->cancelled);
    if (params->force_range == 1) {
        p.force_range = olive::VideoParams::kColorRangeFull;
    } else if (params->force_range == 2) {
        p.force_range = olive::VideoParams::kColorRangeLimited;
    }

    olive::AVFramePtr frame = wrapper->decoder->RetrieveVideo(p);
    if (!frame) return -1;

    std::memset(out_frame, 0, sizeof(OakFrame));
    out_frame->width = frame->width;
    out_frame->height = frame->height;
    out_frame->colorspace = "ACES - ACEScg";
    out_frame->pts_num = params->time_num;
    out_frame->pts_den = params->time_den;

    auto* frame_holder = new olive::AVFramePtr(frame);
    out_frame->internal = frame_holder;

    if (frame->format == AV_PIX_FMT_RGBAF32LE ||
        frame->format == AV_PIX_FMT_RGBAF32BE) {
        out_frame->pix_fmt = OAK_FRAME_PIX_RGBA32F;
    } else if (frame->format == AV_PIX_FMT_RGBA) {
        out_frame->pix_fmt = OAK_FRAME_PIX_RGBA8;
    } else {
        out_frame->pix_fmt = OAK_FRAME_PIX_INVALID;
    }

    if (params->renderer_hint) {
        OakGLFunctions* gl = GetOakGL();
        if (gl && gl->tex_upload && gl->blit_yuv) {
            AVPixelFormat avfmt = static_cast<AVPixelFormat>(frame->format);
            if (IsPlanarYUVFormat(avfmt)) {
                int uv_w = frame->width;
                int uv_h = frame->height;
                if (avfmt == AV_PIX_FMT_YUV420P || avfmt == AV_PIX_FMT_YUVJ420P ||
                    avfmt == AV_PIX_FMT_YUVA420P || avfmt == AV_PIX_FMT_YUV420P10LE ||
                    avfmt == AV_PIX_FMT_YUV420P12LE || avfmt == AV_PIX_FMT_YUV420P16LE) {
                    uv_w = (frame->width + 1) / 2;
                    uv_h = (frame->height + 1) / 2;
                } else if (avfmt == AV_PIX_FMT_YUV422P || avfmt == AV_PIX_FMT_YUVJ422P ||
                           avfmt == AV_PIX_FMT_YUVA422P || avfmt == AV_PIX_FMT_YUV422P10LE ||
                           avfmt == AV_PIX_FMT_YUV422P12LE || avfmt == AV_PIX_FMT_YUV422P16LE) {
                    uv_w = (frame->width + 1) / 2;
                }

                void* y_tex = gl->tex_upload(params->renderer_hint, frame->width, frame->height,
                                             OAK_RENDER_PIX_FMT_R8,
                                             frame->data[0], frame->linesize[0]);
                void* u_tex = gl->tex_upload(params->renderer_hint, uv_w, uv_h,
                                             OAK_RENDER_PIX_FMT_R8,
                                             frame->data[1], frame->linesize[1]);
                void* v_tex = gl->tex_upload(params->renderer_hint, uv_w, uv_h,
                                             OAK_RENDER_PIX_FMT_R8,
                                             frame->data[2], frame->linesize[2]);

                if (y_tex && u_tex && v_tex) {
                    OakTargetHandle target = oak_target_create(
                        reinterpret_cast<OakRendererHandle>(params->renderer_hint),
                        frame->width, frame->height,
                        OAK_RENDER_PIX_FMT_RGBA32F, false);

                    if (target) {
                        float mat3x3[9];
                        bool full_range;
                        GetYUVColorMatrix(frame->colorspace, frame->color_range, mat3x3, &full_range);

                        gl->blit_yuv(params->renderer_hint, y_tex, u_tex, v_tex, target,
                                     frame->width, frame->height,
                                     mat3x3, full_range, OAK_FRAME_PIX_INVALID);

                        OakTextureHandle color_tex = oak_target_detach_color_texture(
                            reinterpret_cast<OakRendererHandle>(params->renderer_hint), target);

                        oak_target_destroy(reinterpret_cast<OakRendererHandle>(params->renderer_hint), target);

                        gl->tex_destroy(params->renderer_hint, y_tex);
                        gl->tex_destroy(params->renderer_hint, u_tex);
                        gl->tex_destroy(params->renderer_hint, v_tex);

                        if (color_tex) {
                            out_frame->storage = OAK_FRAME_GPU;
                            out_frame->planes = 1;
                            out_frame->pix_fmt = OAK_FRAME_PIX_RGBA32F;
                            out_frame->data[0] = color_tex;
                            out_frame->stride[0] = 0;
                            return 0;
                        }
                    }
                }

                if (y_tex) gl->tex_destroy(params->renderer_hint, y_tex);
                if (u_tex) gl->tex_destroy(params->renderer_hint, u_tex);
                if (v_tex) gl->tex_destroy(params->renderer_hint, v_tex);
            } else {
                OakRenderPixelFormat render_fmt = OAK_RENDER_PIX_FMT_RGBA32F;
                if (out_frame->pix_fmt == OAK_FRAME_PIX_RGBA8)  render_fmt = OAK_RENDER_PIX_FMT_RGBA8;
                else if (out_frame->pix_fmt == OAK_FRAME_PIX_RGBA16) render_fmt = OAK_RENDER_PIX_FMT_RGBA16;

                void* tex = gl->tex_upload(params->renderer_hint, frame->width, frame->height,
                                           render_fmt,
                                           frame->data[0], frame->linesize[0]);
                if (tex) {
                    out_frame->storage = OAK_FRAME_GPU;
                    out_frame->planes = 1;
                    out_frame->data[0] = tex;
                    out_frame->stride[0] = 0;
                    return 0;
                }
            }
        }
    }

    // CPU path
    out_frame->storage = OAK_FRAME_CPU;
    out_frame->planes = 1;
    out_frame->data[0] = frame->data[0];
    out_frame->stride[0] = frame->linesize[0];

    const char* src_cs_name = AVColorSpaceToOCIOName(frame->colorspace);
    if (!src_cs_name) {
        if (frame->color_primaries == AVCOL_PRI_BT709) src_cs_name = "Rec.709";
        else if (frame->color_primaries == AVCOL_PRI_BT2020) src_cs_name = "Rec.2020";
        else if (frame->color_primaries == AVCOL_PRI_SMPTE170M) src_cs_name = "Rec.601 (NTSC)";
        else if (frame->color_primaries == AVCOL_PRI_SMPTE240M) src_cs_name = "SMPTE 240M";
        else src_cs_name = "Rec.709";
    }

    if (std::strcmp(src_cs_name, "ACES - ACEScg") != 0) {
        if (!wrapper->color_config) {
            wrapper->color_config = oak_color_config_load(nullptr);
        }
        if (wrapper->color_config) {
            if (!wrapper->color_processor ||
                wrapper->cached_src_colorspace != QString::fromUtf8(src_cs_name)) {
                if (wrapper->color_processor) {
                    oak_color_processor_free(wrapper->color_processor);
                    wrapper->color_processor = nullptr;
                }
                wrapper->color_processor = oak_color_processor_create(
                    wrapper->color_config, src_cs_name, "ACES - ACEScg");
                wrapper->cached_src_colorspace = src_cs_name;
            }
            if (wrapper->color_processor &&
                (frame->format == AV_PIX_FMT_RGBAF32LE ||
                 frame->format == AV_PIX_FMT_RGBAF32BE)) {
                oak_color_processor_apply(
                    wrapper->color_processor,
                    frame->width, frame->height,
                    reinterpret_cast<const float*>(frame->data[0]),
                    reinterpret_cast<float*>(frame->data[0]),
                    0);
            }
        }
    }

    return 0;
}

/* ================================================================ */
/*  Extended audio decode                                               */
/* ================================================================ */

int oak_decoder_read_audio_ex(OakDecoderHandle decoder, int stream_index,
                              const OakDecoderAudioParams* params,
                              float** out_data, int64_t* out_actual_samples)
{
    (void)stream_index;
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder || !params) return -1;

    if (wrapper->desc.GetAudioStreams().isEmpty()) return -1;
    const olive::AudioParams& ap = wrapper->desc.GetAudioStreams().first();

    olive::SampleBuffer buffer(ap, static_cast<size_t>(params->sample_count));
    buffer.allocate();

    olive::TimeRange range(
        olive::core::rational(params->start_sample, ap.sample_rate()),
        olive::core::rational(params->sample_count, ap.sample_rate()));

    olive::LoopMode loop_mode = (params->loop_mode == 1)
                                    ? olive::LoopMode::kLoopModeLoop
                                    : olive::LoopMode::kLoopModeOff;
    olive::RenderMode::Mode render_mode = (params->render_mode == 1)
                                              ? olive::RenderMode::kOnline
                                              : olive::RenderMode::kOffline;

    QString cache_path = params->cache_path ? QString::fromUtf8(params->cache_path) : QString();

    auto status = wrapper->decoder->RetrieveAudio(
        buffer, range, ap, cache_path, loop_mode, render_mode);

    if (status != olive::Decoder::kOK) {
        if (out_data) *out_data = nullptr;
        if (out_actual_samples) *out_actual_samples = 0;
        return -1;
    }

    int channels = buffer.channel_count();
    size_t samples = buffer.sample_count();
    size_t total = channels * samples;
    auto* data = new float[total];

    for (size_t s = 0; s < samples; s++) {
        for (int c = 0; c < channels; c++) {
            data[s * channels + c] = buffer.data(c)[s];
        }
    }

    if (out_data) *out_data = data;
    if (out_actual_samples) *out_actual_samples = static_cast<int64_t>(samples);
    return 0;
}

/* ================================================================ */
/*  Conform audio                                                       */
/* ================================================================ */

int oak_decoder_conform_audio(OakDecoderHandle decoder,
                              const char* cache_path,
                              int target_sample_rate, int target_channels,
                              OakAudioFormat target_sample_fmt)
{
    auto* wrapper = reinterpret_cast<oakcodec::OakDecoderWrapper*>(decoder);
    if (!wrapper || !wrapper->decoder || !cache_path) return -1;

    uint64_t layout_mask = (target_channels == 1) ? AV_CH_LAYOUT_MONO :
                           (target_channels == 2) ? AV_CH_LAYOUT_STEREO :
                           (target_channels == 6) ? AV_CH_LAYOUT_5POINT1 :
                           (target_channels == 8) ? AV_CH_LAYOUT_7POINT1 :
                           AV_CH_LAYOUT_STEREO;

    olive::AudioParams ap(target_sample_rate, layout_mask,
                          OakAudioFmtToOlive(target_sample_fmt));

    QVector<QString> output_filenames;
    QString base = QString::fromUtf8(cache_path);
    output_filenames.append(base + QStringLiteral(".conform.wav"));

    return wrapper->decoder->ConformAudio(output_filenames, ap, nullptr) ? 0 : -1;
}


/* ================================================================ */
/*  Frame utilities (for PluginRenderer)                                */
/* ================================================================ */

extern "C" {

void* oak_frame_alloc(int width, int height, int av_format)
{
    AVFrame* f = av_frame_alloc();
    if (!f) return nullptr;
    f->format = av_format;
    f->width = width;
    f->height = height;
    if (av_frame_get_buffer(f, 0) < 0) {
        av_frame_free(&f);
        return nullptr;
    }
    return f;
}

void oak_frame_free(void* frame)
{
    if (frame) {
        AVFrame* f = static_cast<AVFrame*>(frame);
        av_frame_free(&f);
    }
}

int oak_frame_get_plane(void* frame, int plane, void** out_data, int* out_linesize)
{
    if (!frame) return -1;
    AVFrame* f = static_cast<AVFrame*>(frame);
    if (plane < 0 || plane >= AV_NUM_DATA_POINTERS) return -1;
    if (out_data) *out_data = f->data[plane];
    if (out_linesize) *out_linesize = f->linesize[plane];
    return 0;
}

int oak_frame_get_params(void* frame, int* out_width, int* out_height, int* out_av_format)
{
    if (!frame) return -1;
    AVFrame* f = static_cast<AVFrame*>(frame);
    if (out_width) *out_width = f->width;
    if (out_height) *out_height = f->height;
    if (out_av_format) *out_av_format = f->format;
    return 0;
}

int oak_frame_convert(void* src_frame, void* dst_frame)
{
    if (!src_frame || !dst_frame) return -1;
    AVFrame* src = static_cast<AVFrame*>(src_frame);
    AVFrame* dst = static_cast<AVFrame*>(dst_frame);

    SwsContext* sws_ctx = sws_getContext(
        src->width, src->height, static_cast<AVPixelFormat>(src->format),
        dst->width, dst->height, static_cast<AVPixelFormat>(dst->format),
        SWS_POINT, nullptr, nullptr, nullptr);
    if (!sws_ctx) return -1;

    int ret = sws_scale(sws_ctx, src->data, src->linesize, 0, src->height,
                        dst->data, dst->linesize);
    sws_freeContext(sws_ctx);
    return (ret > 0) ? 0 : -1;
}

int oak_video_format_to_av(int pixel_format, int channel_count)
{
    return static_cast<int>(olive::FFmpegUtils::GetFFmpegPixelFormat(
        static_cast<olive::core::PixelFormat::Format>(pixel_format), channel_count));
}

int oak_av_to_video_format(int av_format, int* out_pixel_format, int* out_channel_count)
{
    switch (av_format) {
    case AV_PIX_FMT_GRAY8:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::U8);
        if (out_channel_count) *out_channel_count = 1;
        return 0;
    case AV_PIX_FMT_RGB24:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::U8);
        if (out_channel_count) *out_channel_count = 3;
        return 0;
    case AV_PIX_FMT_RGBA:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::U8);
        if (out_channel_count) *out_channel_count = 4;
        return 0;
    case AV_PIX_FMT_RGB48:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::U16);
        if (out_channel_count) *out_channel_count = 3;
        return 0;
    case AV_PIX_FMT_RGBA64:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::U16);
        if (out_channel_count) *out_channel_count = 4;
        return 0;
    case AV_PIX_FMT_RGBF16:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::F16);
        if (out_channel_count) *out_channel_count = 3;
        return 0;
    case AV_PIX_FMT_RGBAF16:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::F16);
        if (out_channel_count) *out_channel_count = 4;
        return 0;
    case AV_PIX_FMT_RGBF32:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::F32);
        if (out_channel_count) *out_channel_count = 3;
        return 0;
    case AV_PIX_FMT_RGBAF32:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::F32);
        if (out_channel_count) *out_channel_count = 4;
        return 0;
    case AV_PIX_FMT_GRAY16LE:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::U16);
        if (out_channel_count) *out_channel_count = 1;
        return 0;
    case AV_PIX_FMT_GRAYF16:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::F16);
        if (out_channel_count) *out_channel_count = 1;
        return 0;
    case AV_PIX_FMT_GRAYF32:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::F32);
        if (out_channel_count) *out_channel_count = 1;
        return 0;
    default:
        if (out_pixel_format) *out_pixel_format = static_cast<int>(olive::core::PixelFormat::INVALID);
        if (out_channel_count) *out_channel_count = 0;
        return -1;
    }
}

int oak_video_format_is_planar(int av_format)
{
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(av_format));
    if (!desc) return -1;
    return (desc->flags & AV_PIX_FMT_FLAG_PLANAR) ? 1 : 0;
}

int oak_video_format_compatible(int pixel_format)
{
    olive::core::PixelFormat fmt(static_cast<olive::core::PixelFormat::Format>(pixel_format));
    olive::core::PixelFormat result = olive::FFmpegUtils::GetCompatiblePixelFormat(fmt);
    return static_cast<int>(static_cast<olive::core::PixelFormat::Format>(result));
}

} // extern "C"
