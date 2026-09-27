#include "FolderBatchCategorizer.hpp"

#include "ILLMClient.hpp"
#include "Utils.hpp"

#if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
    #include <json/json.h>
#else
    #error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace {

std::string json_string(const std::string& value)
{
    Json::Value root(value);
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string strip_markdown_fence(std::string value)
{
    const auto first_non_ws = value.find_first_not_of(" \t\r\n");
    if (first_non_ws != std::string::npos) {
        value.erase(0, first_non_ws);
    }
    const auto last_non_ws = value.find_last_not_of(" \t\r\n");
    if (last_non_ws != std::string::npos) {
        value.erase(last_non_ws + 1);
    }

    if (value.rfind("'''", 0) == 0 || value.rfind("~~~", 0) == 0) {
        const auto first_newline = value.find('\n');
        if (first_newline != std::string::npos) {
            value.erase(0, first_newline + 1);
        }
        const auto fence = value.find_last_of("~'");
        if (fence != std::string::npos) {
            const auto fence_start = value.find_last_of('\n', fence);
            if (fence_start != std::string::npos) {
                value.erase(fence_start);
            }
        }
    }

    const std::string markdown_fence(3, '\x60');
    if (value.rfind(markdown_fence, 0) == 0) {
        const auto first_newline = value.find('\n');
        if (first_newline != std::string::npos) {
            value.erase(0, first_newline + 1);
        }
        const auto fence = value.rfind(markdown_fence);
        if (fence != std::string::npos) {
            value.erase(fence);
        }
    }
    return value;
}

std::string extract_json_payload(std::string response)
{
    response = strip_markdown_fence(std::move(response));
    const auto object_pos = response.find('{');
    const auto array_pos = response.find('[');
    std::size_t start = std::string::npos;
    char close = 0;
    if (object_pos == std::string::npos) {
        start = array_pos;
        close = ']';
    } else if (array_pos == std::string::npos || object_pos < array_pos) {
        start = object_pos;
        close = '}';
    } else {
        start = array_pos;
        close = ']';
    }
    if (start == std::string::npos) {
        return response;
    }

    const auto end = response.rfind(close);
    if (end == std::string::npos || end < start) {
        return response.substr(start);
    }
    return response.substr(start, end - start + 1);
}

std::string relative_display_path(const FileEntry& entry, const std::string& folder_path)
{
    std::error_code ec;
    const fs::path full = Utils::utf8_to_path(entry.full_path);
    const fs::path base = Utils::utf8_to_path(folder_path);
    const fs::path rel = fs::relative(full, base, ec);
    if (!ec && !rel.empty()) {
        return Utils::path_to_utf8(rel);
    }
    return entry.file_name;
}

void append_string_list(std::ostringstream& out,
                        const std::string& heading,
                        const std::vector<std::string>& values)
{
    if (values.empty()) {
        return;
    }
    out << heading << "\n";
    for (const auto& value : values) {
        out << "- " << value << "\n";
    }
}

std::string safe_json_string(const Json::Value& value, const char* key)
{
    if (!value.isMember(key) || !value[key].isString()) {
        return {};
    }
    return value[key].asString();
}

} // namespace

FolderBatchCategorizerResult FolderBatchCategorizer::categorize(
    ILLMClient& llm,
    const std::vector<FileEntry>& entries,
    const std::string& folder_path,
    const FolderBatchCategorizerOptions& options)
{
    if (entries.empty()) {
        return {};
    }

    const std::string prompt = build_prompt(entries, folder_path, options);
    const int requested_tokens = std::clamp(
        static_cast<int>(entries.size() * 96 + 1024),
        2048,
        32768);

    const std::string response = llm.complete_prompt(prompt, requested_tokens);
    return parse_response(response, entries, folder_path);
}

