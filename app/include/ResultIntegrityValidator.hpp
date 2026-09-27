#pragma once

#include "Types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

enum class IntegrityIssueKind {
    DuplicateSource,
    MissingSource,
    UnknownSource,
    TargetConflict
};

struct IntegrityIssue {
    IntegrityIssueKind kind{IntegrityIssueKind::MissingSource};
    std::string source_path;
    std::string target_path;
    std::string message;
};

struct ResultIntegrityReport {
    std::vector<IntegrityIssue> issues;

    bool has_blocking_issues() const;
    int count(IntegrityIssueKind kind) const;
    std::vector<IntegrityIssue> issues_for_source(const std::string& source_path) const;
};

class ResultIntegrityValidator {
public:
    static ResultIntegrityReport validate(const std::vector<FileEntry>& snapshot,
                                          const std::vector<CategorizedFile>& results,
                                          const std::string& base_dir,
                                          bool use_subcategories);

    static std::string source_path_for(const CategorizedFile& file);
    static std::string target_path_for(const CategorizedFile& file,
                                       const std::string& base_dir,
                                       bool use_subcategories);
};
