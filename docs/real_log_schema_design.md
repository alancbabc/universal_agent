# 真实日志数据库设计 v0.2

## 目标

把 `app_20260613_180017.log` 转换为 Agent 可稳定查询的 SQLite 语义数据库。设计重点不是完整复刻软件内部状态，而是保留对数据分析和原因推断真正有价值的信息：检测事实、缺陷事实、耗时、设备事件、原始证据。

## 日志样本事实

当前样本导入后的校验结果：

```text
raw_log_line=101527
inspection_trigger=2715
inference_run=2715
inspection_item=10860
defect_observation=575
equipment_event=38152
entity_evidence=90312
```

检测结果：

```text
OK: 10336
NG: 524
```

缺陷实例：

```text
HD: 489
ZW: 78
QL: 8
```

相机 NG 率：

```text
DA8352848: total=2715, ng=211, ng_rate=7.77%
DA8352862: total=2715, ng=181, ng_rate=6.67%
DA8352810: total=2715, ng=69, ng_rate=2.54%
DA8352821: total=2715, ng=63, ng_rate=2.32%
```

## 当前保留的核心表

```text
log_import
raw_log_line
camera
inspection_trigger
inspection_item
defect_observation
inference_run
equipment_event
entity_evidence
defect_dictionary
```

## 核心设计取舍

### 保留原始日志层

`raw_log_line` 保存所有原始日志行。原因：

- 解析规则后续会迭代，可以重放。
- Agent 给出的结论必须能追溯证据。
- 未识别日志也不能丢。

### 使用内部主键 `trigger_pk`

`trigger_id` 不作为全局主键，因为真实设备可能跨天、重启、换批次后重复。内部使用：

```text
inspection_trigger.trigger_pk
```

外部业务 ID 保留为：

```text
inspection_trigger.trigger_id
```

### 只把真实缺陷写入 `defect_observation`

`OK` 不进入缺陷实例表。当前日志存在复合结果：

```text
结果 = HD, HD
结果 = HD, HD, HD, HD
```

因此：

- `inspection_item.result_raw` 保留完整结果。
- `inspection_item.result_primary_code` 用于快速展示。
- `defect_observation` 按 token 拆分真实缺陷。

### 合并设备事件

v0.2 不再提前拆成 `plc_signal_event`、`plc_write_event`、`runtime_event`。统一使用：

```text
equipment_event
```

用字段区分：

```text
event_category: PLC / PROCESS / SYSTEM
event_type: plc_write / plc_bridge_changed / batch_cache_reset / ...
```

这样能保留关键设备信息，同时减少早期 schema 复杂度。

### 统一证据关系

不在每张业务表里放大量 `source_xxx_line_id` 字段，而是统一使用：

```text
entity_evidence
```

示例：

```text
entity_type = inspection_item
entity_id = 123
evidence_role = detect_done
line_id = 456
```

这样后续任何实体都能关联多条原始日志证据。

## Agent 视图

当前提供这些语义视图：

```text
v_inspection_item_flat
v_defect_instance_flat
v_equipment_event_flat
v_runtime_event_flat
v_trigger_summary
v_anomaly_context
```

Agent 优先查询视图，而不是直接操作底层表。

## 可支持的分析

当前设计可支持：

- 任意时间段检测数、NG 数、NG 率。
- 按相机统计 NG 率和缺陷分布。
- 按缺陷类型统计。
- 复合缺陷拆分统计。
- 推理耗时、收图耗时、进入检测耗时、出结果耗时分析。
- 连续 NG 和时间窗口聚集分析。
- WARN、PLC 信号、PLC 写入与异常时段的关联分析。
- 结论到原始日志的证据追溯。

## 当前不能完整支持的分析

当前真实日志缺少：

```text
图片路径
bbox
置信度
模型版本
```

因此当前不能完整支持：

- 图像区域级原因定位。
- bbox 偏移分析。
- 置信度漂移分析。
- 模型版本对比分析。

schema 和 profile 已预留相关字段，后续需要从真实业务数据库补齐。

## 当前导入链路

推荐命令：

```bat
scripts\generate_real_log_db.bat
```

该脚本会生成：

```text
database/real_log_vi_agent.db
```

该数据库是本地生成物，已被 `.gitignore` 忽略。

## 后续建议

下一步应把 Qt 调试界面切到 `profiles/real_log_profile.json`，并增加开始时间、结束时间输入框。这样 Agent 就可以直接基于真实日志库做任意时段分析。