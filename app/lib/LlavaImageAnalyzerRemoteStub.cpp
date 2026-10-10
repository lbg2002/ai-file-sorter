// The lightweight build does not load GGUF, llama.cpp or GGML libraries.
#include "LlavaImageAnalyzer.hpp"
#include <stdexcept>

namespace {
[[noreturn]] void visual_unavailable()
{
    throw std::runtime_error("Local visual models are not available in REMOTE_API_ONLY.");
}
}

LlavaImageAnalyzer::LlavaImageAnalyzer(const std::filesystem::path& model,
                                     const std::filesystem::path& projector,
                                     VisualPromptPolicy policy,
                                     Settings options)
    : settings_(options), prompt_policy_(policy)
{
    (void)model;
    (void)projector;
    visual_unavailable();
}
LlavaImageAnalyzer::LlavaImageAnalyzer(const std::filesystem::path& model,
                                     const std::filesystem::path& projector,
                                     VisualPromptPolicy policy)
    : LlavaImageAnalyzer(model, projector, policy, Settings{}) {}
LlavaImageAnalyzer::~LlavaImageAnalyzer() = default;
ImageAnalysisResult LlavaImageAnalyzer::analyze(const std::filesystem::path&)
{ visual_unavailable(); }
bool LlavaImageAnalyzer::is_supported_image(const std::filesystem::path&)
{ return false; }
