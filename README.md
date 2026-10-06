# faceRecognition

这是一个基于 Win32、MFC 和 Video for Windows 的摄像头人脸处理实验项目。程序由一个主程序、两个公共 DLL 和五个按顺序执行的插件组成。

本仓库保留原有 Visual Studio 工程，同时提供 CMake 命令行构建。使用 CMake 时仍然调用 MSVC、Windows SDK 和 MFC，但不需要打开 Visual Studio 图形界面。

## 项目组成

| CMake 目标 | 生成文件 | 用途 |
|---|---|---|
| `VFWCAPTURE` | `VFWCAPTURE.exe` | 摄像头采集和插件宿主 |
| `ImageProc` | `ImageProc.dll` | 公共图像处理函数 |
| `TraceFeature` | `TraceFeature.dll` | 特征处理函数 |
| `ImagePrepare` | `PA_ImagePrepare.dll` | 图像复制和降采样 |
| `FaceLocator` | `PB_FaceLocator.dll` | 肤色分割和人脸定位 |
| `TraceObject` | `PC_TraceObject.dll` | 目标跟踪 |
| `BlinkEyeCheck` | `PD_BlinkEyeCheck.dll` | 眨眼检测 |
| `FinalProc` | `PZ_FinalProc.dll` | 最终处理 |

## 构建要求

必须安装：

- Windows 10/11；
- CMake 3.24 或更高版本；
- MSVC v145 x86 编译工具；
- Windows SDK；
- 对应 MSVC 工具集的 MFC/ATL 组件。

本项目只支持 **Win32/x86**，不能使用 x64 或 MinGW。CMake 配置发现错误架构或编译器时会主动停止。

当前预设使用 `Visual Studio 18 2026` 生成器。它只是 CMake 调用 MSBuild 的后端，不会打开 Visual Studio 窗口。

可以直接在普通 `cmd`、PowerShell 或 VS Code 终端中运行下面的命令。若 CMake 无法发现 MSVC，可先初始化开发者环境：

```bat
call "D:\program\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
```

## 首次配置

在仓库根目录执行：

```bat
cmake --preset win32
```

需要丢弃旧的 CMake 缓存并重新配置时：

```bat
cmake --preset win32 --fresh
```

配置文件生成在 `build/`，不会修改原有 `.sln` 和 `.vcxproj`。

## 编译整个项目

### Debug

```bat
cmake --build --preset debug
```

### Release

```bat
cmake --build --preset release
```

如果希望使用更多并行任务，可以追加：

```bat
cmake --build --preset debug --parallel
```

## 单独编译主程序和公共 DLL

以下示例使用 Debug。若要构建 Release，把 `--preset debug` 改成 `--preset release`。

只编译图像处理 DLL：

```bat
cmake --build --preset debug --target ImageProc
```

只编译特征处理 DLL：

```bat
cmake --build --preset debug --target TraceFeature
```

只编译主程序：

```bat
cmake --build --preset debug --target VFWCAPTURE
```

CMake 会自动先编译该目标所依赖的其他目标。例如构建 `VFWCAPTURE` 时会先生成 `ImageProc`，不需要手动复制或链接仓库根目录中的旧 `.lib` 文件。

## 单独编译各个插件

图像预处理插件：

```bat
cmake --build --preset debug --target ImagePrepare
```

人脸定位插件：

```bat
cmake --build --preset debug --target FaceLocator
```

目标跟踪插件：

```bat
cmake --build --preset debug --target TraceObject
```

眨眼检测插件：

```bat
cmake --build --preset debug --target BlinkEyeCheck
```

最终处理插件：

```bat
cmake --build --preset debug --target FinalProc
```

单独构建插件时，其依赖会自动构建。例如 `ImagePrepare` 会自动构建 `ImageProc` 和 `TraceFeature`。

> CMake target 使用容易输入的项目名；最终生成的插件 DLL 仍带有 `PA_`、`PB_` 等顺序前缀。

## 生成文件位置

Debug 完整构建后目录如下：

```text
build/
└── bin/
    └── Debug/
        ├── VFWCAPTURE.exe
        ├── ImageProc.dll
        ├── TraceFeature.dll
        └── Plugin/
            ├── PA_ImagePrepare.dll
            ├── PB_FaceLocator.dll
            ├── PC_TraceObject.dll
            ├── PD_BlinkEyeCheck.dll
            └── PZ_FinalProc.dll
```

Release 使用同样的结构，只是目录名变为：

```text
build/bin/Release/
```

链接阶段生成的 import library 位于：

```text
build/lib/Debug/
build/lib/Release/
```

不要把 Debug EXE 与 Release DLL 混合使用。

## 运行整个项目

先构建完整项目：

```bat
cmake --build --preset debug
```

然后从仓库根目录启动：

```bat
build\bin\Debug\VFWCAPTURE.exe
```

PowerShell 也可以使用：

```powershell
& .\build\bin\Debug\VFWCAPTURE.exe
```

Release 运行命令：

```bat
build\bin\Release\VFWCAPTURE.exe
```

程序以 EXE 所在目录为基准查找：

```text
Plugin\*.dll
```

因此不要单独移动 EXE，也不要只复制某一个插件。`ImageProc.dll`、`TraceFeature.dll` 必须与 EXE 位于同一目录，五个插件必须位于其 `Plugin` 子目录。

程序启动后，插件默认未启用，需要在界面插件列表中选择并启用。插件期望处理顺序由文件名前缀表达：

```text
PA → PB → PC → PD → PZ
```

运行摄像头功能还需要系统中存在可由 Video for Windows 访问的摄像头或采集设备。

## 清理构建结果

清理当前配置的产物：

```bat
cmake --build --preset debug --target clean
```

完全重新配置，可以删除 `build/`，或者直接运行：

```bat
cmake --preset win32 --fresh
```

## 常见问题

### CMake 找不到 Visual Studio 或 v145

先运行开发者环境初始化命令：

```bat
call "D:\program\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
```

随后重新执行：

```bat
cmake --preset win32 --fresh
```

也请确认 Visual Studio Installer 中已经安装：

- x86/x64 C++ build tools；
- v145 toolset；
- Windows SDK；
- C++ MFC/ATL 组件。

### 提示必须使用 Win32

不要手工使用 `-A x64`。预设已经固定为：

```text
Win32 + v145
```

### 程序找不到 DLL

确认运行的是：

```text
build/bin/Debug/VFWCAPTURE.exe
```

而不是旧的：

```text
Debug/VFWCAPTURE.exe
```

并确认 EXE 同目录存在 `ImageProc.dll` 和 `TraceFeature.dll`。

### 程序没有发现插件

确认目录名称及布局为：

```text
build/bin/Debug/Plugin/*.dll
```

不要把插件 DLL 放到 EXE 同级目录。

### 修改 CMakeLists 后没有生效

重新配置并构建：

```bat
cmake --preset win32 --fresh
cmake --build --preset debug
```

## 原 Visual Studio 工程

原有 [VFWCAPTURE.sln](VFWCAPTURE.sln) 和各个 `.vcxproj` 仍然保留。CMake 是独立的命令行构建入口，两种构建方式的输出目录不同。为避免加载旧二进制，使用哪种方式构建，就从对应方式的输出目录运行程序。
