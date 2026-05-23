/*
 *  oak_frame_api.h
 *  跨模块零拷贝帧抽象。不依赖 FFmpeg 或 OpenGL。
 *  全链路默认格式：RGBA32F + ACEScg (AP1, scene-referred linear)。
 */

#ifndef OAK_FRAME_API_H
#define OAK_FRAME_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  帧存储位置                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_FRAME_CPU = 0,
    OAK_FRAME_GPU,
    OAK_FRAME_EXTERNAL,
} OakFrameStorage;

/* ------------------------------------------------------------------ */
/*  像素格式（同时覆盖 RGBA 与 YUV/压缩格式）                              */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_FRAME_PIX_INVALID = 0,

    /* RGBA packed */
    OAK_FRAME_PIX_RGBA8,
    OAK_FRAME_PIX_RGBA16,
    OAK_FRAME_PIX_RGBA32F,    /* 默认中间格式，全链路 F32 + ACEScg */

    /* YUV 4:2:0 planar */
    OAK_FRAME_PIX_YUV420P8,
    OAK_FRAME_PIX_YUV420P10,

    /* YUV 4:2:2 planar */
    OAK_FRAME_PIX_YUV422P8,
    OAK_FRAME_PIX_YUV422P10,

    /* YUV 4:4:4 planar */
    OAK_FRAME_PIX_YUV444P8,
    OAK_FRAME_PIX_YUV444P10,

    /* semi-planar */
    OAK_FRAME_PIX_NV12,
    OAK_FRAME_PIX_NV21,
    OAK_FRAME_PIX_P010,
    OAK_FRAME_PIX_P016,

    /* single/dual channel */
    OAK_FRAME_PIX_R8,
    OAK_FRAME_PIX_RG8,
    OAK_FRAME_PIX_R16,
    OAK_FRAME_PIX_R32F,

    /* hardware opaque */
    OAK_FRAME_PIX_HW_VIDEOTOOLBOX,
    OAK_FRAME_PIX_HW_D3D11,
    OAK_FRAME_PIX_HW_VAAPI,
    OAK_FRAME_PIX_HW_CUDA,
    OAK_FRAME_PIX_HW_MEDIACODEC,
} OakFramePixelFormat;

/* ------------------------------------------------------------------ */
/*  帧描述符                                                            */
/* ------------------------------------------------------------------ */

typedef struct OakFrame {
    int width;
    int height;
    OakFramePixelFormat pix_fmt;
    OakFrameStorage     storage;

    int64_t pts_num;
    int64_t pts_den;

    /* 色彩空间名称（OCIO colorspace）。空字符串表示未标记。 */
    const char* colorspace;

    /*
     * CPU:   data[i] = 第 i 平面首地址, stride[i] = pitch
     * GPU:   data[0] = OakTextureHandle (packed) 或 data[0..planes-1] = 各平面 texture
     * EXTERNAL: data[0] = 平台句柄
     */
    void* data[4];
    int   stride[4];
    int   planes;

    /* codec 内部 opaque（AVFrame*），外部禁止读取 */
    void* internal;
} OakFrame;

/* ------------------------------------------------------------------ */
/*  生命周期                                                            */
/* ------------------------------------------------------------------ */

void oak_frame_release(OakFrame* frame);
void oak_frame_release_internal_only(OakFrame* frame);

#ifdef __cplusplus
}
#endif

#endif /* OAK_FRAME_API_H */
