# TODO

## Goal
- Rework the render cache into a two-tier LRU (memory + disk) using the existing PlaybackCache/FrameHashCache, and leave a clear extension path to a three-tier LRU (GPU + memory + disk). Each step must compile.

## Current Cache Pieces (as-is)
- `app/render/playbackcache.cpp/.h`: time-range validity + persisted state, no frame data.
- `app/render/cache/framehashcache.h/.cpp`: disk-backed frame cache (EXR/JPG), inherits PlaybackCache.
- `app/render/cache/framecache.h`: prototype in-memory LRU cache (Texture-based, needs cleanup/embedding).

## Embedding Strategy (how FrameMemCache fits)
- Treat `PlaybackCache` as the **time-range validity layer**.
- Treat `FrameHashCache` as the **disk tier** (authoritative on-disk frames).
- Refactor `FrameMemCache` into a **memory tier** that can be composed above disk:
  - `get()` order: GPU (future) -> memory -> disk.
  - `put()` writes memory first, then disk (or disk on-demand).
  - `invalidate()` clears memory entries and updates PlaybackCache ranges; disk invalidation stays in FrameHashCache.

## Small-Step Plan (each step builds)

### Step 1 (compile-safe): Clean up `FrameMemCache` and make it embeddable
- Split `app/render/cache/framecache.h` into `framecache.h/.cpp` (no logic changes yet).
- Remove `gtest` include and any test-only headers.
- Add include guards / `#pragma once` and const-correct accessors.
- Make `FrameCacheKey` hashable for `QHash` (add `qHash(...)`) and equality operator `const`.
- Make `FrameCacheEntry` accept both CPU `FramePtr` and GPU `TexturePtr` (store one active pointer), but use CPU path only for now.
- Add a clear `size_bytes()` API for eviction accounting.

### Step 2 (compile-safe): Define a small tiered-cache interface
- Add a new header, e.g. `app/render/cache/framecachestore.h`:
  - `struct CacheKey` (version, time, VideoParams, proxy mode, render scale).
  - `struct CacheValue` (FramePtr + optional metadata).
  - `class FrameCacheStore { get/put/invalidateByVersion/invalidateRange }`.
- Implement `FrameMemCacheStore` by adapting `FrameMemCache` to that interface.

### Step 3 (compile-safe): Add a disk-store wrapper around `FrameHashCache`
- Implement `FrameDiskCacheStore` (thin wrapper) that:
  - Uses `FrameHashCache::LoadCacheFrame/SaveCacheFrame`.
  - Uses PlaybackCache ranges for validity and invalidation.
- No behavior changes yet (just compile).

### Step 4 (small behavior): Wire two-tier lookup for current frame only
- In the render path that already uses `FrameHashCache`:
  - Check memory tier first.
  - If miss, check disk tier; on hit, load to memory and serve.
  - On render miss, render normally, `put()` into memory, and optionally persist to disk.
- Keep cache sizes tiny to reduce risk.

### Step 5 (behavior): Centralize LRU eviction in memory tier
- Use `size_bytes()` + a memory budget to evict least-recently-used entries.
- Make the LRU list strictly reflect access order (`get()` updates LRU).

### Step 6 (behavior): Make disk writes explicit and batched
- Add policy flags: `write_through` (always write) vs `write_back` (defer).
- For now, default to safe `write_through`.

### Step 7 (behavior): Connect PlaybackCache invalidation to memory+disk
- On `Invalidate`/`InvalidateAll`, clear memory tier entries for those time ranges.
- Keep disk invalidation as-is (FrameHashCache already listens to `DiskManager`).

### Step 8 (future-ready compile-safe): Introduce GPU tier stubs
- Add a `FrameGpuCacheStore` that uses `TexturePtr` only (no actual data yet).
- Add the 3-tier selection order: GPU -> memory -> disk.
- Guard GPU tier behind a feature flag or runtime capability.

### Step 9 (optional behavior): Metrics and logging
- Add hit/miss counters per tier (debug only) to validate LRU behavior.

## Open Questions
- Default memory budget (MB) per hardware tier.
- Disk policy: how long to keep cached frames and where to store them.
- GPU cache ownership rules (context lifetime, eviction on context loss).
