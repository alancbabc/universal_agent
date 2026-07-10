# 真实日志导入与分析链路

## 当前落地版本

版本：`real_log_v0.2`

目标：把 `app_20260613_180017.log` 从原始文本转成可供 Agent 查询的 SQLite 语义数据库。

生成物：

```text
database/real_log_vi_agent.db
```

该 `.db` 是本地生成物，不提交 Git。

## 核心表

v0.2 收敛后的核心表：

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

设计原则：

- `raw_log_line` 保留每行原始日志，作为证据和重解析基础。
- `inspection_item` 是核心检测事实表，一条记录表示“一个触发 ID + 一个相机”的检测结果。
- `defect_observation` 只保存真实缺陷 token，不保存 `OK`。
- `equipment_event` 合并 PLC 信号、PLC 写入、WARN、流程事件，避免过早拆表。
- `entity_evidence` 统一保存语义实体到原始日志行的证据关系。
- `trigger_id` 不作为全局主键，使用 `trigger_pk` 作为内部主键，避免多日志/多天导入时冲突。

## Agent 视图

对 Agent 暴露的语义视图：

```text
v_inspection_item_flat
v_defect_instance_flat
v_equipment_event_flat
v_runtime_event_flat
v_trigger_summary
v_anomaly_context
```

当前 `profiles/real_log_profile.json` 已指向这些视图。

## 生成命令

推荐使用：

```bat
scripts\generate_real_log_db.bat
```

等价 Python 命令：

```bat
python scripts\import_real_log_fast.py --log app_20260613_180017.log --db database\real_log_vi_agent.db --schema database\real_log_schema_proposal.sql --project-id wuliangye_line_a
```

`import_real_log_fast.py` 会复用基础导入器，并替换慢速时间关联函数。

## 当前导入校验结果

已基于真实日志生成数据库，结果如下：

```text
raw_log_line=101527
inspection_trigger=2715
inference_run=2715
inspection_item=10860
defect_observation=575
equipment_event=38152
entity_evidence=90312
```

检测结果分布：

```text
OK: 10336
HD: 414
ZW: 78
HD, HD: 17
QL: 8
HD, HD, HD, HD: 3
其他复合 HD 结果：4
```

缺陷实例分布：

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

这些统计与前面对原始日志的抽样分析一致。

## 当前可支持的分析

基于真实日志库，Agent 可以做：

- 任意时间段总检测数、NG 数、NG 率。
- 按相机统计 NG 率和缺陷类型。
- 按缺陷类型统计分布。
- 复合缺陷拆分统计，例如 `HD, HD, HD`。
- 推理耗时、收图耗时、进入检测耗时、出结果耗时分析。
- 连续 NG 和时间窗口聚集分析。
- 设备事件、WARN、PLC 信号与异常时段的关联分析。
- 每个结论追溯到原始日志行。

## 当前不足

当前真实日志不包含：

- 图片路径
- bbox
- 置信度
- 模型版本

因此当前库暂时不能做完整图像级定位、置信度漂移、模型版本对比。schema 和 profile 已预留这些字段，后续需要从项目真实业务数据库补齐。

## 下一步

建议下一步把现有 Qt 调试入口切到 `profiles/real_log_profile.json`，并在界面增加开始/结束时间输入，让 Agent 直接基于真实日志库做任意时段分析。
