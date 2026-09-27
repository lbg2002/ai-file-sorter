#include "RuleEngine.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace {

fs::path g_rule_path;

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string clean_field(std::string value)
{
    std::replace(value.begin(), value.end(), '\t', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    return value;
}

std::vector<std::string> split_tab(const std::string& line)
{
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string part;
    while (std::getline(ss, part, '\t')) {
        parts.push_back(part);
    }
    return parts;
}

} // namespace

void RuleStore::initialize(const std::string& data_dir)
{
    g_rule_path = Utils::utf8_to_path(data_dir) / "file_rules.tsv";
}

std::vector<FileRule> RuleStore::load()
{
    std::vector<FileRule> rules;
    if (g_rule_path.empty()) {
        return rules;
    }

    std::ifstream input(g_rule_path, std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        const auto parts = split_tab(line);
        if (parts.size() < 6) {
            continue;
        }
        FileRule rule;
        rule.enabled = parts[0] == "1";
        rule.field = parts[1];
        rule.op = parts[2];
        rule.value = parts[3];
        rule.category = parts[4];
        rule.subcategory = parts[5];
        rules.push_back(std::move(rule));
    }
    return rules;
}

bool RuleStore::save(const std::vector<FileRule>& rules)
{
    if (g_rule_path.empty()) {
        return false;
    }
    std::error_code ec;
    fs::create_directories(g_rule_path.parent_path(), ec);
    std::ofstream output(g_rule_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    for (const auto& rule : rules) {
        output << (rule.enabled ? "1" : "0") << '\t'
               << clean_field(rule.field) << '\t'
               << clean_field(rule.op) << '\t'
               << clean_field(rule.value) << '\t'
               << clean_field(rule.category) << '\t'
               << clean_field(rule.subcategory) << '\n';
    }
    return static_cast<bool>(output);
}

bool RuleEngine::matches(const FileEntry& entry, const FileRule& rule)
{
    if (!rule.enabled || rule.value.empty()) {
        return false;
    }

    const fs::path path = Utils::utf8_to_path(entry.full_path);
    std::string candidate;
    const std::string field = lower(rule.field);
    if (field == "extension") {
        candidate = Utils::path_to_utf8(path.extension());
    } else if (field == "filename" || field == "name") {
        candidate = entry.file_name;
    } else if (field == "path") {
        candidate = entry.full_path;
    } else if (field == "size" || field == "sizebytes") {
        if (entry.type != FileType::File) {
            return false;
        }
        std::error_code ec;
        const auto size = fs::file_size(path, ec);
        if (ec) {
            return false;
        }
        try {
            const auto threshold = static_cast<std::uintmax_t>(std::stoull(rule.value));
            const std::string op = lower(rule.op);
            if (op == ">" || op == "greater") return size > threshold;
            if (op == "<" || op == "less") return size < threshold;
            return size == threshold;
        } catch (...) {
            return false;
        }
    } else {
        return false;
    }

    const std::string op = lower(rule.op);
    if (op == "regex") {
        try {
            return std::regex_search(candidate, std::regex(rule.value, std::regex::icase));
        } catch (...) {
            return false;
        }
    }

    const std::string lhs = lower(candidate);
    const std::string rhs = lower(rule.value);
    if (op == "contains") {
        return lhs.find(rhs) != std::string::npos;
    }
    if (op == "startswith") {
        return lhs.rfind(rhs, 0) == 0;
    }
    if (op == "endswith") {
        return lhs.size() >= rhs.size() && lhs.compare(lhs.size() - rhs.size(), rhs.size(), rhs) == 0;
    }
    return lhs == rhs;
}

std::optional<CategorizedFile> RuleEngine::apply_first(const FileEntry& entry,
                                                       const std::vector<FileRule>& rules)
{
    for (const auto& rule : rules) {
        if (!matches(entry, rule)) {
            continue;
        }
        const fs::path path = Utils::utf8_to_path(entry.full_path);
        CategorizedFile result{
            Utils::path_to_utf8(path.parent_path()),
            entry.file_name,
            entry.type,
            rule.category,
            rule.subcategory,
            0
        };
        result.canonical_category = rule.category;
        result.canonical_subcategory = rule.subcategory;
        return result;
    }
    return std::nullopt;
}
