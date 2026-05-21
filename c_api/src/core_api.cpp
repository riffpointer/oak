/*
 * Olive - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * C API implementation for libolivecore.
 */

#include "olive/core_api.h"

#include "core.h"
#include "util/rational.h"
#include "util/color.h"
#include "util/timerange.h"
#include "render/samplebuffer.h"
#include "render/audioparams.h"
#include "render/pixelformat.h"

#include <cstring>
#include <cstdlib>
#include <new>

using olive::core::rational;
using olive::core::Color;
using olive::core::TimeRange;
using olive::core::SampleBuffer;
using olive::core::AudioParams;
using olive::core::SampleFormat;
using olive::core::PixelFormat;

/* ========== Thread-local error state ========== */
thread_local int g_last_error_code = OLIVE_OK;
thread_local char g_last_error_string[1024];

static void SetError(int code, const char *msg)
{
    g_last_error_code = code;
    std::strncpy(g_last_error_string, msg, sizeof(g_last_error_string) - 1);
    g_last_error_string[sizeof(g_last_error_string) - 1] = '\0';
}

static void ClearError()
{
    g_last_error_code = OLIVE_OK;
    g_last_error_string[0] = '\0';
}

/* ========== Helpers: C <-> C++ conversions ========== */

static rational ToCppRational(OliveRational r)
{
    return rational(static_cast<int>(r.num), static_cast<int>(r.den));
}

static OliveRational ToCRational(const rational &r)
{
    return {static_cast<int64_t>(r.numerator()), static_cast<int64_t>(r.denominator())};
}

static Color ToCppColor(OliveColor c)
{
    return Color(static_cast<float>(c.r), static_cast<float>(c.g),
                 static_cast<float>(c.b), static_cast<float>(c.a));
}

static OliveColor ToCColor(const Color &c)
{
    return {static_cast<double>(c.red()), static_cast<double>(c.green()),
            static_cast<double>(c.blue()), static_cast<double>(c.alpha())};
}

static SampleFormat ToCppSampleFormat(OliveSampleFormat fmt)
{
    switch (fmt) {
    case OLIVE_SAMPLE_FMT_U8:   return SampleFormat::U8;
    case OLIVE_SAMPLE_FMT_S16:  return SampleFormat::S16;
    case OLIVE_SAMPLE_FMT_S32:  return SampleFormat::S32;
    case OLIVE_SAMPLE_FMT_FLT:  return SampleFormat::F32;
    case OLIVE_SAMPLE_FMT_DBL:  return SampleFormat::F64;
    case OLIVE_SAMPLE_FMT_U8P:  return SampleFormat::U8P;
    case OLIVE_SAMPLE_FMT_S16P: return SampleFormat::S16P;
    case OLIVE_SAMPLE_FMT_S32P: return SampleFormat::S32P;
    case OLIVE_SAMPLE_FMT_FLTP: return SampleFormat::F32P;
    case OLIVE_SAMPLE_FMT_DBLP: return SampleFormat::F64P;
    default:                    return SampleFormat::INVALID;
    }
}

static AudioParams ToCppAudioParams(const OliveAudioParams &p)
{
    return AudioParams(p.sample_rate, static_cast<uint64_t>(p.channel_layout),
                       ToCppSampleFormat(p.format));
}

/* ========== C API implementation ========== */

extern "C" {

/* ----- Version ----- */
int olive_core_api_version(void)
{
    return OLIVE_CORE_API_VERSION;
}

/* ----- Memory management ----- */
void olive_core_free(void *ptr)
{
    std::free(ptr);
}

void *olive_core_alloc(size_t size)
{
    return std::malloc(size);
}

void *olive_core_realloc(void *ptr, size_t size)
{
    return std::realloc(ptr, size);
}

/* ----- Error handling ----- */
int olive_core_last_error_code(void)
{
    return g_last_error_code;
}

const char *olive_core_last_error_string(void)
{
    return g_last_error_string;
}

/* ----- Rational ----- */
OliveRational olive_rational_make(int64_t num, int64_t den)
{
    ClearError();
    return {num, den};
}

OliveRational olive_rational_add(OliveRational a, OliveRational b)
{
    try {
        ClearError();
        rational ra = ToCppRational(a);
        rational rb = ToCppRational(b);
        return ToCRational(ra + rb);
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "rational_add failed");
        return {0, 0};
    }
}

