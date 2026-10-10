# FileSort Guard · AI 智能文件整理

> 一款面向 Linux 桌面的轻量级文件整理工具：**先预览、再审核、最后执行；每次操作都可追踪。**
>
> A review-first desktop file organizer with remote LLM APIs, deterministic rules, safe previews, and undo history.

基于 [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter) 的开源分支，重点改进中文使用体验、文件整理安全性、分类一致性和 Qt 原生界面。

[![Remote API Ubuntu DEB](https://github.com/lbg2002/ai-file-sorter/actions/workflows/remote-api-release.yml/badge.svg)](https://github.com/lbg2002/ai-file-sorter/actions/workflows/remote-api-release.yml)

**[下载与发布版本](https://github.com/lbg2002/ai-file-sorter/releases) · [GitHub Actions 构建](https://github.com/lbg2002/ai-file-sorter/actions) · [问题反馈](https://github.com/lbg2002/ai-file-sorter/issues)**

## 核心能力

| 功能 | 实际能力 |
| --- | --- |
| AI 智能分类 | 根据名称、扩展名、路径、已有目录及可用内容摘要推荐类别 |
| 自定义远程 API | 支持 OpenAI 兼容协议端点和应用已有的远程 API 提供商 |
| 规则模式 | 不请求大模型，根据确定性规则生成整理建议 |
| 审核与批量编辑 | 确认前逐项查看、修改类别、子类别与重命名提议 |
| 安全预览 | Dry Run 模式查看计划移动位置，不立即操作文件 |
| 整理结果校验 | 识别重复条目、缺失源、未知源及目标冲突等异常 |
| 撤销与历史 | 保存审核和移动历史，支持撤销最近的整理操作 |
| 目录整体分类 | 非递归模式可将普通文件夹作为整体移动，不拆散内容 |
| 递归文件扫描 | 可整理子目录中的文件；与父目录整体迁移互斥，以避免冲突 |
| 项目目录保护 | 尽量避免拆散 Git、Unity、Blender 等结构化项目目录 |
| 图片/文档文件名 | 完整版本支持本地图像理解和文档内容分析；远程专用版仍支持可用的文档文本处理功能 |
| 音视频元数据 | 根据可读取的媒体标签提出规范的文件命名建议 |
| 个性化分类 | 白名单、历史分类提示、语言设置和自定义 AI 提示词 |
| 原生桌面界面 | Qt6 Widgets，亮色/暗色主题、中文显示、桌面图标及可滚动提示词预览 |

## 工作方式

```text
选择待整理的目录
       │
       ▼
选择 AI 模式（远程 API）或规则模式
       │
       ▼
扫描文件 / 选择整理文件夹
       │
       ▼
构建分类建议及目的路径
       │
       ▼
安全校验与人工审核
       │
       ├── 仅预览：不修改文件
       │
       └── 确认执行：移动并写入操作历史
                             │
                             └── 必要时撤销
```

**整理策略：**优先复用合理的已有文件夹和历史分类，避免把类似文件拆成大量孤立的小目录。新目录不是越多越好；重命名、覆盖或移动之前应当检查每一条建议。

## 两种构建版本

### Remote API Only · 推荐

适用于使用 Kimi / Qwen / DeepSeek 等**提供 OpenAI 兼容 API 的端点**，或者使用程序内置的远程接口的用户。其模型可用性由你的 API 提供商决定。

- 编译选项：`REMOTE_API_ONLY=1`
- 不编译或链接 `llama.cpp`、GGML、MTMD 本地推理组件
- 不下载 CUDA/Vulkan 模型后端，也不需要 GPU
- 隐藏本地 GGUF 模型选择和下载控件
- **不支持本地视觉模型推理**；此模式不应被视为所有本地 AI 功能的等价替代
- 仍需要 Qt6、MediaInfo、JsonCpp、curl 等桌面应用依赖

### Full · 完整版

保留上游本地 GGUF / 图像理解 / CPU / GPU 加速支持，需要正确编译并安装其匹配的 llama.cpp 与运行时库。一般远程 API 用户无需选择此模式。

## Ubuntu 24.04 安装

优先从 [Actions 构建结果](https://github.com/lbg2002/ai-file-sorter/actions/workflows/remote-api-release.yml) 或 [Releases](https://github.com/lbg2002/ai-file-sorter/releases) 下载标明 **remote-ubuntu24-amd64** 的 DEB。

如果文件来自 Actions，请进入成功的构建运行，下载 `filesort-guard-remote-ubuntu24-amd64` artifact 并解压，再执行：

```bash
sudo apt install ./aifilesorter_*_amd64.deb
```

软件包名依然是 `aifilesorter`，新版本会由 APT 覆盖旧版，不需要先手动卸载；安装前建议备份重要用户配置。程序主体安装于 `/opt/aifilesorter`，启动命令是 `aifilesorter`，应用列表名称为 **FileSort Guard**。

### 从源码构建远程专用版

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config python3 patchelf \
  qt6-base-dev qt6-base-dev-tools qt6-l10n-tools qt6-tools-dev-tools \
  libcurl4-gnutls-dev libjsoncpp-dev libsqlite3-dev libssl-dev \
  libfmt-dev libspdlog-dev libmediainfo-dev libzen-dev zlib1g-dev libx11-dev

git clone https://github.com/lbg2002/ai-file-sorter.git
cd ai-file-sorter
make -C app REMOTE_API_ONLY=1 -j2
bash app/scripts/package_deb.sh --remote-api-only 1.9.2+guard.75
```

远程专用版**不需要初始化 llama.cpp 子模块**，也不会因为缺少 `-lllama` / `-lggml` 无法链接；但首次编译仍需 Qt/C++ 常规开发依赖。

## 使用建议与安全说明

1. **先用测试文件夹演练。** 复制 20～50 个普通文件，开启 Dry Run，检查目的目录、冲突警告和审核后的分类。
2. **整体迁移文件夹时关闭递归扫描。** 在整理选项勾选“分类文件夹”，可以把普通顶层文件夹作为不可拆分的整体；复杂软件或代码项目应慎重移动。
3. **真正移动前仔细审核。** AI 的判断可能不准确；程序会做安全校验，但不保证理解每份文件的业务价值。
4. **API 密钥保存在本地配置中。** 选择远程模型意味着与远程提供商交换模型请求所需的文件名、路径或摘要等信息，具体隐私取决于所选服务。
5. **定期备份重要资料。** 撤销功能不是完整的备份系统，不能替代外部备份。

## 外观与交互

采用原生 Qt6 Widgets 重新整理视觉层级：文件目录入口、整理选项、分类模式和文件预览分别分区展示。支持系统主题、云雾浅色和午夜深色，强调圆角卡片、克制的蓝色操作按钮、易读文本和适度留白。文件名过长时表格省略显示，鼠标悬停可查看完整内容；AI 提示词预览支持滚动与关闭按钮。

**设计原则：原生、高效、易读，不以动画或重型 WebView 替代已有 Qt 功能。**

## 贡献与许可证

欢迎通过 [Issues](https://github.com/lbg2002/ai-file-sorter/issues) 提交错误复现和功能建议。分支遵循仓库根目录的 [LICENSE](LICENSE) 开源许可；上游作者、第三方库和许可归属保持不变。

本项目不是 Apple 官方产品；“Apple 风格”仅指界面布局、视觉层级与交互规范的设计参考。
