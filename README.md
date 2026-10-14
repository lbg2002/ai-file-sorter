<div align="center">

# 🗂️ FileSort Guard

### AI 文件整理 + 可视化规则分类 + 操作前安全审查

*An AI-powered and rule-based file organizer with safety-first move validation.*

[![Build](https://img.shields.io/badge/Linux%20Build-GitHub%20Actions-658cce)](https://github.com/lbg2002/ai-file-sorter/actions)
![Technology](https://img.shields.io/badge/C%2B%2B20-Qt6-4d8ca4)
![Modes](https://img.shields.io/badge/Modes-AI%20%7C%20Rules-9978c1)
![Safety](https://img.shields.io/badge/Review-Before%20Moving-55a189)
![License](https://img.shields.io/badge/License-AGPL--3.0-7e8d9c)

**[⬇ 下载 Linux 预览构建](https://github.com/lbg2002/ai-file-sorter/actions/runs/38014638941)** · [🧭 快速了解](#-它能做什么) · [🛡️ 安全机制](#3-safety-validation-and-visual-anomaly-preview) · [🔧 从源码构建](#build-from-source)

</div>

> **社区 Fork，非上游官方安装包。** 本仓库基于 [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter) 二次开发，专注文件移动前的可检查性与安全性。**本仓库目前没有正式的 GitHub Release 安装包**，公开可下载的 Linux 构建在 GitHub Actions → Artifacts，通常需要登录 GitHub，且构建产物可能过期。其他系统请按源码说明自行构建并测试。

## ✨ 它能做什么？

| 🤖 AI 智能分类 | 📐 无模型规则分类 | 🛡️ 移动前审查 |
| :--- | :--- | :--- |
| 使用上游 AI / 本地 LLM 分类能力；系统提示词可自行编辑与预览 | 根据文件扩展名、文件名、路径、大小执行确定性规则，无需 LLM | 对照真实扫描结果，检测重复源、遗漏文件、未知源和目标冲突 |
| **适合多类型杂乱文件** | **适合严格可控的资料归档** | **适合重要文档的安全整理** |

**典型场景：** 下载文件夹、论文资料、图片素材、代码项目归档。程序会先给出分类方案与异常提示，**实际移动文件前必须经过检查和确认**；不匹配规则的文件留在原位。

## 📦 预览构建下载

**[→ GitHub Actions · Build（Linux 构建产物）](https://github.com/lbg2002/ai-file-sorter/actions/runs/38014638941)**

| Linux 发行版 | Artifact 名称 |
| :--- | :--- |
| Ubuntu LTS | `aifilesorter-linux-ubuntu-lts` |
| Debian Stable | `aifilesorter-linux-debian-stable` |
| Fedora | `aifilesorter-linux-fedora-latest` |
| Rocky Linux | `aifilesorter-linux-rocky-latest` |
| Arch Linux | `aifilesorter-linux-arch-latest` |

打开运行页面底部的 **Artifacts** 下载对应压缩包；解压后按包内文件类型及本机系统依赖安装或运行。构建包包含上游图形应用及相关依赖，**与轻量纯 Rust 小工具不同，包体积可能较大**。请先在备份或测试目录体验，不要直接对唯一原始资料执行批量移动。

### 🧭 推荐操作流程

```text
选择文件夹 → 选择 AI / 规则模式 → 生成分类方案
                ↓
        与真实文件清单交叉校验
                ↓
      查看异常 → 人工确认 → 移动
                              ↳ 已有 Undo 支持
```

## Why this fork

AI-assisted file organization is useful, but the model output should never be treated as the source of truth for the filesystem.

The core rule in this fork is:

> **The filesystem scan is the fact; AI output is only a proposal.**

Before the review plan is applied, FileSort Guard checks the proposal against the items that were actually scanned.

## Added in this fork

### 1. Visual prompt editor

Open **Settings → Edit AI Prompt…**.

The editor lets you:

- enable or disable the custom prompt override;
- edit the categorization system prompt;
- restore the default fork template;
- preview a rendered prompt before saving.

Supported variables:

| Variable | Meaning |
| --- | --- |
| `{{filename}}` | Current file or directory name |
| `{{path}}` | Path/context sent to categorization |
| `{{item_type}}` | `file` or `directory` |
| `{{context}}` | Whitelist / consistency context |
| `{{output_format}}` | Required category response format |

When the override is disabled, the original upstream prompts are used.

The override is applied to:

- OpenAI / OpenAI-compatible categorization;
- Gemini categorization;
- local LLM categorization.

### 2. Separate AI mode and Rule mode

The main screen now exposes two independent organization modes:

- **AI mode** — reuses the existing upstream AI pipeline.
- **Rule mode** — does not call an LLM.

This fork intentionally does **not** add embeddings, vector databases, hybrid routing, or automatic AI fallback.

Open **Settings → Manage File Rules…** to edit rules.

The first matching rule wins.

Supported fields:

- `extension`
- `filename`
- `path`
- `size`

Supported operators include:

- `equals`
- `contains`
- `regex`
- `startswith`
- `endswith`
- `>` / `<` for file size

Example:

```text
extension | equals   | .pdf     -> Documents / PDF
filename  | contains | meeting  -> Meetings / Notes
filename  | regex    | ^paper_  -> Research / Papers
```

Files that do not match a rule are left untouched.

### 3. Safety validation and visual anomaly preview

The upstream AI pipeline categorizes items individually and normalizes the result into structured `CategorizedFile` records. FileSort Guard validates that final structured plan against a fresh filesystem snapshot before showing the review dialog.

The validator detects four conditions:

| Status | Meaning | Review behavior |
| --- | --- | --- |
| **Duplicate source** | The same source item appears more than once in the proposed plan | Highlighted amber; processing blocked |
| **Missing from result** | A scanned item is absent from the proposed plan | Highlighted red; item is kept in place |
| **Unknown source** | A proposed source was not present in the scan snapshot | Highlighted purple; processing blocked |
| **Destination conflict** | Different source items resolve to the same destination path | Highlighted rose; processing blocked |

The status is shown as text as well as background color, so the review does not rely on color alone.

Missing items are never interpreted as a delete request. They remain at their original path.

### Safety policy

This fork keeps the existing preview/confirmation flow and adds these constraints:

1. AI output is never the filesystem authority.
2. A missing AI result does not delete a file.
3. Unknown/hallucinated source paths are ignored and block processing.
4. Duplicate source operations block processing.
5. Multiple files targeting the same destination block processing.
6. Rule mode only moves matched items; unmatched items remain untouched.
7. Existing upstream Undo support remains available after a successful operation.

## Architecture of the fork additions

```text
                         Selected folder
                              |
                              v
                        Filesystem scan
                              |
              +---------------+---------------+
              |                               |
              v                               v
          AI mode                         Rule mode
              |                               |
       upstream AI pipeline               RuleEngine
              |                               |
       CategorizedFile[]                 CategorizedFile[]
              |
              v
     ResultIntegrityValidator
              |
      +-------+--------+---------+----------+
      |                |         |          |
   duplicate         missing   unknown   target conflict
      |                |         |          |
      +----------------+---------+----------+
                       |
                       v
               Visual review table
                       |
                explicit approval
                       |
                       v
                existing move/undo
```

## Fork-specific source files

```text
app/include/
  PromptTemplateStore.hpp
  PromptEditorDialog.hpp
  RuleEngine.hpp
  RuleEditorDialog.hpp
  ResultIntegrityValidator.hpp

app/lib/
  PromptTemplateStore.cpp
  PromptEditorDialog.cpp
  RuleEngine.cpp
  RuleEditorDialog.cpp
  ResultIntegrityValidator.cpp

tests/unit/
  test_rule_engine.cpp
  test_result_integrity_validator.cpp
```

The existing upstream AI implementation is intentionally reused rather than rewritten.

## Build from source

The application remains a **C++20 + Qt 6 + CMake** project.

Clone this fork:

```bash
git clone https://github.com/lbg2002/ai-file-sorter.git
cd ai-file-sorter
git submodule update --init --recursive
```

Then follow the platform-specific dependency and build guidance already present in the repository documentation and `app/CMakeLists.txt`.

A typical CMake flow is:

```bash
cmake -S app -B build
cmake --build build --config Release
```

To enable unit tests:

```bash
cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Some platforms require additional Qt/vcpkg/PDFium/media dependencies; see the existing build scripts and documentation in this repository.

## Data files added by this fork

The fork stores its lightweight additions in the existing application data directory:

- `prompt_override.txt` — prompt override state and text;
- `file_rules.tsv` — user-defined deterministic rules.

No vector database or embedding store is introduced.

## Development scope

This fork intentionally stays small.

### In scope

- prompt visibility/editing;
- deterministic rule mode;
- visual safety review;
- anomaly detection;
- unit tests for rules and validation.

### Out of scope

- embedding-based routing;
- RAG;
- vector databases;
- multi-agent workflows;
- automatic deletion;
- AI/rule hybrid mode.

## Upstream

Original project:

- Repository: [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter)
- Architecture: [upstream docs/architecture.md](https://github.com/hyperfield/ai-file-sorter/blob/main/docs/architecture.md)

This fork preserves the upstream copyright notices and license.

## License and trademarks

The source code is distributed under the **GNU Affero General Public License v3.0 (AGPL-3.0)**, consistent with the upstream project.

The upstream project also has a separate trademark policy. The name **FileSort Guard** is used here to distinguish this modified fork from the official upstream application. Upstream names, logos, icons, and other brand assets remain the property of their respective owners and should not be interpreted as branding or endorsement of this fork.

See:

- `LICENSE`
- `TRADEMARKS.md`

## Credits

FileSort Guard is built on the work of the contributors to [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter).

The fork focuses on making AI-driven file operations easier to inspect, customize, and verify before they touch the filesystem.