OliveRational olive_rational_sub(OliveRational a, OliveRational b)
{
    try {
        ClearError();
        return ToCRational(ToCppRational(a) - ToCppRational(b));
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "rational_sub failed");
        return {0, 0};
    }
}

OliveRational olive_rational_mul(OliveRational a, OliveRational b)
{
    try {
        ClearError();
        return ToCRational(ToCppRational(a) * ToCppRational(b));
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "rational_mul failed");
        return {0, 0};
    }
}

OliveRational olive_rational_div(OliveRational a, OliveRational b)
{
    try {
        ClearError();
        return ToCRational(ToCppRational(a) / ToCppRational(b));
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "rational_div failed");
        return {0, 0};
    }
}

double olive_rational_to_double(OliveRational r)
{
    try {
        ClearError();
        return ToCppRational(r).toDouble();
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "rational_to_double failed");
        return 0.0;
    }
}

OliveRational olive_rational_from_double(double v, int64_t max_den)
{
    try {
        ClearError();
        rational r = rational::fromDouble(v);
        return ToCRational(r);
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "rational_from_double failed");
        return {0, 0};
    }
}

int olive_rational_cmp(OliveRational a, OliveRational b)
{
    try {
        ClearError();
        rational ra = ToCppRational(a);
        rational rb = ToCppRational(b);
        if (ra < rb) return -1;
        if (ra > rb) return 1;
        return 0;
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "rational_cmp failed");
        return 0;
    }
}

int olive_rational_is_valid(OliveRational r)
{
    try {
        ClearError();
        return ToCppRational(r).isNaN() ? 0 : 1;
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "rational_is_valid failed");
        return 0;
    }
}

void olive_rational_reduce(OliveRational *r)
{
    if (!r) {
        SetError(OLIVE_ERROR_INVALID, "rational_reduce: null pointer");
        return;
    }
    try {
        ClearError();
        // rational is immutable after construction (reduce happens in ctor),
        // so we reconstruct and assign back.
        rational cpp_r = ToCppRational(*r);
        *r = ToCRational(cpp_r);
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "rational_reduce failed");
    }
}

/* ----- Color ----- */
OliveColor olive_color_make(double r, double g, double b, double a)
{
    ClearError();
    return {r, g, b, a};
}

OliveColor olive_color_add(OliveColor a, OliveColor b)
{
    try {
        ClearError();
        return ToCColor(ToCppColor(a) + ToCppColor(b));
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "color_add failed");
        return {0, 0, 0, 0};
    }
}

OliveColor olive_color_mul_scalar(OliveColor c, double s)
{
    try {
        ClearError();
        return ToCColor(ToCppColor(c) * static_cast<float>(s));
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "color_mul_scalar failed");
        return {0, 0, 0, 0};
    }
}

/* ----- TimeRange ----- */
OliveTimeRange *olive_time_range_create(OliveRational in, OliveRational out)
{
    try {
        ClearError();
        return reinterpret_cast<OliveTimeRange *>(
            new TimeRange(ToCppRational(in), ToCppRational(out)));
    } catch (const std::bad_alloc &) {
        SetError(OLIVE_ERROR_NOMEM, "time_range_create: out of memory");
        return nullptr;
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "time_range_create failed");
        return nullptr;
    }
}

void olive_time_range_destroy(OliveTimeRange *tr)
{
    delete reinterpret_cast<TimeRange *>(tr);
}

OliveRational olive_time_range_in(OliveTimeRange *tr)
{
    if (!tr) {
        SetError(OLIVE_ERROR_INVALID, "time_range_in: null pointer");
        return {0, 0};
    }
    ClearError();
    return ToCRational(reinterpret_cast<TimeRange *>(tr)->in());
}

OliveRational olive_time_range_out(OliveTimeRange *tr)
{
    if (!tr) {
        SetError(OLIVE_ERROR_INVALID, "time_range_out: null pointer");
        return {0, 0};
    }
    ClearError();
    return ToCRational(reinterpret_cast<TimeRange *>(tr)->out());
}

OliveRational olive_time_range_length(OliveTimeRange *tr)
{
    if (!tr) {
        SetError(OLIVE_ERROR_INVALID, "time_range_length: null pointer");
        return {0, 0};
    }
    ClearError();
    return ToCRational(reinterpret_cast<TimeRange *>(tr)->length());
}

int olive_time_range_contains(OliveTimeRange *tr, OliveRational t)
{
    if (!tr) {
        SetError(OLIVE_ERROR_INVALID, "time_range_contains: null pointer");
        return 0;
    }
    ClearError();
    return reinterpret_cast<TimeRange *>(tr)->Contains(ToCppRational(t)) ? 1 : 0;
}

