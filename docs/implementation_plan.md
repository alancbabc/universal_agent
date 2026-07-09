# VI Agent 日志数据分析功能实施方案

## 1. 当前目标

第一阶段目标是完成一个可联调的 `vi_agent.dll`：

- 宿主程序 `main.exe` 启动时调用 `vi_agent_init(json)`。
- 宿主程序按钮点击时调用 `vi_agent_openWindow(parentHwnd)`。
- `vi_agent.dll` 内部直接弹出 Qt 对话框。
- 重复点击打开按钮时复用已有窗口并置前。
- 先完成配置解析、窗口弹出、基础状态展示和模块骨架。
- 后续逐步接入 SQLite 查询、项目 profile、统计分析、异常原因分析和联网模型。

当前确认的第一版实现方式：

```text
main.exe / 设备软件
  -> vi_agent.dll::vi_agent_init(initJson)
  -> vi_agent.dll::vi_agent_openWindow(parentHwnd)
  -> vi_agent.dll 内部创建/置前 Qt 对话框
```

正式部署如果遇到 Qt 版本冲突、崩溃隔离、插件路径冲突等问题，保持 DLL 对外接口不变，将 DLL 内部替换为启动独立 `AgentHost.exe`。

## 2. 第一阶段边界

第一阶段必须完成：

- C ABI 导出接口。
- JSON 初始化协议。
- Qt 对话框唯一实例管理。
- `parentHwnd` 传入和窗口置前逻辑。
- 错误码和 `vi_agent_getLastError`。
- 示例 profile。
- 调试用 `main.exe`。

第一阶段暂不强制完成：

- 完整自然语言 agent。
- 所有项目数据库适配。
- 联网模型服务。
- 图片缺陷框渲染。
- 完整异常归因报告。

这些能力作为第二阶段以后逐步接入。

## 3. 分阶段计划

### 阶段 0：工程和接口打通

交付物：

- `vi_agent.dll`
- `vi_agent_demo.exe`
- `include/vi_agent_api.h`
- `docs/api_contract.md`
- `profiles/sample_wuliangye.json`

验收标准：

- demo 启动后调用 `vi_agent_init` 成功。
- 点击按钮后弹出 agent 对话框。
- 多次点击只置前已有对话框。
- 传入非法 JSON 能返回错误。

### 阶段 1：SQLite 只读查询

交付物：

- SQLite 只读连接模块。
- Schema 检查模块。
- 基础 SQL 查询封装。

验收标准：

- 能打开项目 SQLite 数据库。
- 能读取表结构。
- 能执行只读查询。
- 禁止写 SQL。
- 大查询有 limit 和时间范围保护。

### 阶段 2：Project Profile 映射

交付物：

- profile schema。
- 表字段映射。
- defect 字典。
- 统一业务模型。

验收标准：

- 不同项目只需换 profile 即可映射到统一模型。
- 缺字段时给出明确错误。
- 支持单表和多表 join。

### 阶段 3：固定统计分析

交付物：

- 总数、OK 数、NG 数、NG 率。
- 按相机统计。
- 按缺陷类型统计。
- 按时间桶趋势。
- 置信度统计。
- 模型版本分布。

验收标准：

- 用户选择时间范围后能看到统计结果。
- 统计结果可追溯到 SQL 和原始记录。

### 阶段 4：异常分析和证据链

交付物：

- 相机异常偏高分析。
- 缺陷突增分析。
- 连续 NG 分析。
- 置信度异常分析。
- 模型版本切换前后对比。
- 图片路径、bbox、置信度证据展示。

验收标准：

- 能输出异常类型、严重程度、证据和建议。
- 能打开图片并叠加缺陷框。

### 阶段 5：自然语言 agent

交付物：

- LLM client。
- 意图识别。
- 结构化分析计划。
- 结果解释生成。

验收标准：

- 用户可输入“分析昨天下午 2 点到 4 点是否异常”。
- Agent 将问题转为结构化计划，而不是直接生成 SQL。
- SQL 仍由本地 QueryBuilder 生成。

## 4. 模块拆解

```text
exported_api
  DLL 导出层，只负责参数校验和转发。

AgentRuntime
  保存全局配置、窗口实例、错误信息和模块生命周期。

AgentConfig
  解析 init JSON，保存 project_id、database_path、profile_path 等。

ViAgentDialog
  第一版 Qt 对话框，展示配置状态和调试按钮。

DataAccess
  SQLite 只读访问，后续接入。

ProjectProfile
  项目表结构映射，后续接入。

AnalysisEngine
  统计和异常规则，后续接入。

LlmClient
  联网模型调用，后续接入。
```

## 5. 关键技术约束

- DLL 导出接口不能暴露 Qt 类型、C++ STL 类型。
- 对外字符串统一 UTF-8。
- `openWindow` 必须支持传 `parentHwnd`。
- `openWindow` 重复调用时复用已有窗口。
- 第一版假设宿主进程已经创建 `QApplication`。
- `vi_agent.dll` 与宿主建议使用同一 Qt 版本和同一编译器运行库。
- SQLite 数据库默认只读访问，不能影响设备软件写库。

## 6. 当前需要团队确认的后续问题

- 宿主软件使用 Qt5 还是 Qt6。
- 宿主软件编译器版本：MSVC 版本或 MinGW。
- 是否允许 `vi_agent.dll` 链接 Qt6，还是必须跟随宿主 Qt 版本。
- 第一批真实 SQLite 表结构。
- profile 文件由宿主传路径，还是由 agent 按 project_id 自动查找。

