/*
 * Olive - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * Pure C API for libolivecore — base data types.
 */

#ifndef OLIVE_CORE_API_H
#define OLIVE_CORE_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "export.h"

#include <stdint.h>
#include <stddef.h>

#define OLIVE_CORE_API_VERSION 1

/* ========== Result codes ========== */
typedef enum {
    OLIVE_OK = 0,
    OLIVE_ERROR_GENERIC = -1,
    OLIVE_ERROR_INVALID = -2,
    OLIVE_ERROR_NOMEM = -3,
    OLIVE_ERROR_NOT_FOUND = -4,
    OLIVE_ERROR_IO = -5,
    OLIVE_ERROR_CANCELLED = -6,
    OLIVE_ERROR_UNSUPPORTED = -7,
} OliveResult;

/* ========== Pixel format ========== */
typedef enum {
    OLIVE_PIXEL_FMT_INVALID = -1,
    OLIVE_PIXEL_FMT_U8 = 0,
    OLIVE_PIXEL_FMT_U16,
    OLIVE_PIXEL_FMT_F16,
    OLIVE_PIXEL_FMT_F32,
    OLIVE_PIXEL_FMT_COUNT
} OlivePixelFormat;

/* ========== Sample format ========== */
typedef enum {
    OLIVE_SAMPLE_FMT_INVALID = 0,
    OLIVE_SAMPLE_FMT_U8,
    OLIVE_SAMPLE_FMT_S16,
    OLIVE_SAMPLE_FMT_S32,
    OLIVE_SAMPLE_FMT_FLT,
    OLIVE_SAMPLE_FMT_DBL,
    OLIVE_SAMPLE_FMT_U8P,
    OLIVE_SAMPLE_FMT_S16P,
    OLIVE_SAMPLE_FMT_S32P,
    OLIVE_SAMPLE_FMT_FLTP,
    OLIVE_SAMPLE_FMT_DBLP,
    OLIVE_SAMPLE_FMT_COUNT
} OliveSampleFormat;

/* ========== POD structs ========== */
typedef struct {
    int64_t num;
    int64_t den;
} OliveRational;

typedef struct {
    double r;
    double g;
    double b;
    double a;
} OliveColor;

typedef struct {
    int width;
    int height;
} OliveSize;

typedef struct {
    int width;
    int height;
    int depth;
    int channel_count;
    OlivePixelFormat format;
    double pixel_aspect_num;
    double pixel_aspect_den;
} OliveVideoParams;

typedef struct {
    int sample_rate;
    int64_t channel_layout;
    OliveSampleFormat format;
} OliveAudioParams;

/* ========== Opaque types ========== */
typedef struct OliveTimeRange   OliveTimeRange;
typedef struct OliveSampleBuffer OliveSampleBuffer;

/* ========== Version ========== */
OLIVE_CORE_API int olive_core_api_version(void);

/* ========== Memory management ========== */
OLIVE_CORE_API void  olive_core_free(void* ptr);
OLIVE_CORE_API void* olive_core_alloc(size_t size);
OLIVE_CORE_API void* olive_core_realloc(void* ptr, size_t size);

/* ========== Error handling (thread-local) ========== */
OLIVE_CORE_API int         olive_core_last_error_code(void);
OLIVE_CORE_API const char* olive_core_last_error_string(void);

/* ========== Rational ========== */
OLIVE_CORE_API OliveRational olive_rational_make(int64_t num, int64_t den);
OLIVE_CORE_API OliveRational olive_rational_add(OliveRational a, OliveRational b);
OLIVE_CORE_API OliveRational olive_rational_sub(OliveRational a, OliveRational b);
OLIVE_CORE_API OliveRational olive_rational_mul(OliveRational a, OliveRational b);
OLIVE_CORE_API OliveRational olive_rational_div(OliveRational a, OliveRational b);
OLIVE_CORE_API double        olive_rational_to_double(OliveRational r);
OLIVE_CORE_API OliveRational olive_rational_from_double(double v, int64_t max_den);
OLIVE_CORE_API int           olive_rational_cmp(OliveRational a, OliveRational b);
OLIVE_CORE_API int           olive_rational_is_valid(OliveRational r);
OLIVE_CORE_API void          olive_rational_reduce(OliveRational* r);

/* ========== Color ========== */
OLIVE_CORE_API OliveColor olive_color_make(double r, double g, double b, double a);
OLIVE_CORE_API OliveColor olive_color_add(OliveColor a, OliveColor b);
OLIVE_CORE_API OliveColor olive_color_mul_scalar(OliveColor c, double s);

/* ========== TimeRange ========== */
OLIVE_CORE_API OliveTimeRange* olive_time_range_create(OliveRational in, OliveRational out);
OLIVE_CORE_API void            olive_time_range_destroy(OliveTimeRange* tr);
OLIVE_CORE_API OliveRational   olive_time_range_in(OliveTimeRange* tr);
OLIVE_CORE_API OliveRational   olive_time_range_out(OliveTimeRange* tr);
OLIVE_CORE_API OliveRational   olive_time_range_length(OliveTimeRange* tr);
OLIVE_CORE_API int             olive_time_range_contains(OliveTimeRange* tr, OliveRational t);
OLIVE_CORE_API int             olive_time_range_overlaps(OliveTimeRange* a, OliveTimeRange* b);

/* ========== PixelFormat utilities ========== */
OLIVE_CORE_API int     olive_pixel_format_bytes_per_channel(OlivePixelFormat fmt);
OLIVE_CORE_API int     olive_pixel_format_channel_count(OlivePixelFormat fmt);
OLIVE_CORE_API size_t  olive_pixel_format_frame_size(OlivePixelFormat fmt, int width, int height);
OLIVE_CORE_API const char* olive_pixel_format_name(OlivePixelFormat fmt);

/* ========== SampleBuffer ========== */
OLIVE_CORE_API OliveSampleBuffer* olive_sample_buffer_create(OliveAudioParams params,
                                                               int sample_count);
OLIVE_CORE_API void               olive_sample_buffer_destroy(OliveSampleBuffer* buf);
OLIVE_CORE_API int                olive_sample_buffer_sample_count(OliveSampleBuffer* buf);
OLIVE_CORE_API int                olive_sample_buffer_channel_count(OliveSampleBuffer* buf);
OLIVE_CORE_API void*              olive_sample_buffer_channel_data(OliveSampleBuffer* buf,
                                                                     int channel);
OLIVE_CORE_API size_t             olive_sample_buffer_channel_data_size(OliveSampleBuffer* buf);
OLIVE_CORE_API OliveAudioParams   olive_sample_buffer_params(OliveSampleBuffer* buf);

#ifdef __cplusplus
}
#endif

#endif // OLIVE_CORE_API_H
