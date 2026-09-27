#include "BatchFolderCategorizer.hpp"

#include "ILLMClient.hpp"
#include "PromptTemplateStore.hpp"
#include "Utils.hpp"

#if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
    #include <json/json.h>
#else
    #error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;

namespace {

std::string trim_copy(const std::string& value)
{
    const char* whitespace = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::string json_text_or_empty(const Json::Value& object, const char* key)
{
    if (!object.isObject() || !object.isMember(key) || !object[key].isString()) {
        return {};
    }
    return trim_copy(object[key].asString());
}

std::string relative_display_path(const std::string& full_path, const std::string& root_path)
{
    const fs::path full = Utils::utf8_to_path(full_path).lexically_normal();
    const fs::path root = Utils::utf8_to_path(root_path).lexically_normal();
    const fs::path relative = full.lexically_relative(root);
    if (relative.empty()) {
        return Utils::path_to_utf8(full.filename());
    }

    const std::string text = Utils::path_to_utf8(relative);
    if (text == ".." || text.rfind("../", 0) == 0 || text.rfind("..\\", 0) == 0) {
        return Utils::path_to_utf8(full.filename());
    }
    return text;
}

std::string extract_json_payload(const std::string& response)
{
    std::string trimmed = trim_copy(response);
    if (trimmed.rfind("```", 0) == 0) {
        const auto first_newline = trimmed.find('\n');
        const auto closing = trimmed.rfind("```");
        if (first_newline != std::string::npos &&
            closing != std::string::npos &&
            closing > first_newline) {
            trimmed = trim_copy(trimmed.substr(first_newline + 1, closing - first_newline - 1));
        }
    }

    const auto object_start = trimmed.find('{');
    const auto object_end = trimmed.rfind('}');
    if (object_start != std::string::npos &&
        object_end != std::string::npos &&
        object_end >= object_start) {
        return trimmed.substr(object_start, object_end - object_start + 1);
    }

    const auto array_start = trimmed.find('[');
    const auto array_end = trimmed.rfind(']');
    if (array_start != std::string::npos &&
        array_end != std::string::npos &&
        array_end >= array_start) {
        return trimmed.substr(array_start, array_end - array_start + 1);
    }

    return trimmed;
}

bool parse_item_id(const Json::Value& item, long long& id)
{
    if (!item.isObject() || !item.isMember("id")) {
        return false;
    }
    const Json::Value& raw = item["id"];
    if (raw.isInt64() || raw.isUInt64() || raw.isInt() || raw.isUInt()) {
        id = raw.asInt64();
        return true;
    }
    if (raw.isString()) {
        try {
            const std::string text = trim_copy(raw.asString());
            std::size_t consumed = 0;
            const long long parsed = std::stoll(text, &consumed);
            if (consumed == text.size()) {
                id = parsed;
                return true;
            }
        } catch (...) {
        }
    }
    return false;
}

bool safe_filename_only(const std::string& value)
{
    if (value.empty() || value == "." || value == "..") {
        return false;
    }
    return value.find('/') == std::string::npos &&
           value.find('\\') == std::string::npos;
}

std::string constraints_text(const BatchFolderCategorizationOptions& options)
{
    std::ostringstream out;
    out << "Category language: " << options.category_language << "\n";
    out << "Categorization style: "
        << (options.prefer_stable_categories
                ? "prefer a small stable taxonomy and reuse the same category names"
                : "prefer precise useful categories while keeping the taxonomy understandable")
        << "\n";
    out << "Subcategories: " << (options.use_subcategories ? "enabled" : "disabled") << "\n";

    if (!options.allowed_categories.empty()) {
        out << "Allowed main categories (use only these exact labels):\n";
        for (const auto& category : options.allowed_categories) {
            out << "- " << category << "\n";
        }
    }

    if (!options.allowed_subcategories_by_category.empty()) {
        out << "Allowed subcategories by main category:\n";
        for (const auto& [category, subcategories] : options.allowed_subcategories_by_category) {
            out << "- " << category << ": ";
            for (std::size_t i = 0; i < subcategories.size(); ++i) {
                if (i > 0) {
                    out << ", ";
                }
                out << subcategories[i];
            }
            out << "\n";
        }
    }
    return out.str();
}

CategorizedFile make_unknown_item(long long id,
                                  const Json::Value& item,
                                  const BatchFolderCategorizationOptions& options,
                                  std::size_t invalid_index)
{
    std::string marker;
    if (id == std::numeric_limits<long long>::min()) {
        marker = "__AI_INVALID_ITEM_" + std::to_string(invalid_index) + "__";
    } else {
        marker = "__AI_UNKNOWN_ID_" + std::to_string(id) + "__";
    }

    CategorizedFile result{
        options.folder_path,
        marker,
        FileType::File,
        json_text_or_empty(item, "category"),
        options.use_subcategories ? json_text_or_empty(item, "subcategory") : std::string(),
        0
    };
    result.canonical_category = result.category;
    result.canonical_subcategory = result.subcategory;
    return result;
}

} // namespace

std::string BatchFolderCategorizer::build_inventory_json(const std::vector<FileEntry>& entries,
                                                         const std::string& folder_path)
{
    Json::Value root(Json::arrayValue);
    for (Json::ArrayIndex i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const fs::path path = Utils::utf8_to_path(entry.full_path);

        Json::Value item(Json::objectValue);
        item["id"] = static_cast<Json::UInt64>(i);
        item["relative_path"] = relative_display_path(entry.full_path, folder_path);
        item["name"] = entry.file_name;
        item["type"] = entry.type == FileType::Directory ? "directory" : "file";
        item["extension"] = entry.type == FileType::File
            ? Utils::path_to_utf8(path.extension())
            : std::string();
        root.append(item);
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string BatchFolderCategorizer::build_prompt(
    const std::vector<FileEntry>& entries,
    const BatchFolderCategorizationOptions& options)
{
    const std::string inventory = build_inventory_json(entries, options.folder_path);
    const std::string context = constraints_text(options);
    return PromptTemplateStore::render_batch(
        options.folder_path,
        inventory,
        options.recursive,
        options.category_language,
        context,
        entries.size());
}

BatchFolderCategorizationResult BatchFolderCategorizer::parse_response(
    const std::string& response,
    const std::vector<FileEntry>& entries,
    const BatchFolderCategorizationOptions& options)
{
    BatchFolderCategorizationResult result;
    result.raw_response = response;
    result.requested_count = entries.size();

    const std::string payload = extract_json_payload(response);
    Json::CharReaderBuilder reader_builder;
    Json::Value root;
    std::istringstream stream(payload);
    std::string errors;
    if (!Json::parseFromStream(reader_builder, stream, &root, &errors)) {
        throw std::runtime_error(
            "The model did not return valid JSON for the folder batch: " + errors);
    }

    const Json::Value* items = nullptr;
    if (root.isObject() && root["items"].isArray()) {
        items = &root["items"];
    } else if (root.isArray()) {
        items = &root;
    }
    if (!items) {
        throw std::runtime_error(
            "The model response must be a JSON object with an 'items' array.");
    }

    std::size_t invalid_index = 0;
    result.files.reserve(items->size());
    for (const auto& item : *items) {
        long long id = std::numeric_limits<long long>::min();
        if (!parse_item_id(item, id) || id < 0 ||
            static_cast<std::size_t>(id) >= entries.size()) {
            result.files.push_back(make_unknown_item(id, item, options, invalid_index++));
            continue;
        }

        const FileEntry& source = entries[static_cast<std::size_t>(id)];
        const fs::path source_path = Utils::utf8_to_path(source.full_path);
        CategorizedFile categorized{
            Utils::path_to_utf8(source_path.parent_path()),
            source.file_name,
            source.type,
            json_text_or_empty(item, "category"),
            options.use_subcategories ? json_text_or_empty(item, "subcategory") : std::string(),
            0
        };

        std::string suggested_name = json_text_or_empty(item, "suggested_name");
        if (suggested_name.empty()) {
            suggested_name = json_text_or_empty(item, "new_name");
        }
        if (safe_filename_only(suggested_name) && suggested_name != source.file_name) {
            categorized.suggested_name = suggested_name;
        }

        categorized.canonical_category = categorized.category;
        categorized.canonical_subcategory = categorized.subcategory;
        result.files.push_back(std::move(categorized));
    }

    result.returned_count = result.files.size();
    return result;
}

int BatchFolderCategorizer::recommended_max_output_tokens(std::size_t item_count)
{
    const std::size_t estimate = 768 + item_count * 72;
    return static_cast<int>(std::clamp<std::size_t>(estimate, 2048, 32768));
}

BatchFolderCategorizationResult BatchFolderCategorizer::categorize(
    ILLMClient& llm,
    const std::vector<FileEntry>& entries,
    const BatchFolderCategorizationOptions& options) const
{
    if (entries.empty()) {
        return BatchFolderCategorizationResult{};
    }

    const std::string prompt = build_prompt(entries, options);
    if (prompt.size() > 700000) {
        throw std::runtime_error(
            "The selected folder inventory is too large for one safe AI request. "
            "Reduce the selection or disable recursive scanning.");
    }

    const std::string response =
        llm.complete_prompt(prompt, recommended_max_output_tokens(entries.size()));
    return parse_response(response, entries, options);
}
