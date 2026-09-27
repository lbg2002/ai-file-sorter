#include "PromptTemplateStore.hpp"

#include "Utils.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

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
        "You are a file categorization assistant. Classify the current item for filesystem organization.\n"
        "Item type: {{item_type}}\n"
        "File name: {{filename}}\n"
        "Path: {{path}}\n"
        "Use the filename, extension, path, and any document/image summary supplied in the user message.\n"
        "Respect any allowed-category or consistency constraints in this context:\n{{context}}\n"
        "Do not invent files, paths, categories outside explicit constraints, or extra operations.\n"
        "Return exactly one line in this format: {{output_format}}";
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
                                                   const std::string& file_name,
                                                   const std::string& file_path,
                                                   FileType file_type,
                                                   const std::string& context)
{
    std::scoped_lock lock(g_mutex);
    if (!g_enabled) {
        return std::string(original_prompt);
    }

    std::string rendered = g_template.empty() ? default_template() : g_template;
    replace_all(rendered, "{{filename}}", file_name);
    replace_all(rendered, "{{path}}", file_path);
    replace_all(rendered, "{{item_type}}", file_type == FileType::Directory ? "directory" : "file");
    replace_all(rendered, "{{context}}", context);
    replace_all(rendered, "{{output_format}}", "<Main category> : <Subcategory>");
    return rendered;
}
