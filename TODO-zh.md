# TODO

## 目标
- 将渲染缓存重构为“内存 + 磁盘”的二级 LRU，并预留“显存 + 内存 + 磁盘”三级 LRU 的扩展路径。每一步必须可编译。

## 当前缓存现状（已有代码）
- `app/render/playbackcache.cpp/.h`：时间范围有效性 + 状态持久化，不存帧数据。
- `app/render/cache/framehashcache.h/.cpp`：磁盘帧缓存（EXR/JPG），继承 PlaybackCache。
- `app/render/cache/framecache.h`：内存 LRU 原型（基于 Texture，需整理并嵌入）。

## 嵌入思路（FrameMemCache 如何融入）
- `PlaybackCache` 作为“时间范围有效性层”。
- `FrameHashCache` 作为“磁盘层”。
- 将 `FrameMemCache` 重构为“内存层”，并组合成分层缓存：
  - `get()` 顺序：显存（未来）-> 内存 -> 磁盘。
  - `put()` 先写内存，再写磁盘（或延迟写磁盘）。
  - `invalidate()` 统一清理内存并更新 PlaybackCache；磁盘仍由 FrameHashCache 负责。

## 小步快跑计划（每步可编译）

### 步骤 1（可编译）：整理 `FrameMemCache` 使其可嵌入
- 将 `app/render/cache/framecache.h` 拆成 `framecache.h/.cpp`（先不改行为）。
- 移除 `gtest` 和测试相关头文件。
- 增加 include guard / `#pragma once`，完善 const 正确性。
- 为 `FrameCacheKey` 增加 `qHash(...)` 和 `const` 的 `operator==`，可用于 `QHash`。
- `FrameCacheEntry` 支持 CPU `FramePtr` 与 GPU `TexturePtr`（先仅用 CPU）。
- 增加统一的 `size_bytes()` 接口用于淘汰计算。

### 步骤 2（可编译）：定义统一的分层缓存接口
- 新增 `app/render/cache/framecachestore.h`：
  - `CacheKey`（版本、时间、VideoParams、代理模式、渲染缩放）。
  - `CacheValue`（FramePtr + 可选元数据）。
  - `FrameCacheStore` 接口：`get/put/invalidateByVersion/invalidateRange`。
- 适配 `FrameMemCache` 为 `FrameMemCacheStore`。

### 步骤 3（可编译）：为磁盘层增加封装
- 新增 `FrameDiskCacheStore` 作为薄封装：
  - 调用 `FrameHashCache::LoadCacheFrame/SaveCacheFrame`。
  - 仍使用 PlaybackCache 的有效范围与失效机制。

### 步骤 4（小行为）：当前帧的二级查询
- 在已有渲染路径中：
  - 先查内存层。
  - 未命中则查磁盘层，命中后加载到内存并返回。
  - 未命中则正常渲染，并写入内存，必要时写盘。
- 初期将缓存预算控制在很小范围。

### 步骤 5（行为）：内存层统一 LRU 淘汰
- 根据 `size_bytes()` + 内存预算淘汰最久未使用条目。
- 访问 `get()` 必须更新 LRU 顺序。

### 步骤 6（行为）：明确磁盘写入策略
- 增加 `write_through`（立即写盘）与 `write_back`（延迟写盘）策略。
- 默认采用保守的 `write_through`。

### 步骤 7（行为）：失效策略贯通
- `Invalidate/InvalidateAll` 时清理内存层对应时间范围。
- 磁盘失效仍由 FrameHashCache 与 DiskManager 处理。

### 步骤 8（面向未来，可编译）：增加显存层占位
- 新增 `FrameGpuCacheStore`（仅 TexturePtr，占位）。
- 形成三层顺序：显存 -> 内存 -> 磁盘。
- 通过特性开关或能力判断启用。

### 步骤 9（可选行为）：统计与日志
- 增加每层命中/未命中计数（仅 debug）。

## 待确认问题
- 内存预算默认值（按硬件分级）。
- 磁盘缓存保留策略与路径。
- GPU 缓存的上下文生命周期与失效策略。
