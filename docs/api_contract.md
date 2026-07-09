# vi_agent.dll 对外接口协议

## 1. 设计原则

- 使用 C ABI，便于 C++ 软件稳定调用。
- 不在 DLL 边界暴露 Qt、STL、C++ 类。
- 所有字符串均为 UTF-8。
- 所有复杂入参使用 JSON。
- 返回值使用错误码，错误详情通过 `vi_agent_getLastError` 获取。

## 2. 导出接口

```cpp
extern "C" __declspec(dllexport)
int vi_agent_init(const char* initJsonUtf8);

extern "C" __declspec(dllexport)
int vi_agent_openWindow(void* parentHwnd);

extern "C" __declspec(dllexport)
int vi_agent_closeWindow();

extern "C" __declspec(dllexport)
int vi_agent_uninit();

extern "C" __declspec(dllexport)
int vi_agent_getLastError(char* buffer, int bufferSize);
```

## 3. 错误码

```text
0    成功
-1   参数为空或非法
-2   JSON 解析失败
-3   尚未初始化
-4   Qt QApplication 不存在
-5   窗口创建失败
-6   配置校验失败
-100 未知异常
```

## 4. init JSON

示例：

```json
{
  "project_id": "wuliangye_line_a",
  "database_path": "D:/Device/Data/log.db",
  "profile_path": "D:/AgentRuntime/profiles/wuliangye_line_a.json",
  "log_paths": ["D:/Device/Logs/app_*.log"],
  "image_root": "D:/Device/Images",
  "model_endpoint": "https://example.com/v1",
  "api_key": "",
  "ui_language": "zh-CN"
}
```

字段说明：

```text
project_id      项目 ID，必填
database_path   SQLite 数据库路径，建议必填
profile_path    项目 profile 路径，建议必填
log_paths       日志路径列表，可选
image_root      图片根目录，可选
model_endpoint  联网模型服务地址，可选
api_key         模型服务 key，可选；正式环境建议传环境变量名
ui_language     UI 语言，默认 zh-CN
```

## 5. 调用流程

```cpp
const char* initJson = R"({
  "project_id": "wuliangye_line_a",
  "database_path": "D:/Device/Data/log.db",
  "profile_path": "D:/AgentRuntime/profiles/wuliangye_line_a.json"
})";

int rc = vi_agent_init(initJson);
if (rc != 0) {
    char err[1024] = {};
    vi_agent_getLastError(err, sizeof(err));
}

vi_agent_openWindow((void*)mainWindow->winId());

vi_agent_uninit();
```

## 6. openWindow 行为约定

```text
如果尚未初始化：
  返回 -3

如果 QApplication 不存在：
  返回 -4

如果窗口不存在：
  创建窗口，设置 owner，显示

如果窗口已存在：
  show / raise / activateWindow

函数应尽快返回，不阻塞宿主主界面。
```

