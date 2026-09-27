#pragma once

#include "CategoryLanguage.hpp"
#include "Types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

class ILLMClient;

struct FolderBatchCategorizerOptions {
    bool use_subcategories{true};
    bool use_whitelist{false};
    CategoryLanguage category_language{CategoryLanguage::English};
    std::vector<std::string> allowed_categories;
    std::vector<std::string> allowed_subcategories;
    std::unordered_map<std::string, std::vector<std::string>> allowed_subcategories_by_category;
    std::string custom_policy;
};

struct FolderBatchCategorizerResult {
    std::vector<CategorizedFile> items;
    std::string raw_response;
    std::vector<std::string> warnings;
};

class FolderBatchCategorizer {
public:
    static FolderBatchCategorizerResult categorize(ILLMClient& llm,
                                                   const std::vector<FileEntry>& entries,
                                                   const std::string& folder_path,
                                                   const FolderBatchCategorizerOptions& options);

    static std::string build_prompt(const std::vector<FileEntry>& entries,
                                    const std::string& folder_path,
                                    const FolderBatchCategorizerOptions& options);

    static FolderBatchCategorizerResult parse_response(const std::string& response,
                                                       const std::vector<FileEntry>& entries,
                                                       const std::string& folder_path);
};
