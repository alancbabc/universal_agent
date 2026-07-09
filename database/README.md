# 测试 SQLite 数据库设计

这个目录用于第一版 agent 开发和联调。表结构不是某个真实项目的最终结构，而是根据当前缺陷检测日志和后续分析需求设计的测试结构。

## 文件

- `test_schema.sql`：建表、索引、视图。
- `test_seed.sql`：插入确定性的测试数据和异常场景。
- `test_vi_agent.db`：生成后的测试数据库，执行 SQL 后产生。

## 核心表

```text
project_info
  项目信息。

camera
  相机基础信息。

model_version
  模型名称、版本、阈值和部署时间。

inspection_batch
  一次 PLC 触发或一个产品批次。

inspection_item
  单相机检测结果，包含 OK/NG、缺陷类型、置信度、耗时、模型版本。

image_asset
  检测图片路径和尺寸。

defect_instance
  单个缺陷框，包含 bbox、缺陷类型、置信度、严重度。

runtime_event
  PLC、收图、推理、WARN 等运行事件。
```

## 视图

```text
v_inspection_item_flat
  给统计查询使用的扁平结果视图。

v_defect_instance_flat
  给缺陷证据链和图片 bbox 展示使用的扁平缺陷视图。
```

## 内置异常场景

测试数据会生成 120 个触发批次、4 个相机、480 条单相机检测记录，并刻意注入：

- `DA8352862` 在部分时间段 `HD` 缺陷升高。
- `DA8352848` 在部分时间段出现 `ZW` 缺陷。
- `DA8352810` 出现连续 `HD`。
- `DA8352821` 偶发 `QL`。
- 第 60-66 个批次推理耗时升高。
- 两条 `批次缓存结果重置` WARN 事件。
- 模型版本从 `v1.8.2` 切到 `v1.8.3`。

## 生成方式

```powershell
sqlite3 database/test_vi_agent.db ".read database/test_schema.sql" ".read database/test_seed.sql"
```

