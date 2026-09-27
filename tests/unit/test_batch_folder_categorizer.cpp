#include "BatchFolderCategorizer.hpp"
#include "PromptTemplateStore.hpp"

#include "TestHelpers.hpp"
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("BatchFolderCategorizer parses one structured response for a folder")
{
    TempDir temp;
    const auto root = temp.path();

    std::vector<FileEntry> entries{
        {(root / "paper.pdf").string(), "paper.pdf", FileType::File},
        {(root / "notes.txt").string(), "notes.txt", FileType::File},
    };

    BatchFolderCategorizationOptions options;
    options.folder_path = root.string();
    options.use_subcategories = true;

    const std::string response = R"({
        "items": [
            {"id": 0, "category": "Research", "subcategory": "Papers", "suggested_name": "orbit-paper.pdf"},
            {"id": 1, "category": "Notes", "subcategory": "Text", "suggested_name": ""}
        ]
    })";

    const auto parsed = BatchFolderCategorizer::parse_response(response, entries, options);
    REQUIRE(parsed.requested_count == 2);
    REQUIRE(parsed.returned_count == 2);
    REQUIRE(parsed.files.size() == 2);
    REQUIRE(parsed.files[0].file_name == "paper.pdf");
    REQUIRE(parsed.files[0].category == "Research");
    REQUIRE(parsed.files[0].suggested_name == "orbit-paper.pdf");
    REQUIRE(parsed.files[1].category == "Notes");
}

TEST_CASE("BatchFolderCategorizer keeps unknown ids visible for safety validation")
{
    TempDir temp;
    const auto root = temp.path();

    std::vector<FileEntry> entries{
        {(root / "real.txt").string(), "real.txt", FileType::File},
    };

    BatchFolderCategorizationOptions options;
    options.folder_path = root.string();

    const auto parsed = BatchFolderCategorizer::parse_response(
        R"({"items":[{"id":99,"category":"Invented","subcategory":"Other"}]})",
        entries,
        options);

    REQUIRE(parsed.files.size() == 1);
    REQUIRE(parsed.files.front().file_name.find("__AI_UNKNOWN_ID_99__") != std::string::npos);
}

TEST_CASE("BatchFolderCategorizer rejects path-like rename suggestions")
{
    TempDir temp;
    const auto root = temp.path();

    std::vector<FileEntry> entries{
        {(root / "a.txt").string(), "a.txt", FileType::File},
    };

    BatchFolderCategorizationOptions options;
    options.folder_path = root.string();

    const auto parsed = BatchFolderCategorizer::parse_response(
        R"({"items":[{"id":0,"category":"Docs","subcategory":"","suggested_name":"../escape.txt"}]})",
        entries,
        options);

    REQUIRE(parsed.files.size() == 1);
    REQUIRE(parsed.files.front().suggested_name.empty());
}
