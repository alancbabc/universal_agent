# SQLite 查询模块下一步实现计划

## 1. 目标

基于当前测试库 `database/test_vi_agent.db` 和 profile `profiles/test_vi_agent_profile.json`，实现第一版只读查询能力。

第一版只做确定性查询，不接自然语言模型：

- 打开 SQLite 数据库。
- 读取 profile。
- 校验 profile 中声明的表、视图、字段是否存在。
- 查询总检测数、NG 数、NG 率。
- 按相机统计。
- 按缺陷类型统计。
- 查询 WARN/ERROR 事件。

## 2. 建议模块

```text
ProjectProfile
  读取 profiles/test_vi_agent_profile.json
  保存实体到表字段的映射

SQLiteConnection
  使用 sqlite_3.53.0/sqlite3.h
  sqlite3_open_v2(..., SQLITE_OPEN_READONLY, ...)
  执行参数化 SELECT

SchemaInspector
  查询 sqlite_master
  PRAGMA table_info(...)
  校验字段存在性

MetricsRepository
  根据 profile 生成固定 SQL
  输出结构化统计数据

AnalysisEngine
  根据统计数据判断异常
```

## 3. 第一批固定 SQL

总量和 NG 率：

```sql
SELECT
    COUNT(*) AS total_count,
    SUM(CASE WHEN is_ok = 0 THEN 1 ELSE 0 END) AS ng_count,
    1.0 * SUM(CASE WHEN is_ok = 0 THEN 1 ELSE 0 END) / COUNT(*) AS ng_rate
FROM v_inspection_item_flat
WHERE detect_time BETWEEN ? AND ?;
```

按相机：

```sql
SELECT
    camera_id,
    camera_name,
    COUNT(*) AS total_count,
    SUM(CASE WHEN is_ok = 0 THEN 1 ELSE 0 END) AS ng_count,
    1.0 * SUM(CASE WHEN is_ok = 0 THEN 1 ELSE 0 END) / COUNT(*) AS ng_rate
FROM v_inspection_item_flat
WHERE detect_time BETWEEN ? AND ?
GROUP BY camera_id, camera_name
ORDER BY ng_rate DESC;
```

按缺陷：

```sql
SELECT
    result_code,
    result_name,
    COUNT(*) AS count
FROM v_inspection_item_flat
WHERE detect_time BETWEEN ? AND ?
GROUP BY result_code, result_name
ORDER BY count DESC;
```

运行事件：

```sql
SELECT
    event_time,
    level,
    event_type,
    source,
    message
FROM runtime_event
WHERE event_time BETWEEN ? AND ?
  AND level IN ('WARN', 'ERROR')
ORDER BY event_time;
```

## 4. UI 接入

`ViAgentDialog` 第一版可以增加一个“测试查询”按钮：

```text
点击按钮
  -> 读取 init 中的 database_path/profile_path
  -> 打开 SQLite
  -> 执行上面的固定 SQL
  -> 在 resultView_ 中展示统计文本
```

这一步完成后，就从“能弹窗”进入“能真实读取测试库并分析”。

