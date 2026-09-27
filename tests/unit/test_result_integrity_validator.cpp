#include "ResultIntegrityValidator.hpp"

#include "TestHelpers.hpp"
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("ResultIntegrityValidator detects duplicate missing unknown and target conflicts")
{
    TempDir temp;
    const auto root = temp.path();

    const std::string a = (root / "a.txt").string();
    const std::string b = (root / "b.txt").string();
    const std::string c = (root / "c.txt").string();

    std::vector<FileEntry> snapshot{
        {a, "a.txt", FileType::File},
        {b, "b.txt", FileType::File},
        {c, "c.txt", FileType::File},
    };

    CategorizedFile r1{root.string(), "a.txt", FileType::File, "Docs", "", 0};
    CategorizedFile r2{root.string(), "a.txt", FileType::File, "Other", "", 0};
    CategorizedFile r3{root.string(), "b.txt", FileType::File, "Docs", "", 0};
    r3.suggested_name = "same.txt";
    CategorizedFile r4{root.string(), "ghost.txt", FileType::File, "Docs", "", 0};
    r4.suggested_name = "same.txt";

    const auto report = ResultIntegrityValidator::validate(snapshot, {r1, r2, r3, r4}, root.string(), false);

    REQUIRE(report.count(IntegrityIssueKind::DuplicateSource) == 1);
    REQUIRE(report.count(IntegrityIssueKind::MissingSource) == 1);
    REQUIRE(report.count(IntegrityIssueKind::UnknownSource) == 1);
    REQUIRE(report.count(IntegrityIssueKind::TargetConflict) == 2);
    REQUIRE(report.has_blocking_issues());
}

TEST_CASE("Missing sources alone do not block safe apply")
{
    TempDir temp;
    const auto root = temp.path();
    std::vector<FileEntry> snapshot{
        {(root / "a.txt").string(), "a.txt", FileType::File},
        {(root / "b.txt").string(), "b.txt", FileType::File},
    };
    CategorizedFile only{root.string(), "a.txt", FileType::File, "Docs", "", 0};

    const auto report = ResultIntegrityValidator::validate(snapshot, {only}, root.string(), false);
    REQUIRE(report.count(IntegrityIssueKind::MissingSource) == 1);
    REQUIRE_FALSE(report.has_blocking_issues());
}


TEST_CASE("Unsafe path-like categories block processing")
{
    TempDir temp;
    const auto root = temp.path();
    std::vector<FileEntry> snapshot{
        {(root / "a.txt").string(), "a.txt", FileType::File},
    };
    CategorizedFile proposed{root.string(), "a.txt", FileType::File, "../escape", "", 0};

    const auto report = ResultIntegrityValidator::validate(
        snapshot, {proposed}, root.string(), false);

    REQUIRE(report.count(IntegrityIssueKind::UnsafeTarget) == 1);
    REQUIRE(report.has_blocking_issues());
}
