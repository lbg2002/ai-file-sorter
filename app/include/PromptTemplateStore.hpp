#pragma once

#include "Types.hpp"

#include <string>
#include <string_view>
#include <cstddef>

class PromptTemplateStore {
public:
    static void initialize(const std::string& data_dir);
    static bool enabled();
    static void set_enabled(bool enabled);
    static std::string prompt_template();
    static void set_prompt_template(std::string value);
    static std::string default_template();
    static bool save();

    static std::string render_or_default(std::string_view original_prompt,
                                         const std::string& file_name,
                                         const std::string& file_path,
                                         FileType file_type,
                                         const std::string& context);

    static std::string render_batch(const std::string& folder_path,
                                    const std::string& inventory_json,
                                    bool recursive,
                                    const std::string& category_language,
                                    const std::string& context,
                                    std::size_t item_count);
};
