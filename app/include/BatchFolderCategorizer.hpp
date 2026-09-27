#pragma once

#include "Types.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class ILLMClient;

struct BatchFolderCategorizationOptions {
    std::string folder_path;
    bool recursive{false};
    bool use_subcategories{true};
    std::string category_language{"English"};
    bool prefer_stable_categories{false};
    std::vector<std::string> allowed_categories;
    std::unordered_map<std::string, std::vector<std::string>> allowed_subcategories_by_category;
};

struct BatchFolderCategorizationResult {
    std::vector<CategorizedFile> files;
    std::string raw_response;
    std::size_t requested_count{0};
    std::size_t returned_count{0};
};

class BatchFolderCategorizer {
public:
    static std::string build_inventory_json(const std::vector<FileEntry>& entries,
                                            const std::string& folder_path);
    static std::string build_prompt(const std::vector<FileEntry>& entries,
                                    const BatchFolderCategorizationOptions& options);
    static BatchFolderCategorizationResult parse_response(
        const std::string& response,
        const std::vector<FileEntry>& entries,
        const BatchFolderCategorizationOptions& options);
    static int recommended_max_output_tokens(std::size_t item_count);

    BatchFolderCategorizationResult categorize(
        ILLMClient& llm,
        const std::vector<FileEntry>& entries,
        const BatchFolderCategorizationOptions& options) const;
};
