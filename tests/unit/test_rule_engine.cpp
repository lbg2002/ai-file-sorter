#include "RuleEngine.hpp"

#include "TestHelpers.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("RuleEngine matches extension and filename rules")
{
    TempDir temp;
    FileEntry pdf{(temp.path() / "paper.pdf").string(), "paper.pdf", FileType::File};

    FileRule extension;
    extension.field = "extension";
    extension.op = "equals";
    extension.value = ".pdf";
    extension.category = "Research";
    extension.subcategory = "Papers";

    REQUIRE(RuleEngine::matches(pdf, extension));
    const auto result = RuleEngine::apply_first(pdf, {extension});
    REQUIRE(result.has_value());
    REQUIRE(result->category == "Research");
    REQUIRE(result->subcategory == "Papers");

    FileRule name;
    name.field = "filename";
    name.op = "contains";
    name.value = "paper";
    name.category = "Documents";
    REQUIRE(RuleEngine::matches(pdf, name));
}

TEST_CASE("RuleEngine uses first matching rule")
{
    TempDir temp;
    FileEntry file{(temp.path() / "report.docx").string(), "report.docx", FileType::File};

    FileRule first{true, "filename", "contains", "report", "Meetings", "Reports"};
    FileRule second{true, "extension", "equals", ".docx", "Documents", "Word"};

    const auto result = RuleEngine::apply_first(file, {first, second});
    REQUIRE(result.has_value());
    REQUIRE(result->category == "Meetings");
}


TEST_CASE("RuleStore preserves rules with empty subcategories")
{
    TempDir temp;
    RuleStore::initialize(temp.path().string());

    FileRule rule{true, "extension", "equals", ".txt", "Documents", ""};
    REQUIRE(RuleStore::save({rule}));

    const auto loaded = RuleStore::load();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded.front().category == "Documents");
    REQUIRE(loaded.front().subcategory.empty());
}
