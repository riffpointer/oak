# Oak Video Editor Testing Strategy and Plan

This document describes the automated testing strategy for Oak Video Editor, including unit tests, integration tests, and CI execution.

## Goals

- Maximize automation and reduce manual testing.
- Cover all modules with at least one automated test.
- Keep integration tests headless (no GUI interaction).
- Make failures reproducible on Windows/macOS/Linux CI.

## Test Layers

### 1) Unit Tests (GoogleTest)
- Focus: small units, deterministic behavior, no GUI.
- Location: `tests/gtest/`.
- Execution: `ctest` target `olive-gtest`.

### 1.5) Module Smoke Tests (GoogleTest)
- Focus: compile-time and link-time coverage for GUI-heavy modules without instantiating widgets.
- Location: `tests/gtest/module_smoke_test.cpp`.
- Execution: `ctest` target `olive-gtest`.

### 2) Integration Tests (GoogleTest)
- Focus: cross-module flows without GUI (e.g., serialize → deserialize → resolve).
- Location: `tests/gtest/` (prefixed with `ProjectSerializer`, `TaskManager`, etc.).

### 3) Legacy Tests (Olive macro tests)
- Existing tests in `tests/general`, `tests/timeline`, `tests/compositing` remain.

## Module Coverage Map

Each top-level module has at least one test that exercises its core API or serialization path.

- `app/common`: `common_current_test.cpp`, `common_xmlutils_test.cpp`
- `app/config`: `config_test.cpp`
- `app/node`: `node_value_test.cpp`, `node_keyframe_test.cpp`, `node_serialization_test.cpp`
- `app/node/project/serializer`: `project_serializer_test.cpp`
- `app/render`: `render_videoparams_test.cpp`, `render_audioparams_test.cpp`
- `app/timeline`: `timeline_marker_test.cpp`
- `app/undo`: `undo_stack_test.cpp`
- `app/task`: `task_taskmanager_test.cpp`
- `app/codec`: `codec_frame_test.cpp`
- `app/pluginSupport`: `plugin_support_test.cpp`
- `app/audio`, `app/cli`, `app/dialog`, `app/panel`, `app/tool`, `app/ui`, `app/widget`, `app/window`: `module_smoke_test.cpp`

If a module has a GUI dependency (e.g., widgets), tests focus on non-visual data/model components.

## Integration Test Details

### Project Serializer Roundtrip
- Creates a minimal project with a built-in node.
- Saves to XML via `ProjectSerializer::Save`.
- Loads with `ProjectSerializer::Load`.
- Verifies that nodes are restored.

### Task Manager Execution
- Adds a dummy task to `TaskManager`.
- Waits for completion using an event loop.
- Verifies the task ran.

## Unit Coverage Highlights (Expanded)

- `app/undo`: `undo_stack_test.cpp` now covers empty stack state, model data, redo list coloring, jump behavior, and ignored empty multi-commands.
- `app/timeline`: `timeline_marker_test.cpp` now covers list ordering, closest-marker lookup, list save/load with unknown elements, and marker add/remove/change commands.
- `app/pluginSupport`: `plugin_support_image_test.cpp` now checks OFX property wiring (bounds/ROD, pixel depth, components, premult) and allocation clearing behavior.
- `app/render`: `render_videoparams_branch_test.cpp` now covers auto divider selection, pixel aspect validation, square-pixel width, and Save/Load roundtrip.

## Headless Execution

- Tests avoid QWidget usage.
- CI sets `QT_QPA_PLATFORM=offscreen` to prevent GUI initialization issues.

## Continuous Integration

CI runs on Windows, macOS, and Linux:

1. Install system dependencies (Qt, FFmpeg, OpenImageIO, OpenColorIO, OpenEXR, PortAudio, Expat).
2. Configure with `-DBUILD_TESTS=ON`.
3. Build with CMake + Ninja.
4. Run `ctest` with output on failure.

