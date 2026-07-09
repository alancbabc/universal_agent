# 构建和运行说明

## 1. 已确认环境

当前按以下环境作为第一版主目标：

- Qt 5.12.12
- Visual Studio 2022
- C++17
- Windows
- 本仓库内置 `sqlite_3.53.0/sqlite3.c`，构建时直接编进 `vi_agent.dll`

注意：VS2022 默认 MSVC 工具集是 v143。Qt 5.12.12 常见预编译包可能是 `msvc2017_64`，对应 v141 工具集。MSVC 2015 以后有较好的二进制兼容性，但为了减少 Qt ABI 和运行库问题，建议优先使用与 Qt 包匹配的 MSVC 工具集。

推荐优先级：

1. 如果 Qt 5.12.12 是你们自己用 VS2022/v143 编译的，直接使用 VS2022 默认生成器。
2. 如果 Qt 5.12.12 是官方/已有 `msvc2017_64` 包，建议在 VS2022 Installer 中安装 `MSVC v141 build tools`，并用 `-T v141` 构建。
3. 如果现有设备软件已经稳定使用某个 Qt kit，`vi_agent.dll` 应尽量使用同一套 Qt bin/lib/include 和同一运行库配置。

## 2. CMake 构建

当前机器已安装 VS2022 Build Tools。推荐用 `NMake Makefiles` 构建：

```powershell
scripts\build_vs2022_nmake.bat third_party\Qt\5.12.12\msvc2017_64 build_nmake_qt512
```

也可以不传第一个参数，脚本会默认使用本仓库内已安装的 Qt 5.12.12 kit。

```powershell
scripts\build_vs2022_nmake.bat
```

Qt 5.12.12 + VS2022 + v141 示例：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -T v141 -DCMAKE_PREFIX_PATH="C:/Qt/5.12.12/msvc2017_64"
cmake --build build --config Release
```

如果 Qt 5.12.12 是 v143/VS2022 编译版本：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/5.12.12/msvc2022_64"
cmake --build build --config Release
```

如果实际 Qt 安装路径不同，把 `CMAKE_PREFIX_PATH` 改为包含 `lib/cmake/Qt5/Qt5Config.cmake` 的 Qt kit 根目录。

## 3. 运行 demo

```powershell
.\build\Release\vi_agent_demo.exe
```

demo 启动后：

- 程序自动调用 `vi_agent_init`。
- 点击“打开 Agent 对话框”会调用 `vi_agent_openWindow(parentHwnd)`。
- 重复点击只会置前已有对话框。

如果运行时提示缺 Qt DLL，需要把 Qt 的 `bin` 目录加入 PATH，或用 Qt 自带的 `windeployqt` 部署：

```powershell
C:\Qt\5.12.12\msvc2017_64\bin\windeployqt.exe .\build\Release\vi_agent_demo.exe
```

## 4. 宿主软件集成

宿主软件只需要包含：

```cpp
#include "vi_agent_api.h"
```

并链接或动态加载 `vi_agent.dll`。

建议使用动态加载，便于现场替换 DLL：

```cpp
HMODULE dll = LoadLibraryW(L"vi_agent.dll");
auto init = reinterpret_cast<int(*)(const char*)>(GetProcAddress(dll, "vi_agent_init"));
auto openWindow = reinterpret_cast<int(*)(void*)>(GetProcAddress(dll, "vi_agent_openWindow"));
```

## 5. Qt 进程内 DLL 注意事项

当前第一版是 `vi_agent.dll` 直接在宿主进程内弹 Qt 对话框，因此要求：

- 宿主进程已经创建 `QApplication`。
- `vi_agent_openWindow` 从主 UI 线程调用。
- `vi_agent.dll` 与宿主尽量使用同一 Qt 5.12.12 运行库。
- DLL 边界不传 `QString`、`QWidget*` 等 Qt 类型，只传 C 字符串和 `void* parentHwnd`。
## Qt platform plugin

如果运行时报：

```text
Could not find the Qt platform plugin "windows"
```

说明程序没有找到 `platforms/qwindows.dll`。当前 `scripts\build_vs2022_nmake.bat` 会在构建后自动调用 `windeployqt --release`，将 Qt 运行库和 `platforms/qwindows.dll` 部署到构建目录。正常产物应包含：

```text
build_nmake_qt512/vi_agent_demo.exe
build_nmake_qt512/vi_agent.dll
build_nmake_qt512/platforms/qwindows.dll
```

也可以用 `scripts\run_demo_qt512.bat` 启动，它会显式设置 Qt 插件路径。