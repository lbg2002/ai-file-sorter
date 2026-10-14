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
        "你是一个谨慎的文件与文件夹整理助手。目标是减少目录碎片，而不是为每个文件创建新分类。\n"
        "当前项目：{{item_type}}；名称：{{filename}}；路径：{{path}}。\n"
        "参考文件名、后缀、父目录及提供的文档/图片摘要判断用途。\n"
        "现有文件夹与历史分类候选（如有）优先于创建新目录；相近文件应归入相同的稳定大类。\n"
        "仅在明显不适合已有分类时建议新目录；禁止为单个文件建立无必要的专属分类。\n"
        "主分类一般采用宽泛的用途类别（如 Documents、Images、Software、Archives、Projects），"
        "子分类仅在确有必要时细化到主题。不要把文件名、日期、扩展名直接当分类。\n"
        "如果项目类型为 directory，请把文件夹当作不可拆分的整体，依据文件夹名称与内容概况分类；"
        "不得建议拆散文件夹内的文件，尤其不能破坏项目、依赖及版本管理结构。\n"
        "严格遵守此处的白名单、分类一致性和语言等约束：\n{{context}}\n"
        "分类标签以英文输出（由应用按所选语言统一翻译）；只返回一行，不附加理由、不产生文件操作。\n"
        "格式：{{output_format}}";
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
        // Keep the specialized upstream image/document instructions intact, but
        // consistently discourage fragmented taxonomy even without an override.
        return std::string(original_prompt) +
            "\\n整理策略：优先复用现有目录和已经采用的分类名称，合并语义相近的资料；"
            "不要为了单个文件临时创建新类别。将目录视为整体，不拆分其内容。"
            "严格遵守既有白名单与输出格式，只输出英文分类标签。";
    }

    std::string rendered = g_template.empty() ? default_template() : g_template;
    replace_all(rendered, "{{filename}}", file_name);
    replace_all(rendered, "{{path}}", file_path);
    replace_all(rendered, "{{item_type}}", file_type == FileType::Directory ? "directory" : "file");
    replace_all(rendered, "{{context}}", context);
    replace_all(rendered, "{{output_format}}", "<Main category> : <Subcategory>");
    return rendered;
}
