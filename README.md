# AI File Sorter Lite — Original UI Parity (Rust)

> **开发中的忠实复刻分支，尚未达到完整版功能等价。**

本分支针对 [lbg2002/ai-file-sorter](https://github.com/lbg2002/ai-file-sorter) 原 Qt 桌面产品（包含 feature/modern-review-smart-folders-20261010 的交互方向）进行界面与操作流程复刻。目标：保留用户熟悉的完整桌面工作流，仅删除本地 GGUF 模型下载、llama.cpp、CUDA/Vulkan 推理及相关重型依赖。Rust 负责目录扫描、远程 API 批量规划、文件移动/重命名与日志撤销；Tauri 2 + 系统 WebView 负责 UI。目前是原界面复刻，**不是直接复用原 Qt 源码，也不保证像素级一致**。

## 与原完整版对齐的操作流程

- **主窗口**：原版风格菜单栏、左侧文件资源管理器（导航、展开与拖动宽度）、文件夹路径、文件/目录独立勾选、AI/规则模式、精细/一致分类、白名单、分析按钮与原始文件列表。
- **审核与确认**：独立审核窗口，八列分别是处理、文件、类型、建议文件名、分类、子分类、校验状态和目标路径；逐项选择、编辑、高亮批量修改、预演、稍后继续、模型异常修复与确认执行。
- **设置**：OpenAI 兼容 API 地址、模型名称与列表、会话级 API Key、提示词、规则、白名单、深浅主题。规则支持扩展名简写、名称/路径/文件大小/类型字段与正则表达式。
- **本地文件预览**：主资源管理器和审核窗口均可打开只读文本与图片预览；对图片有 8 MB 限制，对文本仅读取前 128 KiB，不上传正文。
- **整批 AI**：一次扫描后将所有清单项（名称、类型、大小、已有文件夹）统一打包，单次请求 API 得到 JSON 分类/建议文件名。不会逐文件重复请求。
- **本地校验与撤销**：重复、遗漏、幻觉、非法路径、目标冲突、源修改检查；Linux 使用 renameat2(RENAME_NOREPLACE)，Windows 使用 MoveFileW；操作前持久化日志，两级目录与文件名改动也可撤销。

## 尚未与原完整版对齐

- **递归扫描现已支持**（深度 ≤8 层、单批 ≤600 项），递归整理文件与顶层文件夹整体移动互斥，撤销保留原始相对路径。结构化工程文件夹的自动识别与保护策略仍需继续加强。
- 图像内容分析、文档正文提取的远程 API 模式尚未移植，相关选项禁用。
- 音视频元数据改名、完整文件预览缩略图、原版长期学习数据库、完整多语言本地化尚未移植。
- 白名单现为自定义分类目录列表，尚未达到原版树状智能白名单行为。
- 需要真实 Linux/Windows 截图、键盘交互测试和稳定性回归，不能视为最终完成。

**这些是明确的未完成事项，不会用无作用的按钮伪装已经实现。请先在测试文件夹中验证。**

## 构建

[GitHub Actions](https://github.com/lbg2002/ai-file-sorter/actions) 可查看 original-ui-parity 分支上的 Linux DEB/AppImage 与 Windows NSIS 自动构建。只有工作流真正成功，下载项才可视为构建产物。

Ubuntu 22.04+ / 26.04 环境：

    sudo apt install libwebkit2gtk-4.1-dev build-essential \
      libxdo-dev libssl-dev libayatana-appindicator3-dev librsvg2-dev patchelf
    cd src-tauri
    # 使用 npm 预编译 Tauri CLI，省去编译 CLI 自身
    npx --yes @tauri-apps/cli@2 icon icons/source.svg
    cargo test
    cargo run

Windows 需要 Rust stable、MSVC C++ Build Tools、WebView2；CI 使用 Node 22 的预编译 Tauri CLI。

**测试建议**：临时目录 → 文件/目录勾选 → 规则/AI 规划 → 编辑子类与文件名 → 预演 → 执行 → 退出重启 → 撤销 → 故意制造冲突检测。

本项目沿用 [AGPL-3.0](LICENSE)。感谢原作者 [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter)。