### Dependency Installation Notes
- Linux: use distro packages (`apt` on Ubuntu) for Qt6, FFmpeg, OpenImageIO, OpenColorIO, OpenEXR, PortAudio, Expat, OpenGL headers.
- macOS: use Homebrew for Qt6 and media/color/image libraries.
- Windows: use system installers where available (Qt via `install-qt-action`), and vcpkg for the remaining C/C++ libraries.

## Adding New Tests

- Place new unit tests in `tests/gtest`.
- Use GoogleTest conventions.
- Prefer deterministic fixtures and local-only resources.
- When adding a new module, add at least one unit test and one integration scenario if applicable.

---

## C API Full-Coverage Test Plan (multi-dylib)

### Scope

Cover **100 %** of the `extern "C"` surface exposed by the three shared libraries:

| Library | Header | Functions / Types | Test File |
|---|---|---|---|
| `liboakengine.so` | `include/oak/engine_api.h` | 7 functions + `OakFrame` | `tests/gtest/c_api_engine_test.cpp` |
| `liboakcodec.so` | `include/oak/codec_api.h` | ~30 functions + structs | `tests/gtest/c_api_codec_test.cpp` |
| `liboakcolor.so` | `include/oak/color_api.h` | ~30 functions + handles | `tests/gtest/c_api_color_test.cpp` |
| Cross-module | `include/oak/frame_api.h` | 2 functions + `OakFrame` | Covered in all three above + `c_api_integration_test.cpp` |

**Test framework:** GoogleTest (`gtest`/`gmock`).  
**Build target:** `olive-gtest` (add sources to `tests/gtest/CMakeLists.txt`).  
**Valgrind/ASan:** Run the `c_api_*` suite under `-fsanitize=address` on CI to catch leaks and use-after-free across the C boundary.

### 1. Engine API (`engine_api.h`)

```cpp
// Fixture: loads liboakengine.so via dlopen/dlsym (no compile-time link)
class CAPEngineTest : public ::testing::Test {
protected:
    void* engine_so_;
    decltype(&oak_engine_project_load_xml) fn_load_xml_;
    // ... resolve all symbols in SetUp
};
```

| Function | Test Cases |
|---|---|
| `oak_engine_project_load_xml` | 1. Minimal valid Olive XML → non-null handle, `node_count >= 0`  <br>2. Empty string → graceful failure (null handle, not crash) <br>3. Malformed XML → graceful failure <br>4. Large real-world project XML → load without crash, `node_count` matches expected |
| `oak_engine_project_destroy` | 1. Destroy valid handle → subsequent `node_count` call is UB (do not test directly), but verify no ASan error via fixture teardown <br>2. Destroy null → no crash <br>3. Double-destroy → no crash (defensive null-check inside impl) |
| `oak_engine_project_node_count` | 1. Freshly loaded minimal project → `0` or expected count <br>2. Null handle → return `-1` (error code) |
| `oak_engine_session_create` | 1. Valid project + 1920×1080 + `OAK_FRAME_PIX_RGBA32F` + `timebase={1,24}` → non-null session <br>2. Null project → null session <br>3. Zero width/height → null session <br>4. Invalid `pixel_format` enum → null session <br>5. Zero `timebase_den` → null session |
| `oak_engine_session_destroy` | 1. Destroy valid session → no ASan error <br>2. Destroy null → no crash <br>3. Double-destroy → no crash |
| `oak_engine_session_render_frame` | 1. Render frame at `time=0` → returns `0`, `out_frame` populated (`width/height/pix_fmt` match session params), `data[0]` non-null <br>2. Render frame at `time=mid` → same, pixel values plausible (not all zero if project has content) <br>3. Render frame at `time=duration` → same <br>4. Null session → returns non-zero <br>5. Null `out_frame` → returns non-zero <br>6. Render twice without destroy → first frame invalidated, second frame valid (verify no use-after-free) |

### 2. Codec API (`codec_api.h`)

