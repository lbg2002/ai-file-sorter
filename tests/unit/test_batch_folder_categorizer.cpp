#include "BatchFolderCategorizer.hpp"
#include "ILLMClient.hpp"
#include "PromptTemplateStore.hpp"

#include "TestHelpers.hpp"
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>


namespace {

class CountingBatchClient final : public ILLMClient {
public:
    int complete_calls{0};
    std::string last_prompt;

    std::string categorize_file(const std::string&,
                                const std::string&,
                                FileType,
                                const std::string&) override
    {
        throw std::runtime_error("per-file categorization must not be used by batch mode");
    }

    std::string complete_prompt(const std::string& prompt, int) override
    {
        ++complete_calls;
        last_prompt = prompt;
        return R"({"items":[{"id":0,"category":"Documents","subcategory":"Text","suggested_name":""},{"id":1,"category":"Research","subcategory":"Papers","suggested_name":""}]})";
    }

    void set_prompt_logging_enabled(bool) override {}
};

} // namespace

TEST_CASE("Folder batch categorization calls the LLM exactly once")
{
    TempDir temp;
    const auto root = temp.path();
    std::vector<FileEntry> entries{
        {(root / "a.txt").string(), "a.txt", FileType::File},
        {(root / "paper.pdf").string(), "paper.pdf", FileType::File},
    };

    BatchFolderCategorizationOptions options;
    options.folder_path = root.string();
    options.use_subcategories = true;

    CountingBatchClient client;
    BatchFolderCategorizer categorizer;
    const auto result = categorizer.categorize(client, entries, options);

    REQUIRE(client.complete_calls == 1);
    REQUIRE(client.last_prompt.find("\"id\":0") != std::string::npos);
    REQUIRE(client.last_prompt.find("\"id\":1") != std::string::npos);
    REQUIRE(result.files.size() == 2);
}

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
