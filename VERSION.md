# VI Agent 版本说明

## 当前版本

版本号：`v0.1.0-prototype`

版本日期：`2026-07-09`

版本定位：缺陷检测日志分析 Agent 的端到端原型版本。该版本重点验证“主程序调用 DLL、DLL 初始化配置、DLL 弹出 Qt 界面、Qt 界面读取 SQLite 并输出基础统计分析”的最小闭环。

## 当前实现程度

已完成：

- DLL 导出接口雏形：`vi_agent_init`、`vi_agent_openWindow`、`vi_agent_closeWindow`、`vi_agent_uninit`、`vi_agent_getLastError`。
- Demo 主程序：可模拟设备软件启动时调用 `init(json)`，按钮触发 `openWindow(parentHwnd)`。
- Qt 5.12.12 界面：DLL 内部弹出对话框；重复打开时复用已有窗口并置前。
- 初始化配置：支持通过 JSON 传入项目名、数据库路径、profile 路径、时间范围、LLM 开关等信息。
- SQLite 访问链路：基于 profile 中的表/字段映射查询测试数据库。
- 测试数据库设计：包含批次、检测条目、缺陷明细、事件日志等表，并提供 seed 数据。
- 基础统计分析：总检测数、NG 数、NG 率、平均推理耗时、相机维度统计、缺陷类型分布、WARN/ERROR 事件。
- 构建链路：已使用 VS2022 Build Tools + Qt 5.12.12 + NMake 成功构建 `vi_agent.dll` 和 `vi_agent_demo.exe`。
- Qt 部署链路：已通过 `windeployqt` 补齐 `platforms/qwindows.dll` 等运行依赖。

未完成：

- 自然语言指令解析和多轮 Agent 规划。
- LLM 接入、工具调用编排、分析结果解释生成。
- 针对真实项目数据库 schema 的 profile 适配和校验。
- 时间范围选择、缺陷图片预览、bbox 可视化等正式交互界面。
- 更完整的异常规则：连续 NG、相机偏高、模型版本异常、置信度漂移、推理耗时异常等。
- C++ 宿主软件正式集成测试、线程模型和 Qt 事件循环边界验证。
- DLL ABI 稳定性、错误码体系、日志落盘、配置加密/脱敏。
- 单元测试、集成测试和发布包制作。

## 当前可演示链路

```text
vi_agent_demo.exe
  -> vi_agent_init(json)
  -> vi_agent_openWindow(parentHwnd)
  -> vi_agent.dll 内部创建/置前 Qt 对话框
  -> 点击“测试分析流程”
  -> 读取 profiles/test_vi_agent_profile.json
  -> 打开 database/test_vi_agent.db
  -> 执行统计 SQL
  -> 在 Qt 文本区域输出基础统计和初步异常判断
```

## 推荐版本节奏

- `v0.1.x`：本地 DLL + Qt + SQLite 原型验证。
- `v0.2.x`：数据库 schema/profile 校验、时间范围 UI、图片/bbox 查询。
- `v0.3.x`：规则分析增强、自然语言查询到结构化分析计划。
- `v0.4.x`：LLM Agent 接入、工具调用闭环、报告生成。
- `v1.0.0`：真实设备软件集成、发布包、稳定 ABI、完整测试。
