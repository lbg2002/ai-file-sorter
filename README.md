# FileSort Guard

A safety-focused community fork of [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter).

FileSort Guard keeps the upstream AI categorization, document/image analysis, local and remote LLM support, review workflow, undo history, and cross-platform Qt application, while adding two deliberately simple workflows:

- **Editable AI prompts** — view, edit, preview, enable/disable, and restore the categorization system prompt.
- **Rule mode** — organize files deterministically without calling an LLM.
- **Safety validation before apply** — compare the final categorization plan with the scanned filesystem snapshot and highlight anomalies before any move is allowed.

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
