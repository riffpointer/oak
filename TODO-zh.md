# TODO

## 目标
- 在现有磁盘缓存（`FrameHashCache`）之上增加 LRU 内存缓存。
- 采用“小步快跑”方式推进，每一步都可编译、可回退。

## 范围
- 本轮范围：帧结果缓存（预览/渲染读写路径）。
- 本轮不做：GPU 缓存层、跨进程共享内存、音频缓存大改。

## 基本原则
- 复用现有链路：`PlaybackCache`（时间范围有效性）+ `FrameHashCache`（磁盘文件）。
- 避免并存两套架构。
- 未接入主路径的原型代码优先删除，而不是继续叠加改造。

## 小步快跑计划（LRU）

### 步骤 1：新增最小可用内存 LRU 容器
- 新增 `app/render/cache/framememorycache.h/.cpp`：
  - `FrameMemoryCacheKey`：`cache_uuid + timestamp + video params 签名`。
  - `FrameMemoryCacheValue`：`FramePtr` + `size_bytes`。
  - LRU 结构：哈希表 + 双向链表。
- 最小 API：
  - `bool Get(const Key&, FramePtr*)`
  - `void Put(const Key&, const FramePtr&)`
  - `void InvalidateRange(const QUuid&, const TimeRange&, const rational& timebase)`
  - `void InvalidateAll(const QUuid&)`
  - `void SetBudgetBytes(size_t)`

### 步骤 2：落实严格 LRU 淘汰
- 触发条件：`current_bytes > budget_bytes`。
- 循环淘汰最久未使用项，直到回到预算内。
- `Get()` 必须把命中项移动到 MRU。
- `Put()` 覆盖已有 key 时更新值并移动到 MRU。

### 步骤 3：接入“内存优先”的读取路径
- 在帧读取路径中：
  - 先查内存 LRU。
  - 内存未命中再走现有 `FrameHashCache` 磁盘读取。
  - 磁盘命中后回填内存 LRU。
- 失败回退逻辑与当前保持一致。

### 步骤 4：渲染完成后写穿透
- 每次新渲染得到帧后：
  - 先写入内存 LRU。
  - 继续沿用当前磁盘写入（`FrameHashCache::SaveCacheFrame`），即 write-through。
- 本阶段不引入 write-back。

### 步骤 5：打通失效清理
- 将 `PlaybackCache::Invalidate` / `InvalidateAll` 对应到内存 LRU 清理：
  - 同 cache UUID。
  - 同时间范围（按 cache timebase 换算 timestamp）。
- 磁盘失效逻辑继续由 `FrameHashCache`/`DiskManager` 负责。

### 步骤 6：补齐可观测性与保险开关
- 增加 debug 计数：
  - memory hit/miss/evict
  - disk hit/miss（可统计处）
- 增加内存预算配置项（保守默认值，例如 256MB）。
- 增加总开关，可一键禁用内存层。

### 步骤 7：验证与收敛
- 功能验证：
  - 重复 seek/loop 播放
  - 编辑后失效
  - 工程重开
- 压力验证：
  - 小预算内存
  - 高分辨率时间线
  - 长时间线随机访问

## 验收标准
- 开启内存缓存后，重复访问帧有稳定内存命中率提升。
- 编辑/失效后缓存正确性无回归。
- 关闭内存层时，磁盘缓存单独工作正常。
- 长时间压力下无崩溃、无明显泄漏。

## 后续（本轮不做）
- 可选 write-back 策略。
- 可选 GPU 缓存层（位于内存层之上）。
