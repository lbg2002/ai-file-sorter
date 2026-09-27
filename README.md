# FileSort Guard

A safety-focused community fork of [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter).

FileSort Guard keeps the upstream local/remote LLM clients, review workflow, undo history, and cross-platform Qt application, while changing the interactive AI organization flow to be folder-oriented:

- **One-request folder AI** — list the selected folder once and ask the model to categorize the whole inventory in one structured JSON response.
- **Optional recursion** — the existing subdirectory option controls whether the batch inventory includes nested items.
- **Custom AI models** — use OpenAI, Gemini, built-in local models, custom GGUF models, or any configured OpenAI-compatible endpoint such as vLLM/Ollama/LM Studio.
- **Editable batch prompt** — view, edit, preview, enable/disable, and restore the prompt used for the single folder request.
- **Rule mode** — organize files deterministically without calling an LLM.
- **Safety validation before apply** — compare the final categorization plan with the original filesystem snapshot and highlight anomalies before any move is allowed.

> This repository is an independent modified fork. It is not an official build of AI File Sorter and is not affiliated with or endorsed by the upstream project.

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
| `{{folder_path}}` | Selected folder |
| `{{inventory_json}}` | Complete scanned inventory with stable numeric ids |
| `{{recursive}}` | Whether recursive scanning is enabled |
| `{{item_count}}` | Number of scanned items |
| `{{category_language}}` | Requested category language |
| `{{context}}` | Whitelist and categorization-style constraints |
| `{{output_schema}}` | Required structured JSON response schema |

When the override is disabled, FileSort Guard uses its built-in safe folder-batch prompt.

### 2. Separate AI mode and Rule mode

The main screen now exposes two independent organization modes:

- **AI mode (one request per folder)** — scans the selected folder, sends the whole inventory once to the selected model, parses one JSON response, then opens the normal editable review screen.
- **Rule mode** — does not call an LLM.

The **AI model…** button opens the existing model selector. It supports remote OpenAI/Gemini, custom OpenAI-compatible APIs with a user-defined base URL/model/API key, custom local GGUF models, and the built-in local models.

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

AI mode now returns the whole selected folder as one structured response keyed by numeric item ids. The model never supplies authoritative source paths or destination paths; those are reconstructed by the application from the original scan. FileSort Guard validates the parsed plan against the pre-analysis filesystem snapshot before showing the review dialog, and runs another validation over the user's final selected/edited rows immediately before apply.

The validator detects five conditions:

| Status | Meaning | Review behavior |
| --- | --- | --- |
| **Duplicate source** | The same source item appears more than once in the proposed plan | Highlighted amber; processing blocked |
| **Missing from result** | A scanned item is absent from the proposed plan | Highlighted red; item is kept in place |
| **Unknown source** | A proposed source was not present in the scan snapshot | Highlighted purple; processing blocked |
| **Destination conflict** | Different source items resolve to the same destination path | Highlighted rose; deselected until fixed |
| **Unsafe target** | A category/subcategory contains an empty or path-like component | Highlighted orange; deselected until fixed |

The status is shown as text as well as background color, so the review does not rely on color alone.

Missing items are never interpreted as a delete request. They remain at their original path.

### Safety policy

This fork keeps the existing preview/confirmation flow and adds these constraints:

1. AI output is never the filesystem authority.
2. A missing AI result does not delete a file.
3. The model identifies inputs only by numeric ids; unknown/hallucinated ids are surfaced and cannot silently become filesystem paths.
4. Duplicate source operations are rejected by the final safety gate.
5. Multiple files targeting the same destination are rejected unless the user edits or deselects the conflict.
6. Path-like/unsafe category targets are rejected.
7. Rule mode only moves matched items; unmatched items remain untouched.
8. Existing upstream Undo support remains available after a successful operation.

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
     one inventory / one LLM call          RuleEngine
              |                               |
       structured JSON response          CategorizedFile[]
              |
        BatchFolderCategorizer
              |
       CategorizedFile[]
              |
              v
     ResultIntegrityValidator
              |
      +-------+--------+---------+----------+
      |                |         |          |
   duplicate         missing   unknown   target conflict / unsafe target
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
  BatchFolderCategorizer.hpp
  PromptTemplateStore.hpp
  PromptEditorDialog.hpp
  RuleEngine.hpp
  RuleEditorDialog.hpp
  ResultIntegrityValidator.hpp

app/lib/
  BatchFolderCategorizer.cpp
  PromptTemplateStore.cpp
  PromptEditorDialog.cpp
  RuleEngine.cpp
  RuleEditorDialog.cpp
  ResultIntegrityValidator.cpp

tests/unit/
  test_batch_folder_categorizer.cpp
  test_rule_engine.cpp
  test_result_integrity_validator.cpp
```

The upstream model clients are reused. The interactive GUI orchestration is changed from per-item LLM calls to a folder-batch request, while legacy/headless per-item code is retained for compatibility.

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
