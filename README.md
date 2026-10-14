# FileSort Lite · Rust / Tauri 2

**轻量 API-only 文件整理器**：一次规划整个文件夹、人工审查、严格校验、操作记录与安全撤销。Rust 文件操作核心 + Tauri 2 原生 WebView 界面，不再依赖 Python、llama.cpp、本地模型、CUDA、PDFium 或多媒体模型。

> 此版本是独立重写的 **测试分支**，尚未替换主分支。设计思路来源于原 [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter)；继续遵循 AGPL-3.0 协议。建议先拿可丢弃的测试文件夹体验。

## 新版界面

左侧导航分为整理工作台、操作历史和设置。整理工作台包含文件夹扫描、整批 AI 与本地规则模式、可搜索和批量编辑的文件预览、目标分类汇总、安全检查状态。设置支持 API Base URL、模型列表拉取、模型名、临时 API Key、自定义提示词、规则文本、主题和文件夹移动偏好。

## 工作流程

1. 选择文件夹。默认只扫描顶层普通文件（可手动启用顶层文件夹整体移动）。
2. **AI 整批规划**：将全部文件名/扩展名、大小、类型及现有一级文件夹名一次性发送给 OpenAI 兼容 API；或选择 **本地规则规划**，无需网络。
3. 逐项审查预览，直接修改分类、搜索、筛选及批量设置；可将项目明确设为保留原位。
4. 检查模型遗漏、重复、幻觉 ID、非法目录、目标冲突及源文件变化。异常未排除前不允许执行。
5. 人工确认后移动。对每步移动先写入日志，再以“不覆盖”方式重命名，单步执行成功才标记完成。
6. 历史页查看每次移动清单或逆序撤销。已变化、冲突项目不会被强行覆盖。

## 获取安装包

[GitHub Actions](https://github.com/lbg2002/ai-file-sorter/actions) → Rust Desktop 工作流 → 选成功的运行 → Artifacts：

- Ubuntu / Debian：DEB、AppImage
- Windows：NSIS EXE 安装器

安装包体积和 CI 成功状态需以真实构建结果为准。首次 Rust 编译会下载大量构建依赖，**构建缓存大小 ≠ 用户安装包体积**。

## 本地开发

先安装 Rust stable。Ubuntu 等发行版需 Tauri 2 系统依赖：

    sudo apt install libwebkit2gtk-4.1-dev build-essential \
      libxdo-dev libssl-dev libayatana-appindicator3-dev librsvg2-dev patchelf

然后执行：

    cd src-tauri
    cargo install tauri-cli --version '^2'
    cargo tauri icon icons/source.svg
    cargo test
    cargo run

需要独立包：

    cargo install tauri-cli --version '^2'
    cargo tauri build --bundles deb,appimage

Windows 需 Rust stable、MSVC Build Tools、WebView2；安装 Tauri CLI 后运行 cargo tauri build --bundles nsis。

## 安全边界

- 仅发送清单元数据，不解析或上传文件正文；请勿对敏感文件名使用不可信 API。
- 600 个顶层项目 / 60,000 字符的单次上限，超过则明确拒绝，不静默截断。
- 远程接口必须 HTTPS；localhost 允许 HTTP。API Key 仅保留在进程内存，不写入设置和历史。
- Linux 使用 renameat2(RENAME_NOREPLACE)；Windows 使用 MoveFileW，避免覆盖现有目标文件。
- 历史记录保存在用户本地应用数据目录下 FileSortLite/history；写入操作意图以支持意外中断后恢复。
- **撤销不是备份**。文件被编辑、外部程序并发操作、跨设备移动、权限变化等情形可能无法完整恢复。整理重要数据前请另行备份。

采用 [AGPL-3.0](LICENSE) 开源许可。