```cpp
// Fixture: loads liboakcodec.so via dlopen/dlsym
class CAPCodecTest : public ::testing::Test {
protected:
    void* codec_so_;
    // helper: create a 1-second 48kHz stereo float WAV in /tmp for audio tests
    std::string temp_wav_path_;
    // helper: path to a 10-frame 1920x1080 H.264/MP4 (checked-in test asset)
    std::string test_mp4_path_;
};
```

**Decoder Lifecycle**

| Function | Test Cases |
|---|---|
| `oak_decoder_open` | 1. Valid MP4 + hint="" → non-null decoder, `out_info` populated (`video_stream_count >= 1`) <br>2. Valid MP4 + hint="ffmpeg" → same <br>3. Non-existent path → null decoder <br>4. Corrupted file → null decoder or `out_info` with zero streams <br>5. `out_info = NULL` → still opens decoder (no crash) |
| `oak_decoder_close` | 1. Close valid decoder → no ASan error <br>2. Close null → no crash <br>3. Double-close → no crash |
| `oak_media_info_free` | 1. Free info from `oak_decoder_open` → no leak <br>2. Free null → no crash <br>3. Double-free → no crash |
| `oak_decoder_create_from_id` | 1. id="ffmpeg" → non-null <br>2. id="oiio" → non-null (if available) <br>3. id="unknown" → null |
| `oak_decoder_id` | 1. Verify returned string matches creation id |
| `oak_decoder_supports_video` / `audio` | 1. FFmpeg decoder → both return `1` <br>2. Audio-only decoder → video=`0`, audio=`1` |
| `oak_decoder_is_open` | 1. Before `open_stream` → `0` <br>2. After `open_stream` → `1` |
| `oak_decoder_set_progress_callback` | 1. Set callback + open long file → callback invoked with `0.0 .. 1.0` <br>2. Set null callback → no crash |

**Probe & Stream Open**

| Function | Test Cases |
|---|---|
| `oak_decoder_probe_file` | 1. Valid MP4 → `video_streams[0].width/height/frame_rate` correct <br>2. Invalid path → null <br>3. Probe twice on same decoder → no leak (old info freed or overwritten safely) |
| `oak_decoder_open_stream` | 1. Open stream 0 of valid MP4 → returns `0` <br>2. Open invalid stream index → returns non-zero |

**Video Decode**

| Function | Test Cases |
|---|---|
| `oak_decoder_read_video` | 1. Read frame at t=0 → `out_frame.pix_fmt == OAK_FRAME_PIX_RGBA32F`, dimensions match `OakMediaInfo` <br>2. Read frame at t=mid → same <br>3. Read frame at t=EOF → returns non-zero or last frame <br>4. `renderer_hint = NULL` → CPU frame (CPU path) <br>5. `renderer_hint` with mock GPU renderer handle → verify GPU texture path is attempted (if renderer available) <br>6. Null decoder → non-zero <br>7. Null `out_frame` → non-zero |
| `oak_decoder_thumbnail` | 1. `max_size=256` → returned frame `width <= 256` and `height <= 256` <br>2. `max_size=0` → graceful failure |
| `oak_decoder_read_video_ex` | 1. `divider=2` → frame dimensions halved <br>2. `maximum_format=U8` → returned `pix_fmt == OAK_FRAME_PIX_RGBA8` <br>3. `force_range=full` → verify range metadata (if exposed) |

**Audio Decode**

| Function | Test Cases |
|---|---|
| `oak_decoder_read_audio` | 1. Read 1024 samples at t=0 → `out_actual_samples >= 0`, `out_data` non-null, `*out_data` is planar float <br>2. Read across EOF → `out_actual_samples < requested` <br>3. `start_sample < 0` → graceful failure <br>4. Null decoder → non-zero |
| `oak_audio_buffer_free` | 1. Free buffer from `read_audio` → no leak <br>2. Free null → no crash <br>3. Double-free → no crash |
| `oak_decoder_read_audio_ex` | 1. `loop_mode=1` + read across EOF → samples loop (if supported) <br>2. `cache_path` set → conform generated, subsequent reads faster |

