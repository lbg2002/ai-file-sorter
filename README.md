<div align="center">

# 🗂️ FileSort Lite

**小而安全的 AI 文件整理器 · 一次规划 / 逐项校验 / 随时撤销**

*API-only · Linux / Windows · No local models · No heavyweight ML runtime*

</div>

> 这是基于原 [ai-file-sorter](https://github.com/hyperfield/ai-file-sorter) 重新构建的轻量分支。保留仓库历史和原项目许可，移除原有大型本地模型、CUDA、PDFium、媒体识别、插件下载及复杂构建依赖。**该版本仅将文件名、类型、大小和现有文件夹名发送至配置的 API，不读取/上传文件内容。**

## 工作流程

```text
选文件夹（默认只选顶层文件）
          ↓
完整文件清单 ─────→ 一次 OpenAI 兼容 API 请求
          ↓
模型仅输出 ID → 分类文件夹 的 JSON 方案
          ↓
校验：遗漏 / 重复 / 虚构 / 非法路径 / 冲突
          ↓
可视化预览、双击修改分类、选择不移动
          ↓
人工确认 → 安全移动 → 写入操作日志
          ↓
左侧历史 → 选择记录 → 撤销（不会覆盖已有文件）
```

## 安装与运行

**源码方式：** 只需 Python 3.10+，没有任何 pip 依赖。

Linux (Ubuntu/Debian):

```bash
sudo apt install python3 python3-tk
python3 -m filesort_lite
```

Windows：安装 Python（需包含 Tcl/Tk），在仓库目录运行：

```powershell
py -m filesort_lite
```

**独立发行包：** GitHub Actions 在 Linux 和 Windows 上测试后使用 PyInstaller 构建独立程序，无需用户安装 Python；从该分支的 Actions Artifacts 下载。程序体积以实际构建产物为准，目标远小于旧版 100+ MB，但不能承诺固定大小。

## 使用方法

1. 选择本地文件夹；勾选“包含顶层文件夹”才会把文件夹作为整体移动。软件默认不递归扫描，也不移动原有文件夹。
2. 设置 OpenAI 兼容的 `https://…/v1` API 地址、模型名与密钥。也支持完整 `/chat/completions` URL。本机 HTTP 地址仅允许 localhost。
3. 点 **AI 一次规划**。全部文件清单一次发送，不会为每个文件单独请求；最多 600 项、60,000 字符，超限会直接提示，不会静默拆批。
4. 双击预览表中的项目修改目标分类，留空代表保持原位。**异常校验必须解决才能移动**。
5. 选择 **确认移动**，之后可在左侧历史中选择对应记录并撤销。

## 安全设计

- 文件清单是唯一可信源，模型只输出分类建议，不接触本地文件系统操作。
- 只允许单层分类目录；拒绝 `..`、斜杠、Windows 受限字符、系统保留名称和符号链接目标。
- 移动前重新比对扫描时文件元信息；若源文件被改动，则中止并保留日志。
- 目标已存在时拒绝覆盖；所有移动都有事务式 JSON 日志（操作前先持久化意图），支持异常中断后的恢复。
- 撤销时核查文件是否改变；如果路径冲突或内容已更新，跳过并报告，不强制覆盖。
- 操作日志保存在 Windows `%LOCALAPPDATA%/FileSortLite/history` 或 Linux `~/.local/state/FileSortLite/history`（可由 `XDG_STATE_HOME` 覆盖），不放在整理目录里。
- API 密钥不会保存到配置文件或日志里；可使用自己的代理与自定义兼容接口。

**已知边界：** 本版按顶层项目整理，不对文件正文、图像内容进行本地分析，不支持按目录内部文件递归分类。网络服务看到文件名、类型、大小和现有文件夹名，请勿对敏感目录发送请求。极端并发文件系统变更、恶意程序造成的路径竞态不属于该版本的安全保证。建议先在备份副本上试用。

## 开发及测试

```bash
python3 -m unittest discover -s tests -v
```

纯 Python 标准库核心，图形界面 Tkinter；支持 Windows 和 Linux。MIT/BSD 第三方运行库并非必要；仓库沿用原 AGPL-3.0 许可证（见 `LICENSE`）。感谢 [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter) 原作者的工作。