int olive_time_range_overlaps(OliveTimeRange *a, OliveTimeRange *b)
{
    if (!a || !b) {
        SetError(OLIVE_ERROR_INVALID, "time_range_overlaps: null pointer");
        return 0;
    }
    ClearError();
    return reinterpret_cast<TimeRange *>(a)->OverlapsWith(*reinterpret_cast<TimeRange *>(b)) ? 1 : 0;
}

/* ----- PixelFormat ----- */
int olive_pixel_format_bytes_per_channel(OlivePixelFormat fmt)
{
    ClearError();
    return PixelFormat::byte_count(static_cast<PixelFormat::Format>(fmt));
}

int olive_pixel_format_channel_count(OlivePixelFormat fmt)
{
    (void)fmt;
    ClearError();
    // olive::core::PixelFormat does not store channel count; it is format-agnostic.
    // Return 0 to indicate "unknown" at this level.
    return 0;
}

size_t olive_pixel_format_frame_size(OlivePixelFormat fmt, int width, int height)
{
    ClearError();
    int bpc = PixelFormat::byte_count(static_cast<PixelFormat::Format>(fmt));
    return static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(bpc);
}

const char *olive_pixel_format_name(OlivePixelFormat fmt)
{
    ClearError();
    static thread_local char buf[16];
    PixelFormat pf(static_cast<PixelFormat::Format>(fmt));
    std::string s = pf.to_string();
    std::strncpy(buf, s.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return buf;
}

/* ----- SampleBuffer ----- */
OliveSampleBuffer *olive_sample_buffer_create(OliveAudioParams params, int sample_count)
{
    try {
        ClearError();
        AudioParams cpp_params = ToCppAudioParams(params);
        return reinterpret_cast<OliveSampleBuffer *>(
            new SampleBuffer(cpp_params, static_cast<size_t>(sample_count)));
    } catch (const std::bad_alloc &) {
        SetError(OLIVE_ERROR_NOMEM, "sample_buffer_create: out of memory");
        return nullptr;
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "sample_buffer_create failed");
        return nullptr;
    }
}

void olive_sample_buffer_destroy(OliveSampleBuffer *buf)
{
    delete reinterpret_cast<SampleBuffer *>(buf);
}

int olive_sample_buffer_sample_count(OliveSampleBuffer *buf)
{
    if (!buf) {
        SetError(OLIVE_ERROR_INVALID, "sample_buffer_sample_count: null pointer");
        return 0;
    }
    ClearError();
    return static_cast<int>(reinterpret_cast<SampleBuffer *>(buf)->sample_count());
}

int olive_sample_buffer_channel_count(OliveSampleBuffer *buf)
{
    if (!buf) {
        SetError(OLIVE_ERROR_INVALID, "sample_buffer_channel_count: null pointer");
        return 0;
    }
    ClearError();
    return reinterpret_cast<SampleBuffer *>(buf)->channel_count();
}

void *olive_sample_buffer_channel_data(OliveSampleBuffer *buf, int channel)
{
    if (!buf) {
        SetError(OLIVE_ERROR_INVALID, "sample_buffer_channel_data: null pointer");
        return nullptr;
    }
    ClearError();
    return reinterpret_cast<SampleBuffer *>(buf)->data(channel);
}

size_t olive_sample_buffer_channel_data_size(OliveSampleBuffer *buf)
{
    if (!buf) {
        SetError(OLIVE_ERROR_INVALID, "sample_buffer_channel_data_size: null pointer");
        return 0;
    }
    ClearError();
    auto *sb = reinterpret_cast<SampleBuffer *>(buf);
    return sb->sample_count() * sizeof(float);
}

OliveAudioParams olive_sample_buffer_params(OliveSampleBuffer *buf)
{
    if (!buf) {
        SetError(OLIVE_ERROR_INVALID, "sample_buffer_params: null pointer");
        return {0, 0, OLIVE_SAMPLE_FMT_INVALID};
    }
    ClearError();
    const AudioParams &p = reinterpret_cast<SampleBuffer *>(buf)->audio_params();
    OliveAudioParams result;
    result.sample_rate = p.sample_rate();
    result.channel_layout = 0; // FIXME: extract from AVChannelLayout if needed
    result.format = OLIVE_SAMPLE_FMT_FLT; // FIXME: map back from SampleFormat
    return result;
}

} // extern "C"