**Conform**

| Function | Test Cases |
|---|---|
| `oak_decoder_conform_audio` | 1. Conform to 48kHz/2ch/FLOAT → file appears in cache path <br>2. Conform to 44.1kHz/mono → correct resampling/downmix |
| `oak_conform_get` | 1. Poll after conform starts → eventually returns filenames <br>2. `wait=true` → blocks until ready <br>3. `wait=false` + immediate call → returns empty list, `out_count=0` |
| `oak_conform_poll` | 1. Returns progress percentage `0..100` |
| `oak_conform_free_filenames` | 1. Free filenames from `conform_get` → no leak <br>2. Free null → no crash |

**Encoder**

| Function | Test Cases |
|---|---|
| `oak_encoder_create` | 1. Create MP4/H.264/AAC → non-null <br>2. Null filepath → null <br>3. Unsupported codec → null or graceful error |
| `oak_encoder_close` | 1. Close without write → no crash <br>2. Double-close → no crash |
| `oak_encoder_set_video_params` | 1. 1920×1080/RGBA32F/24fps → no error <br>2. Zero width → error |
| `oak_encoder_set_video_output_format` | 1. Set to YUV420P8 before write → encoder converts internally |
| `oak_encoder_set_video_output_colorspace` | 1. Set "Rec.709" → metadata correct |
| `oak_encoder_set_audio_params` | 1. 48kHz/2ch/FLOAT → no error |
| `oak_encoder_write_video` | 1. Write 10 RGBA32F frames → file grows <br>2. Write null frame → error <br>3. Wrong dimensions → error |
| `oak_encoder_write_audio` | 1. Write 1 sec of float samples → no error <br>2. Null data → error |
| `oak_encoder_finalize` | 1. Finalize after write → valid MP4 (probe with decoder to verify) <br>2. Finalize without write → empty but valid container <br>3. Double-finalize → no crash |

**Frame Utilities**

| Function | Test Cases |
|---|---|
| `oak_frame_alloc` | 1. 1920×1080 + `AV_PIX_FMT_RGBA` → non-null, `get_params` returns correct values <br>2. 0×0 → null <br>3. Invalid `av_format` → null |
| `oak_frame_free` | 1. Free valid → no leak <br>2. Free null → no crash <br>3. Double-free → no crash |
| `oak_frame_get_plane` | 1. Plane 0 of RGBA frame → `out_data` non-null, `out_linesize >= width*4` <br>2. Invalid plane index → `-1` <br>3. Null frame → `-1` |
| `oak_frame_get_params` | 1. Valid frame → params match alloc args <br>2. Null frame → `-1` |
| `oak_frame_convert` | 1. U8→F16, F16→F32, RGBA→RGB, RGBA→GRAY → all succeed, output pixel values within tolerance <br>2. Same format → no-op success <br>3. Null src/dst → `-1` <br>4. Dimension mismatch → `-1` or scaled (document expected behavior) |
| `oak_video_format_to_av` | 1. `U8+4` → `AV_PIX_FMT_RGBA` <br>2. `F32+1` → `AV_PIX_FMT_GRAYF32LE` <br>3. Invalid combo → `-1` |
| `oak_av_to_video_format` | 1. `AV_PIX_FMT_RGBA` → `U8, 4` <br>2. Unknown format → `-1` |
| `oak_video_format_is_planar` | 1. `AV_PIX_FMT_YUV420P` → `1` <br>2. `AV_PIX_FMT_RGBA` → `0` <br>3. `-1` → `-1` |
| `oak_video_format_compatible` | 1. `F32` → `F32` (no downgrade needed) <br>2. `F16` → `U16` or `F32` (document expected policy) |

### 3. Color API (`color_api.h`)

