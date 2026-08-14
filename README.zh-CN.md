<p align="center">
  <img src="docs/logo/logo.png" width="160" alt="Codex Weekly Limit Tray 徽标">
</p>

<p align="center">
  <a href="README.md">English</a> | <a href="README.ko.md">한국어</a> | <a href="README.ja.md">日本語</a> | <b>简体中文</b> | <a href="README.ru.md">Русский</a>
</p>

# Codex Weekly Limit Tray

一个小巧的 Windows 原生应用，在系统托盘中显示你的 Codex 每周用量上限。

托盘图标会显示一个粗体大数字，例如 `73`；在尚未获取到上限信息时显示 `--`。点击图标可打开 Codex 面板，面板中会显示剩余量、重置时间和剩余时长。

<p align="center">
  <img src="docs/screenshot/tray.png" width="220" alt="Windows 系统托盘中显示数字 39 的图标"><br>
  <sub><em>托盘图标</em></sub>
  <br><br>
  <img src="docs/screenshot/panel.png" width="320" alt="显示剩余 39%、用量、重置日期和剩余时长的详情面板"><br>
  <sub><em>详情面板</em></sub>
</p>

## 环境要求

- x64 架构的 Windows 10 或 Windows 11
- 官方 [Codex CLI](https://developers.openai.com/codex/cli/)

若要从源码构建，请安装 MSYS2 UCRT64 的 C++ 编译器。构建脚本会先检查 `%USERPROFILE%\msys64\ucrt64\bin\g++.exe`，然后检查 `C:\msys64\ucrt64\bin\g++.exe`，也可以通过 `CXX` 环境变量提供完整路径。

应用通过 Windows 的按用户 Local AppData 已知文件夹来定位 Codex。常规位置如下：

```text
%LOCALAPPDATA%\Programs\OpenAI\Codex\bin\codex.exe
```

## 运行应用

使用公开发行版时，解压 ZIP 并运行 `CodexWeekUsageTray.exe`。

从源码构建并运行：

```powershell
native\build.cmd
native\out\CodexWeekUsageTray.exe
```

图标位于常规的 Windows 系统托盘中，不会遮挡时钟或日期。

### 首次运行会出现 SmartScreen 警告

发行版 EXE 尚未进行 Authenticode 签名，因此首次运行时会出现 **Windows 已保护你的电脑** 提示。信任该文件之前请先校验：

```powershell
Get-FileHash .\CodexWeekUsageTray.exe -Algorithm SHA256
```

把结果与同一版本 `SHA256SUMS` 文件中对应的一行进行比对。如果一致，选择 **更多信息 > 仍要运行**。如果你完全不想运行未签名的可执行文件，请按上面的步骤从源码构建。

### 让托盘图标可见

1. 运行 `CodexWeekUsageTray.exe`。
2. 打开 **设置 > 个性化 > 任务栏 > 其他系统托盘图标**。
3. 找到 **CodexWeekUsageTray** 并将其打开（**开**）。

这样 Windows 就会把图标显示在系统托盘中，而不是隐藏在溢出列表里。

## 使用面板

- **左键点击托盘图标**：打开或隐藏详情面板。
- **右键点击托盘图标** 后选择 **Show panel**：从托盘菜单打开详情面板。
- **Refresh**：立即检查用量上限。
- **Close**：仅隐藏面板，托盘应用仍在运行。
- **Sign in**：在需要时用默认浏览器打开官方 Codex ChatGPT 登录页面。
- **Check**：登录后再次检查账户。

当 Codex 推送上限更新时，应用会立即刷新。作为兜底，它还会以最短 5 秒一次的频率进行检查。

## 构建检查

```powershell
native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
native\out\CodexWeekUsageTray.exe --self-test
```

## 移除已保存的托盘项

如果测试后在 **其他系统托盘图标** 中残留了旧条目，请双击与 `CodexWeekUsageTray.exe` 位于同一文件夹的 `uninstall.cmd`。清理会立即开始，无需输入任何词语，也没有确认提示。

`uninstall.cmd` 会停止匹配的 `CodexWeekUsageTray.exe` 进程，并且只移除它们在当前用户下的托盘设置。在替换旧版本之前，或者残留了重复的托盘条目时，这很有用。它不会删除 EXE 文件，也不会更改其他应用的托盘设置。

你也可以在发行版文件夹中打开终端运行：

```powershell
.\CodexWeekUsageTray.exe --uninstall-dry-run
.\CodexWeekUsageTray.exe --uninstall
```

试运行（dry run）只会列出完全匹配的 `CodexWeekUsageTray.exe` 条目，不做任何更改。

## 安全与隐私

- 应用不会读取、打印、保存或上传你的密码、API 密钥、刷新令牌或用量历史。
- 登录 URL 必须使用 HTTPS，且主机名严格为 `chatgpt.com` 或 `auth.openai.com`，默认浏览器才会打开它。
- 原生宿主没有更新器、下载器、开机启动注册项，也没有应用自有的网络客户端。账户相关流量由 Codex CLI 通过本地 stdio 连接自行处理。
- 发行版 EXE 尚未进行 Authenticode 签名。运行下载的文件前，请先核对已公布的 SHA-256 值。

v2.0.1 及之后的版本包含 EXE、`uninstall.cmd` 以及配套的 `SHA256SUMS` 清单。压缩包中不含 .NET 运行时、DLL 或 PDB。v2.0.0 仅包含 EXE。

应用仅在内存中保存当前的用量上限。Windows 可能会保留常规的通知图标设置，Codex CLI 会保留自己的会话数据。
