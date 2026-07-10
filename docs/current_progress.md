# 当前进展

## 当前版本

当前实现版本：`v0.2.1-dev`

版本日期：`2026-07-10`

当前定位：真实日志语义库分析原型。已经从最早的 DLL/Qt 弹窗原型，推进到可由 demo 宿主程序启动 Agent、读取真实日志导入后的 SQLite 数据库、按用户输入的任意时间范围生成统计分析报告。

## 已打通链路

```text
vi_agent_demo.exe / main.exe
  -> 程序启动时调用 vi_agent_init(json)
  -> init json 传入 project_id、database_path、profile_path、log_paths 等信息
  -> 点击“打开 Agent 对话框”
  -> vi_agent_openWindow(parentHwnd)
  -> vi_agent.dll 直接弹出 Qt 对话框
  -> 对话框读取 Project Profile 默认时间范围
  -> 用户可修改开始时间、结束时间、输入分析问题
  -> 点击“分析当前时间段”
  -> MetricsRepository 加载 profile 映射
  -> 只读打开本地 SQLite 数据库
  -> 查询真实日志语义视图
  -> 输出统计结果、异常判断和证据事件
```

当前 demo 默认初始化配置已经切到：

```text
database_path: database/real_log_vi_agent.db
profile_path: profiles/real_log_profile.json
```

本地生成的 `database/real_log_vi_agent.db` 不纳入 Git，可通过 `scripts/generate_real_log_db.bat` 从真实 log 重新生成。

## 真实日志数据库部分

当前真实日志导入链路已落地：

```text
真实 app log
  -> scripts/import_real_log_fast.py
  -> database/real_log_schema_proposal.sql
  -> database/real_log_vi_agent.db
  -> profiles/real_log_profile.json
  -> vi_agent.dll 查询分析
```

当前语义 schema 版本：`real_log_v0.2`

核心表：

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

核心视图：

```text
v_inspection_item_flat
v_defect_instance_flat
v_equipment_event_flat
v_runtime_event_flat
v_trigger_summary
v_anomaly_context
```

当前导入验证数据量：

```text
raw_log_line: 101527
inspection_trigger: 2715
inference_run: 2715
inspection_item: 10860
defect_observation: 575
equipment_event: 38152
entity_evidence: 90312
```

当前真实日志时间范围：

```text
2026-06-14 02:45:33.051 ~ 2026-06-14 08:32:17.161
```

## 当前分析能力

Qt 对话框当前支持：

- 显示 init JSON 传入的项目 ID、数据库路径、profile 路径、模型服务地址。
- 自动读取 profile 中的默认分析时间范围。
- 手动输入开始时间和结束时间。
- 输入用户问题文本，当前用于报告上下文记录，尚未进入自然语言规划。
- 点击按钮生成当前时间段统计分析报告。

`MetricsRepository` 当前支持：

- 按任意时间范围统计检测项总数、NG 数、NG 率。
- 按相机统计总数、NG 数、NG 率，并识别最高 NG 相机。
- 优先使用 `defect_instance` 视图统计真实缺陷实例分布。
- 在没有缺陷实例映射时，回退到检测项缺陷结果分布。
- 统计 `infer_ms`、`trigger_to_receive_ms`、`receive_to_detect_ms`、`to_detect_to_result_ms` 的 min、avg、p95、max。
- 读取 profile 中的耗时阈值，标记 P95 超阈值。
- 检查各相机最长连续 NG，使用 profile 中的 `continuous_ng_warn` 阈值。
- 查询同时间段 WARN/ERROR 运行事件，并作为异常判断证据。
- 输出初步异常原因建议，例如相机 NG 偏高、连续 NG、取图耗时偏高、推理耗时偏高、运行事件关联等。

## 当前真实日志基准结果

完整时间范围内，当前真实日志库的基准统计为：

```text
DA8352848: total=2715, ng=211, ng_rate=7.77%
DA8352862: total=2715, ng=181, ng_rate=6.67%
DA8352810: total=2715, ng=69,  ng_rate=2.54%
DA8352821: total=2715, ng=63,  ng_rate=2.32%
```

缺陷实例分布：

```text
HD: 489
ZW: 78
QL: 8
```

这些统计来自真实 log 导入后的语义库，不再是早期手工构造的测试库。

## 构建和运行状态

当前工作机已具备：

```text
VS2022 Build Tools
Qt 5.12.12 kit: third_party/Qt/5.12.12/msvc2017_64
CMake
NMake
SQLite amalgamation: sqlite_3.53.0
```

已通过以下脚本成功编译：

```powershell
scripts\build_vs2022_nmake.bat
```

当前构建产物：

```text
build_nmake/vi_agent.dll
build_nmake/vi_agent_demo.exe
build_nmake/platforms/qwindows.dll
```

`windeployqt` 已自动部署 Qt 运行依赖和 `platforms/qwindows.dll`，之前遇到的 Qt platform plugin 缺失问题已在构建脚本层面处理。

## 当前限制

- 当前还不是完整自然语言 Agent；用户问题暂时作为报告上下文，尚未自动拆成 SQL/统计/推理计划。
- 当前分析入口是固定报告模板，不是任意自由查询。
- 尚未加入 schema 自动校验；如果宿主传入的 profile 字段和数据库不匹配，会在 SQL 执行阶段报错。
- 当前真实 log 没有输出图片路径、缺陷框、置信度、模型版本等字段；schema 和 profile 已预留，后续需要从项目业务数据库补齐。
- 图片查看、bbox 叠加、置信度分布、模型版本对比尚未接入 Qt 界面。
- DLL 直接在宿主进程中弹 Qt 对话框，正式部署前仍需和主软件确认 Qt 事件循环、线程调用、插件路径和运行库版本边界。

## 后续执行计划

建议下一阶段进入 `v0.2.2-dev`：

- 增加 schema/profile 校验模块：启动时检查表、视图、字段是否存在。
- 增加数据库结构文档路径输入：支持宿主把数据库说明文档传给 Agent。
- 增加结构化分析计划层：把用户意图拆成时间范围、维度、指标、过滤条件和证据查询。
- 增加更多统计模板：按小时趋势、相机 x 缺陷类型交叉表、NG 高峰窗口、事件关联窗口。
- 增加图片证据链：按缺陷记录查询图片路径、bbox、置信度，并在 Qt 界面展示。
- 接入 LLM 或本地规则/模型混合推理：把统计结果和证据转换为更自然的原因分析说明。
- 补充 DLL 发布包结构：明确 `vi_agent.dll`、Qt DLL、platforms、profiles、schema 文档和示例数据库的部署目录。