```cpp
// Fixture: loads liboakcolor.so via dlopen/dlsym
class CAPColorTest : public ::testing::Test {
protected:
    void* color_so_;
    OakColorConfigHandle config_;
    // default config path (checked-in or system OCIO config)
    std::string ocio_config_path_;
};
```

**Config Management**

| Function | Test Cases |
|---|---|
| `oak_color_config_load` | 1. Valid OCIO config path → non-null <br>2. Null path (default config) → non-null <br>3. Invalid path → null <br>4. Corrupted config → null |
| `oak_color_config_free` | 1. Free valid → no leak <br>2. Free null → no crash <br>3. Double-free → no crash |
| `oak_color_config_space_count` | 1. Default config → `> 0` <br>2. Null config → `0` or `-1` |
| `oak_color_config_space_name` | 1. Index `0` → non-empty string <br>2. Out-of-range index → null |
| `oak_color_config_get_space` | 1. "ACEScg" → non-null <br>2. "NonExistent" → null |
| `oak_color_config_default_input_space` | 1. Returns non-empty string (e.g., "ACEScg") |
| `oak_color_config_default_display` | 1. Returns non-empty string |
| `oak_color_config_display_view_count` / `_name` | 1. Valid display → `> 0` views <br>2. Invalid display → `0` |
| `oak_color_config_reference_space_name` | 1. Returns "scene_linear" or equivalent |
| `oak_color_space_equal` | 1. Same space handle → `true` <br>2. Different spaces → `false` <br>3. Null handles → `false` |

**Processor**

| Function | Test Cases |
|---|---|
| `oak_color_processor_create` | 1. "ACEScg" → "sRGB" → non-null <br>2. Same src/dst → valid no-op processor <br>3. Invalid space name → null |
| `oak_color_processor_create_from_lut` | 1. Valid 3D LUT file → non-null <br>2. Invalid path → null |
| `oak_color_processor_free` | 1. Free valid → no leak <br>2. Double-free → no crash |
| `oak_color_processor_apply` | 1. 64×64 RGBA32F buffer filled with `{1.0,0.5,0.0,1.0}` → output pixels changed (tolerance 1e-3) <br>2. `in_data == out_data` (in-place) → works <br>3. Null processor → no crash (or early return) <br>4. Zero width/height → no crash |
| `oak_color_processor_apply_pixel` | 1. Single pixel `{1,0,0,1}` → output within tolerance of expected transform <br>2. Null processor → no crash |

**Display Transform**

| Function | Test Cases |
|---|---|
| `oak_display_transform_create` | 1. Valid config + display + view → non-null <br>2. Null look → non-null (look optional) <br>3. Invalid display → null |
| `oak_display_transform_free` | 1. Double-free → no crash |
| `oak_display_transform_apply` | 1. Apply to 64×64 buffer → output within tolerance <br>2. Exposure/gamma parameters modify brightness visibly (statistical test) |

**High-level Processor Creation**

| Function | Test Cases |
|---|---|
| `oak_color_processor_create_transform` | 1. direction=`0` → forward transform <br>2. direction=`1` → inverse transform (round-trip test: forward then inverse ≈ identity within tolerance) |
| `oak_color_processor_create_display` | 1. Valid params → non-null <br>2. Apply and verify output is display-referred (not scene-referred) |

**GPU Shader**

| Function | Test Cases |
|---|---|
| `oak_color_gpu_shader_create` | 1. Valid processor → non-null shader <br>2. Null processor → null |
| `oak_color_gpu_shader_free` | 1. Double-free → no crash |
| `oak_color_gpu_shader_get_text` | 1. Valid shader → non-empty GLSL string containing `function_name` <br>2. Null shader → null |
| `oak_color_gpu_shader_get_3d_lut_count` | 1. Display transform with LUT → `> 0` <br>2. Simple matrix transform → `0` |
| `oak_color_gpu_shader_get_3d_lut` | 1. Index `0` → valid `out_name`, `out_sampler`, `out_edge_len`, `out_values` non-null <br>2. Invalid index → `-1` |
| `oak_color_gpu_shader_get_texture_count` / `get_texture` | 1. Similar to 3D LUT tests, but for 1D/2D textures |

