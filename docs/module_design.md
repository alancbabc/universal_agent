# 模块设计

## 1. exported_api

职责：

- 提供 DLL 导出函数。
- 捕获异常，避免异常跨 DLL 边界。
- 将错误写入 `AgentRuntime`。

不负责：

- 不直接访问数据库。
- 不直接执行业务分析。
- 不暴露 Qt 对象给宿主。

## 2. AgentRuntime

职责：

- 保存当前初始化状态。
- 保存 `AgentConfig`。
- 管理唯一 `ViAgentDialog`。
- 保存最后错误。
- 管理后续 DataAccess、ProjectProfile、AnalysisEngine 生命周期。

关键规则：

- `init` 可重复调用。重复调用时更新配置，并关闭旧窗口。
- `uninit` 释放窗口和配置。
- 所有 public 方法返回错误码。

## 3. AgentConfig

职责：

- 解析 `initJsonUtf8`。
- 校验必填字段。
- 保存原始 JSON，便于调试。

关键字段：

```text
projectId
databasePath
profilePath
logPaths
imageRoot
modelEndpoint
apiKey
uiLanguage
rawJson
```

## 4. ViAgentDialog

第一版职责：

- 显示项目 ID、数据库路径、profile 路径。
- 提供一个问题输入框。
- 提供调试按钮。
- 展示当前状态和后续分析结果。

后续职责：

- 查询条件区。
- 对话区。
- 统计结果区。
- 图表区。
- 图片和 bbox 证据区。

## 5. DataAccess

后续职责：

- SQLite 只读连接。
- Schema introspection。
- 参数化查询。
- 查询结果转统一 table。
- 查询超时、limit、分页。

安全约束：

- 只允许 SELECT。
- 禁止多语句。
- 禁止写操作。
- 必须有查询记录日志。

## 6. ProjectProfile

后续职责：

- 读取项目 profile JSON。
- 将项目表字段映射为统一业务对象。
- 提供字段合法性校验。

## 7. AnalysisEngine

后续职责：

- 良率/NG 率。
- 相机维度分析。
- 缺陷类型分析。
- 时间趋势分析。
- 置信度分析。
- 模型版本对比。
- 异常规则。

## 8. LlmClient

后续职责：

- 调用联网模型服务。
- 将用户问题转成结构化分析计划。
- 将统计结果转成人类可读解释。

关键约束：

- LLM 不直接执行 SQL。
- LLM 不直接接触数据库连接。
- 图片上传需配置开关。

