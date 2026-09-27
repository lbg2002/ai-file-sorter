#include "ResultIntegrityValidator.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace {

std::string normalize_path(const std::string& value)
{
    if (value.empty()) {
        return {};
    }
    std::error_code ec;
    fs::path path = Utils::utf8_to_path(value);
    fs::path normalized = path.lexically_normal();
#ifdef _WIN32
    std::string text = Utils::path_to_utf8(normalized);
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
#else
    return Utils::path_to_utf8(normalized);
#endif
}

bool safe_path_component(const std::string& value)
{
    if (value.empty() || value == "." || value == "..") {
        return false;
    }
    return value.find('/') == std::string::npos &&
           value.find('\\') == std::string::npos;
}

std::string destination_name_for(const CategorizedFile& file)
{
    if (file.suggested_name.empty() || file.suggested_name == file.file_name) {
        return file.file_name;
    }

    const fs::path original = Utils::utf8_to_path(file.file_name);
    const fs::path suggested = Utils::utf8_to_path(file.suggested_name);
    if (!suggested.has_extension() && original.has_extension()) {
        return Utils::path_to_utf8(suggested) + Utils::path_to_utf8(original.extension());
    }
    return file.suggested_name;
}

} // namespace

bool ResultIntegrityReport::has_blocking_issues() const
{
    return std::any_of(issues.begin(), issues.end(), [](const IntegrityIssue& issue) {
        return issue.kind == IntegrityIssueKind::DuplicateSource ||
               issue.kind == IntegrityIssueKind::UnknownSource ||
               issue.kind == IntegrityIssueKind::TargetConflict ||
               issue.kind == IntegrityIssueKind::UnsafeTarget;
    });
}

int ResultIntegrityReport::count(IntegrityIssueKind kind) const
{
    return static_cast<int>(std::count_if(issues.begin(), issues.end(),
        [kind](const IntegrityIssue& issue) { return issue.kind == kind; }));
}

std::vector<IntegrityIssue> ResultIntegrityReport::issues_for_source(const std::string& source_path) const
{
    const std::string key = normalize_path(source_path);
    std::vector<IntegrityIssue> matches;
    for (const auto& issue : issues) {
        if (normalize_path(issue.source_path) == key) {
            matches.push_back(issue);
        }
    }
    return matches;
}

std::string ResultIntegrityValidator::source_path_for(const CategorizedFile& file)
{
    fs::path parent = Utils::utf8_to_path(file.file_path);
    return normalize_path(Utils::path_to_utf8(parent / Utils::utf8_to_path(file.file_name)));
}

std::string ResultIntegrityValidator::target_path_for(const CategorizedFile& file,
                                                       const std::string& base_dir,
                                                       bool use_subcategories)
{
    fs::path target = Utils::utf8_to_path(base_dir);
    if (!file.category.empty()) {
        target /= Utils::utf8_to_path(file.category);
    }
    if (use_subcategories && !file.subcategory.empty()) {
        target /= Utils::utf8_to_path(file.subcategory);
    }
    target /= Utils::utf8_to_path(destination_name_for(file));
    return normalize_path(Utils::path_to_utf8(target));
}

ResultIntegrityReport ResultIntegrityValidator::validate(const std::vector<FileEntry>& snapshot,
                                                         const std::vector<CategorizedFile>& results,
                                                         const std::string& base_dir,
                                                         bool use_subcategories)
{
    ResultIntegrityReport report;

    std::unordered_set<std::string> snapshot_paths;
    for (const auto& entry : snapshot) {
        snapshot_paths.insert(normalize_path(entry.full_path));
    }

    std::unordered_map<std::string, int> source_counts;
    std::unordered_map<std::string, std::vector<std::string>> target_sources;
    for (const auto& result : results) {
        const std::string source = source_path_for(result);
        ++source_counts[source];

        if (!snapshot_paths.contains(source)) {
            report.issues.push_back({
                IntegrityIssueKind::UnknownSource,
                source,
                {},
                "The result references a source item that was not present in the scan snapshot."
            });
        }

        if (!result.rename_only) {
            if (!safe_path_component(result.category) ||
                (use_subcategories && !result.subcategory.empty() &&
                 !safe_path_component(result.subcategory))) {
                report.issues.push_back({
                    IntegrityIssueKind::UnsafeTarget,
                    source,
                    {},
                    "The proposed category contains an empty or path-like destination component."
                });
                continue;
            }
        }

        const std::string target = target_path_for(result, base_dir, use_subcategories);
        target_sources[target].push_back(source);
    }

    for (const auto& [source, count] : source_counts) {
        if (count > 1) {
            report.issues.push_back({
                IntegrityIssueKind::DuplicateSource,
                source,
                {},
                "The same source item appears more than once in the proposed result."
            });
        }
    }

    for (const auto& source : snapshot_paths) {
        if (!source_counts.contains(source)) {
            report.issues.push_back({
                IntegrityIssueKind::MissingSource,
                source,
                {},
                "The scanned item is missing from the proposed result. It will remain in place."
            });
        }
    }

    for (const auto& [target, sources] : target_sources) {
        std::unordered_set<std::string> unique_sources(sources.begin(), sources.end());
        if (unique_sources.size() > 1) {
            for (const auto& source : unique_sources) {
                report.issues.push_back({
                    IntegrityIssueKind::TargetConflict,
                    source,
                    target,
                    "Multiple source items resolve to the same destination path."
                });
            }
        }
    }

    return report;
}