**GradingPrimary**

| Function | Test Cases |
|---|---|
| `oak_color_grading_primary_create` | 1. style=`0` (lin) → non-null <br>2. style=`1` (video) → non-null <br>3. Invalid style → null |
| `oak_color_grading_primary_free` | 1. Double-free → no crash |
| `oak_color_grading_primary_set_*` | 1. Set contrast `{2,1,1,1}` → processor darkens/midtones change <br>2. Set offset `{0.1,0,0,0}` → red shift <br>3. Set saturation `0.0` → grayscale output <br>4. Set clamp_black/white → verify clamping |
| `oak_color_grading_primary_no_clamp_black` / `_white` | 1. Returns sentinel values (e.g., `-FLT_MAX`, `FLT_MAX`) |
| `oak_color_processor_create_from_grading` | 1. Valid gp + "ACEScg" → "sRGB" → non-null <br>2. Apply and verify contrast/saturation/clamp effects |

### 4. Frame API (`frame_api.h`)

| Function | Test Cases |
|---|---|
| `oak_frame_release` | 1. Release CPU frame → `internal` freed, `data[0]` nulled <br>2. Release GPU frame → texture handle freed <br>3. Null frame → no crash |
| `oak_frame_release_internal_only` | 1. Release only `internal` (AVFrame), keep `data[0]` intact → used in renderer readback path <br>2. Null frame → no crash |

**`OakFrame` struct validation** (cross-module consistency):

- After `oak_decoder_read_video` → verify all fields populated: `width`, `height`, `pix_fmt`, `storage=CPU`, `pts_num/den`, `colorspace` non-null or empty, `planes >= 1`, `data[0]` non-null, `stride[0] > 0`.
- After `oak_engine_session_render_frame` → same checks, plus `pix_fmt == OAK_FRAME_PIX_RGBA32F`.

### 5. Cross-Module Integration Tests

File: `tests/gtest/c_api_integration_test.cpp`

| Scenario | Steps | Verification |
|---|---|---|
| **Decode → Color → Engine Render** | 1. `oak_decoder_open` on test MP4 <br>2. `oak_decoder_read_video` → `OakFrame` (RGBA32F) <br>3. `oak_color_config_load` → default config <br>4. `oak_color_processor_create` ("ACEScg" → "sRGB") <br>5. `oak_color_processor_apply` on decoded frame pixels <br>6. `oak_engine_project_load_xml` with a simple single-clip node graph <br>7. `oak_engine_session_render_frame` at same time | Engine output pixel values are close to color-transformed decoded pixels (tolerance 1e-2) |
| **Encode Roundtrip** | 1. Render 10 frames via engine <br>2. `oak_encoder_create` → MP4 <br>3. `oak_encoder_write_video` for each frame <br>4. `oak_encoder_finalize` <br>5. `oak_decoder_open` on output MP4 <br>6. `oak_decoder_read_video` | Decoded dimensions match original; pixel MSE below threshold |
| **Conform → Audio Decode** | 1. `oak_decoder_open` on test WAV <br>2. `oak_decoder_conform_audio` to 48kHz/mono <br>3. `oak_conform_poll` until 100 <br>4. `oak_decoder_read_audio_ex` with cache_path | Returns exactly expected sample count; sample values within tolerance of reference resampled audio |
| **GPU Shader → Engine Preview** | 1. Create display transform processor <br>2. `oak_color_gpu_shader_create` <br>3. Verify shader text compiles with mock OpenGL context <br>4. Verify 3D LUT count and texture dimensions are power-of-two or expected OCIO cube size (e.g., 33×33×33) |
| **dlopen Safety** | 1. Load engine + codec + color simultaneously in one process <br>2. Call APIs concurrently from 3 threads (one per module) <br>3. Unload in reverse order | No crash, no symbol resolution errors, ASan clean |

