# 版本管理建议

## 仓库纳入范围

建议纳入 Git：

- `include/`：DLL 对外 C ABI 头文件。
- `src/`：Agent DLL、Qt 对话框、Demo 主程序源码。
- `docs/`：架构、接口、构建、进度和版本说明。
- `profiles/`：项目 profile 示例和测试 profile。
- `database/*.sql`、`database/*.md`：测试库结构和种子数据定义。
- `scripts/`：构建、运行、安装 Qt、生成测试数据库等脚本。
- `sqlite_3.53.0/`：SQLite amalgamation 源码，体积可接受，便于离线构建。
- `CMakeLists.txt`、`.gitignore`、`VERSION.md`。

不建议纳入 Git：

- `build*/`：本机构建产物。
- `third_party/Qt/`：Qt 二进制 Kit，体积较大，应通过脚本安装或由部署包提供。
- `*.exe`、`*.dll`、`*.pdb`、`*.obj` 等编译输出。
- `*.db`：由 SQL 脚本生成的测试数据库文件。
- `*.log`：原始大日志样例，体积较大且可能包含现场数据。
- `aqtinstall.log`：安装过程日志。

## 当前版本标记

当前建议版本号为：

```text
v0.1.0-prototype
```

含义：

- `0.1.0`：第一个可运行原型。
- `prototype`：尚未进入真实设备软件稳定集成阶段。

建议在每次阶段性完成时更新：

- `VERSION.md`
- `docs/current_progress.md`
- Git tag，例如 `v0.1.0-prototype`

## 分支建议

早期可以保持简单：

- `main`：稳定可演示版本。
- `feature/*`：较大的新功能，例如 `feature/schema-validation`、`feature/llm-agent`。
- `fix/*`：缺陷修复，例如 `fix/qt-plugin-deploy`。

## 提交建议

推荐提交粒度：

- 一个功能闭环一次提交，例如“接入 SQLite 查询统计”。
- 一个文档主题一次提交，例如“补充 DLL API 约定”。
- 一个环境修复一次提交，例如“补充 Qt 5.12 运行脚本”。

提交信息建议使用简洁中文或英文均可，例如：

```text
初始化 VI Agent 原型工程
补充 SQLite 测试库和 profile
修复 Qt 5.12 部署脚本
```
