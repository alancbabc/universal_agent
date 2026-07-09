# 当前进展

## 当前版本

当前版本：`v0.1.0-prototype`

版本日期：`2026-07-09`

当前定位：端到端原型。重点证明宿主程序能够调用 DLL，DLL 能初始化 JSON 配置、弹出 Qt 界面，并基于本地 SQLite 测试库完成基础统计分析。

## 已打通链路

```text
main.exe / vi_agent_demo.exe
  -> vi_agent_init(json)
  -> vi_agent_openWindow(parentHwnd)
  -> vi_agent.dll 内部弹出 Qt 对话框
  -> 点击“测试分析流程”
  -> 读取 Project Profile
  -> 打开 SQLite 测试库
  -> 执行固定统计 SQL
  -> 在 Qt 文本框输出基础统计和初步异常判断
```

这已经是第一版端到端基础链路。与早期占位流程不同，现在按钮会真实读取 `database/test_vi_agent.db`。

## 当前测试库结果

测试时间范围：

```text
2026-06-14 02:45:00.000 ~ 2026-06-14 03:25:00.000
```

总体：

```text
总检测数：480
NG 数：22
NG 率：4.58%
平均推理耗时：94.0 ms
```

按相机：

```text
DA8352862 / 相机 4：120，总 NG 8，NG 率 6.67%
DA8352810 / 相机 1：120，总 NG 7，NG 率 5.83%
DA8352848 / 相机 3：120，总 NG 4，NG 率 3.33%
DA8352821 / 相机 2：120，总 NG 3，NG 率 2.50%
```

结果分布：

```text
OK / 正常：458
HD / 黑点：15
ZW / 脏污：4
QL / 缺料：3
```

WARN/ERROR：

```text
2026-06-14 03:07:30.000 WARN batch_cache_reset 批次缓存结果重置
2026-06-14 03:18:00.000 WARN batch_cache_reset 批次缓存结果重置
```

## 构建和运行状态

当前工作机已安装：

```text
VS2022 Build Tools
Qt 5.12.12 kit: third_party/Qt/5.12.12/msvc2017_64
```

已通过 `NMake Makefiles` 成功编译：

```text
build_nmake_qt512/vi_agent.dll
build_nmake_qt512/vi_agent_demo.exe
```

Qt 运行依赖已通过 `windeployqt` 部署，包含：

```text
build_nmake_qt512/platforms/qwindows.dll
```

## 当前限制

- 目前还不是自然语言 Agent，只是固定按钮触发的基础统计分析。
- 目前只适配测试 SQLite schema，真实项目需要各自 profile 映射。
- 目前异常判断是基础规则，不包含连续 NG、相机漂移、模型版本异常、置信度漂移等规则。
- 目前图片路径、缺陷框、置信度、模型版本等字段已在方案中考虑，但测试 UI 尚未完整展示。
- 目前 DLL 与宿主软件的正式线程模型、Qt 事件循环边界、发布包 ABI 稳定性还需要进一步验证。

## 下一步建议

建议进入 `v0.2.x` 阶段：

- 增加 schema 校验：确认 profile 声明的表、视图、字段全部存在。
- 增加时间范围选择 UI。
- 增加图片和 bbox 查询与展示。
- 增加连续 NG、相机偏高、推理耗时异常等规则。
- 再接入自然语言到结构化分析计划。