### 6. Negative & Edge Case Matrix

Run for **every** C API function:

| Category | Example |
|---|---|
| Null handle | Pass `NULL` where a handle is expected → no crash, predictable error code |
| Double-free | Call destroy/close/free twice → no crash |
| Use-after-free | Destroy handle, then call API → no crash (defensive), or ASan catches it in debug builds |
| Invalid enum | Pass `-1` or `999` to enum parameters → graceful failure |
| Zero dimensions | `width=0`, `height=0`, `sample_count=0` → graceful failure |
| Extreme dimensions | `width=1`, `height=1` and `width=16384`, `height=16384` → success or graceful OOM |
| Unaligned pointers | Pass misaligned `float*` to `processor_apply` → still works (or documented UB) |
| Empty strings | Pass `""` for path/space name → graceful failure |
| Concurrent access | Call `read_video` from 2 threads on same decoder → serializes or returns error (document behavior) |

### 7. Test Data / Assets

Place under `tests/assets/` (already tracked by git LFS or small binary files):

| Asset | Purpose | Size Target |
|---|---|---|
| `c_api/test_10frames_1920x1080_h264.mp4` | Video decode, thumbnail, engine render | < 500 KB |
| `c_api/test_1sec_48khz_stereo_float.wav` | Audio decode, conform, encoder audio track | < 200 KB |
| `c_api/test_1frame_4k_png.png` | OIIO decoder path, high-res frame alloc | < 100 KB |
| `c_api/identity_3dlut.cube` | LUT processor test (identity → no-op) | < 10 KB |
| `c_api/ocio_config_v2/config.ocio` | Minimal OCIO v2 config with ACEScg, sRGB, Rec.709 | < 50 KB |
| `c_api/minimal_project.ove.xml` | Single Viewer → Media node graph for engine load/render | < 5 KB |

### 8. CI Integration

Add a dedicated job step:

```yaml
- name: C API Full-Coverage Tests
  run: |
    cd build
    ctest -R olive-gtest --output-on-failure
    # ASan build
    cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address"
    cmake --build . --target olive-gtest
    ./tests/gtest/olive-gtest --gtest_filter="CAPEngine*:CAPCodec*:CAPColor*:CAPIntegration*"
```

### 9. Existing Test Enhancement Map

Several existing tests exercise the C++ paths but not the C API entry points. Add `dlopen` + C-call wrappers to verify dual-path parity:

| Existing Test | Enhancement |
|---|---|
| `codec_frame_test.cpp` | Add `TEST_F` that calls `oak_frame_alloc/free/convert` directly and compares output with C++ `Frame` equivalent. |
| `codec_encoder_test.cpp` | Add round-trip via C API: `oak_encoder_create` → write → finalize → `oak_decoder_open` → read → compare. |
| `plugin_renderer_readback_test.cpp` | Add C-path readback: `oak_engine_session_render_frame` + `oak_frame_get_plane`, verify pixel checksum matches GPU download path. |
| `shader_resources_test.cpp` | Add `oak_color_gpu_shader_create` → parse text → verify texture count and dimensions match C++ `ColorProcessor` GPU path. |

### 10. Completion Criteria

- [ ] `c_api_engine_test.cpp` compiles and all 20+ test cases pass.
- [ ] `c_api_codec_test.cpp` compiles and all 50+ test cases pass.
- [ ] `c_api_color_test.cpp` compiles and all 40+ test cases pass.
- [ ] `c_api_integration_test.cpp` compiles and all 5 scenarios pass.
- [ ] ASan run shows **zero** leaks and **zero** use-after-free across all four files.
- [ ] Codecov reports 100 % function-level coverage for symbols in `engine_api.h`, `codec_api.h`, `color_api.h`, `frame_api.h`.
- [ ] CI green on macOS, Linux, Windows.