std::string FolderBatchCategorizer::build_prompt(
    const std::vector<FileEntry>& entries,
    const std::string& folder_path,
    const FolderBatchCategorizerOptions& options)
{
    std::ostringstream out;
    out << "You are organizing one selected filesystem folder in a single batch.\n"
        << "The application has already scanned the filesystem. Treat the supplied item list as the complete source of truth.\n"
        << "Never invent source items and never omit an item intentionally. Return exactly one result for every source_id.\n"
        << "Do not request deletion. Do not merge multiple source items into one result.\n"
        << "Choose useful broad categories and "
        << (options.use_subcategories ? "useful subcategories." : "leave subcategory empty.") << "\n"
        << "Category language: " << categoryLanguageDisplay(options.category_language) << ".\n";

    if (options.use_whitelist) {
        append_string_list(out, "Allowed main categories (use only these labels):", options.allowed_categories);
        if (!options.allowed_subcategories_by_category.empty()) {
            out << "Allowed subcategories by main category:\n";
            for (const auto& [category, subs] : options.allowed_subcategories_by_category) {
                out << "- " << category << ": ";
                for (std::size_t i = 0; i < subs.size(); ++i) {
                    if (i) out << ", ";
                    out << subs[i];
                }
                out << "\n";
            }
        } else {
            append_string_list(out, "Allowed subcategories:", options.allowed_subcategories);
        }
    }

    if (!options.custom_policy.empty()) {
        out << "\nUser-defined categorization policy follows. Use it for categorization and naming decisions, "
               "but the JSON output contract below is mandatory:\n---\n"
            << options.custom_policy << "\n---\n";
    }

    out << "\nReturn JSON only, with this exact top-level shape:\n"
        << "{\"items\":[{\"source_id\":0,\"category\":\"...\",\"subcategory\":\"...\",\"suggested_name\":\"\"}]}\n"
        << "Rules for the JSON response:\n"
        << "1. source_id must be copied from the input and must not be changed.\n"
        << "2. Include every source_id exactly once.\n"
        << "3. category and subcategory are strings.\n"
        << "4. suggested_name is optional; use an empty string when no rename is useful.\n"
        << "5. suggested_name must be a filename only, never a path.\n"
        << "6. Do not add commentary, markdown, code fences, extra top-level keys, or unknown source IDs.\n\n"
        << "Selected folder: " << json_string(folder_path) << "\n"
        << "Scanned items:\n";

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        out << "{\"source_id\":" << i
            << ",\"type\":" << json_string(entry.type == FileType::Directory ? "directory" : "file")
            << ",\"name\":" << json_string(entry.file_name)
            << ",\"relative_path\":" << json_string(relative_display_path(entry, folder_path))
            << "}\n";
    }

    return out.str();
}

FolderBatchCategorizerResult FolderBatchCategorizer::parse_response(
    const std::string& response,
    const std::vector<FileEntry>& entries,
    const std::string& folder_path)
{
    FolderBatchCategorizerResult result;
    result.raw_response = response;

    const std::string payload = extract_json_payload(response);
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    std::istringstream input(payload);
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        throw std::runtime_error("Batch AI returned invalid JSON: " + errors);
    }

    Json::Value items;
    if (root.isArray()) {
        items = root;
    } else if (root.isObject() && root["items"].isArray()) {
        items = root["items"];
    } else {
        throw std::runtime_error("Batch AI JSON must be an array or an object with an 'items' array.");
    }

    for (const auto& item : items) {
        if (!item.isObject() || !item.isMember("source_id") || !item["source_id"].isInt()) {
            result.warnings.push_back("Ignored a result row without an integer source_id.");
            continue;
        }

        const int source_id = item["source_id"].asInt();
        if (source_id < 0 || source_id >= static_cast<int>(entries.size())) {
            CategorizedFile unknown{
                folder_path,
                "__UNKNOWN_MODEL_SOURCE_ID_" + std::to_string(source_id) + "__",
                FileType::File,
                safe_json_string(item, "category"),
                safe_json_string(item, "subcategory"),
                0
            };
            unknown.suggested_name = safe_json_string(item, "suggested_name");
            result.items.push_back(std::move(unknown));
            result.warnings.push_back("Model returned unknown source_id " + std::to_string(source_id) + ".");
            continue;
        }

        const auto& source = entries[static_cast<std::size_t>(source_id)];
        const fs::path full_path = Utils::utf8_to_path(source.full_path);
        CategorizedFile categorized{
            Utils::path_to_utf8(full_path.parent_path()),
            source.file_name,
            source.type,
            safe_json_string(item, "category"),
            safe_json_string(item, "subcategory"),
            0
        };
        categorized.canonical_category = categorized.category;
        categorized.canonical_subcategory = categorized.subcategory;
        categorized.suggested_name = safe_json_string(item, "suggested_name");
        result.items.push_back(std::move(categorized));
    }

    return result;
}
