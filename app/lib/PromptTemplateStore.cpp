#include "PromptTemplateStore.hpp"

#include "Utils.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace {

std::mutex g_mutex;
bool g_enabled{false};
std::string g_template;
fs::path g_path;

void replace_all(std::string& text, const std::string& needle, const std::string& replacement)
{
    if (needle.empty()) {
        return;
    }
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        text.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
}

} // namespace

void PromptTemplateStore::initialize(const std::string& data_dir)
{
    std::scoped_lock lock(g_mutex);
    g_path = Utils::utf8_to_path(data_dir) / "prompt_override.txt";
    g_enabled = false;
    g_template.clear();

    std::ifstream input(g_path, std::ios::binary);
    if (!input) {
        return;
    }

    std::string first_line;
    std::getline(input, first_line);
    g_enabled = first_line == "enabled=1";

    std::ostringstream rest;
    rest << input.rdbuf();
    g_template = rest.str();
    if (!g_template.empty() && g_template.back() == '\n') {
        g_template.pop_back();
    }
}

bool PromptTemplateStore::enabled()
{
    std::scoped_lock lock(g_mutex);
    return g_enabled;
}

void PromptTemplateStore::set_enabled(bool enabled)
{
    std::scoped_lock lock(g_mutex);
    g_enabled = enabled;
}

std::string PromptTemplateStore::prompt_template()
{
    std::scoped_lock lock(g_mutex);
    return g_template.empty() ? default_template() : g_template;
}

void PromptTemplateStore::set_prompt_template(std::string value)
{
    std::scoped_lock lock(g_mutex);
    g_template = std::move(value);
}

std::string PromptTemplateStore::default_template()
{
    return
        "You are organizing one selected filesystem folder in a single batch.\n"
        "Treat the supplied inventory as the complete source of truth.\n"
        "Folder: {{folder_path}}\n"
        "Recursive scan: {{recursive}}\n"
        "Items: {{item_count}}\n"
        "Requested category language: {{category_language}}\n\n"
        "Categorization constraints:\n{{context}}\n"
        "Inventory JSON (the numeric id is the only source identifier you may return):\n"
        "{{inventory_json}}\n\n"
        "Return JSON only, with exactly this shape:\n{{output_schema}}\n"
        "Rules:\n"
        "1. Return every input id exactly once. Never invent an id and never omit an id.\n"
        "2. Do not return source paths, destination paths, delete operations, overwrite operations, or shell commands.\n"
        "3. category and subcategory are directory labels, not paths. Do not use /, \\, . or .. as path components.\n"
        "4. suggested_name is optional. If used, it must be a filename only, never a path.\n"
        "5. Base categorization on the whole folder context so related files use coherent categories.\n"
        "6. If no whitelist forbids it and no suitable destination folder exists, you may propose a new category/subcategory label. "
        "The application will create that folder only after the user reviews and approves the plan.\n"
        "7. Do not wrap the JSON in Markdown fences or add explanation.";
}

bool PromptTemplateStore::save()
{
    std::scoped_lock lock(g_mutex);
    if (g_path.empty()) {
        return false;
    }
    std::error_code ec;
    fs::create_directories(g_path.parent_path(), ec);
    std::ofstream output(g_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output << (g_enabled ? "enabled=1\n" : "enabled=0\n");
    output << (g_template.empty() ? default_template() : g_template) << "\n";
    return static_cast<bool>(output);
}

std::string PromptTemplateStore::render_or_default(std::string_view original_prompt,
                                                   const std::string&,
                                                   const std::string&,
                                                   FileType,
                                                   const std::string&)
{
    // The editable prompt is now the folder-batch prompt. Keep the legacy
    // per-item pipeline untouched for headless/backward-compatible workflows.
    return std::string(original_prompt);
}

std::string PromptTemplateStore::render_batch(const std::string& folder_path,
                                              const std::string& inventory_json,
                                              bool recursive,
                                              const std::string& category_language,
                                              const std::string& context,
                                              std::size_t item_count)
{
    std::scoped_lock lock(g_mutex);
    std::string rendered = (g_enabled && !g_template.empty())
        ? g_template
        : default_template();

    // Keep the inventory and machine-readable contract mandatory even when a
    // user has an older/custom prompt that omits the new batch variables.
    if (rendered.find("{{inventory_json}}") == std::string::npos) {
        rendered += "\n\nComplete folder inventory (mandatory source of truth):\n{{inventory_json}}";
    }
    if (rendered.find("{{output_schema}}") == std::string::npos) {
        rendered += "\n\nReturn JSON only using this schema:\n{{output_schema}}";
    }
    rendered +=
        "\n\nMandatory safety contract: use only input numeric ids; return each selected id at most once; "
        "never invent source paths, destination paths, delete operations, overwrite operations, or shell commands.";

    replace_all(rendered, "{{folder_path}}", folder_path);
    replace_all(rendered, "{{inventory_json}}", inventory_json);
    replace_all(rendered, "{{recursive}}", recursive ? "true" : "false");
    replace_all(rendered, "{{category_language}}", category_language);
    replace_all(rendered, "{{context}}", context);
    replace_all(rendered, "{{item_count}}", std::to_string(item_count));
    replace_all(rendered,
                "{{output_schema}}",
                "{\"items\":[{\"id\":0,\"category\":\"Documents\","
                "\"subcategory\":\"Reports\",\"suggested_name\":\"\"}]}");
    return rendered;
}
