#pragma once

#include "Types.hpp"

#include <optional>
#include <string>
#include <vector>

struct FileRule {
    bool enabled{true};
    std::string field{"extension"};
    std::string op{"equals"};
    std::string value;
    std::string category;
    std::string subcategory;
};

class RuleStore {
public:
    static void initialize(const std::string& data_dir);
    static std::vector<FileRule> load();
    static bool save(const std::vector<FileRule>& rules);
};

class RuleEngine {
public:
    static bool matches(const FileEntry& entry, const FileRule& rule);
    static std::optional<CategorizedFile> apply_first(const FileEntry& entry,
                                                      const std::vector<FileRule>& rules);
};
