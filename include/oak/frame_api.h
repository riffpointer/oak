/*
 *  oak_frame_api.h
 *  Cross-module zero-copy frame abstraction. No dependency on FFmpeg or OpenGL.
 *
 *  Default pipeline format: RGBA32F + ACEScg (AP1, scene-referred linear).
 *
 *  Design goals:
 *    - Zero-copy priority: decoded frames can stay on GPU until the end of the
 *      render chain or encoder needs them.
 *    - Format transparency: external modules see the pixel format but do not
 *      need to interpret planar layouts themselves.
 *    - Unified lifecycle: regardless of CPU/GPU/EXTERNAL storage, frames are
 *      released through oak_frame_release().
 *    - FFmpeg is invisible: the `internal` field is opaque to all modules
 *      except oakcodec.so.
 */

#ifndef OAK_FRAME_API_H
#define OAK_FRAME_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Frame storage location                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_FRAME_CPU = 0,      /**< Data resides in CPU memory; data[i] is readable/writable. */
    OAK_FRAME_GPU,          /**< Data resides in GPU texture; data[0] is OakTextureHandle. */
    OAK_FRAME_EXTERNAL,     /**< External platform surface (e.g. CVPixelBufferRef, D3D11Texture). */
} OakFrameStorage;

/* ------------------------------------------------------------------ */
/*  Pixel formats (covers RGBA, YUV, compressed, and hardware opaque)   */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_FRAME_PIX_INVALID = 0,

    /* RGBA packed */
    OAK_FRAME_PIX_RGBA8,     /**< 8-bit per channel RGBA (display / thumbnail only). */
    OAK_FRAME_PIX_RGBA16,    /**< 16-bit per channel RGBA. */
    OAK_FRAME_PIX_RGBA32F,   /**< Default intermediate format: 32-bit float RGBA + ACEScg. */

    /* YUV 4:2:0 planar */
    OAK_FRAME_PIX_YUV420P8,   /**< 8-bit Y/U/V three-plane. */
    OAK_FRAME_PIX_YUV420P10,  /**< 10-bit Y/U/V three-plane (stride in 16-bit). */

    /* YUV 4:2:2 planar */
    OAK_FRAME_PIX_YUV422P8,
    OAK_FRAME_PIX_YUV422P10,

    /* YUV 4:4:4 planar */
    OAK_FRAME_PIX_YUV444P8,
    OAK_FRAME_PIX_YUV444P10,

    /* semi-planar (NV series) */
    OAK_FRAME_PIX_NV12,       /**< 8-bit Y + UV interleaved. */
    OAK_FRAME_PIX_NV21,       /**< 8-bit Y + VU interleaved (Android common). */
    OAK_FRAME_PIX_P010,       /**< 10-bit Y + UV interleaved (HDR). */
    OAK_FRAME_PIX_P016,       /**< 16-bit Y + UV interleaved. */

    /* single / dual channel (mask, depth, normal) */
    OAK_FRAME_PIX_R8,
    OAK_FRAME_PIX_RG8,
    OAK_FRAME_PIX_R16,
    OAK_FRAME_PIX_R32F,

    /* hardware opaque formats (EXTERNAL only) */
    OAK_FRAME_PIX_HW_VIDEOTOOLBOX,   /**< macOS/iOS CVPixelBuffer. */
    OAK_FRAME_PIX_HW_D3D11,          /**< Windows D3D11Texture2D. */
    OAK_FRAME_PIX_HW_VAAPI,          /**< Linux VAAPI surface. */
    OAK_FRAME_PIX_HW_CUDA,           /**< NVIDIA CUDA array. */
    OAK_FRAME_PIX_HW_MEDIACODEC,     /**< Android MediaCodec surface. */
} OakFramePixelFormat;

/* ------------------------------------------------------------------ */
/*  Frame descriptor (POD + 4 pointers)                                 */
/* ------------------------------------------------------------------ */

typedef struct OakFrame {
    int width;                  /**< Frame width in pixels. */
    int height;                 /**< Frame height in pixels. */
    OakFramePixelFormat pix_fmt;/**< Pixel format of the frame data. */
    OakFrameStorage     storage;/**< Where the pixel data lives (CPU/GPU/EXTERNAL). */

    /* Timestamp in the stream's own timebase. */
    int64_t pts_num;
    int64_t pts_den;

    /**
     * OCIO colorspace name. Empty string means un-tagged.
     * Full-pipeline convention: decoder output -> node graph -> renderer -> encoder
     * input should all be tagged "ACES - ACEScg". If a node produces non-ACEScg
     * output (e.g. PluginNode outputting sRGB), colorspace must mark the real space
     * so downstream can convert back via oakcolor.so.
     */
    const char* colorspace;

    /**
     * Plane data / handles.
     *
     * CPU mode:
     *   data[i]   = start address of plane i.
     *   stride[i] = bytes per line (pitch) of plane i.
     *   planes    = number of planes (RGBA=1, YUV420P=3, NV12=2).
     *
     * GPU mode:
     *   Packed (RGBA8/16/32F): data[0] = OakTextureHandle, planes = 1.
     *   Planar (YUV420P, NV12): data[0..planes-1] = OakTextureHandle per plane.
     *   stride[i] is unused (set to 0).
     *
     * EXTERNAL mode:
     *   data[0] = platform-specific handle (e.g. CVPixelBufferRef).
     *   planes / stride determined by the platform; external code must not interpret.
     */
    void* data[4];
    int   stride[4];
    int   planes;

    /**
     * Internal opaque pointer. For codec internals this is usually AVFrame*.
     * External modules MUST NOT read, cast, or modify this field.
     */
    void* internal;
} OakFrame;

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief Release a frame and reclaim all associated resources.
 * @param frame Frame pointer (NULL is silently ignored).
 *
 * Depending on storage type, the internal release path differs:
 * - CPU: frees the internal AVFrame and its buffer.
 * - GPU: destroys the texture through the runtime-loaded oakgl.so interface.
 * - EXTERNAL: releases the reference to the platform surface (e.g. CVBufferRelease)
 *   but does NOT destroy the underlying surface (ownership remains external).
 *
 * @note For CPU readback frames obtained from oak_renderer_readback_frame,
 *       the buffer is freed via oak_renderer_free_readback internally.
 */
void oak_frame_release(OakFrame* frame);

/**
 * @brief Release only the internal reference, without freeing data[] resources.
 * @param frame Frame pointer.
 * @note Used in EXTERNAL mode: when the platform surface lifecycle is managed
 *       by the system (e.g. VideoToolbox), codec only needs to release its own
 *       AVFrame reference without destroying the surface.
 */
void oak_frame_release_internal_only(OakFrame* frame);

#ifdef __cplusplus
}
#endif

#endif /* OAK_FRAME_API_H */
