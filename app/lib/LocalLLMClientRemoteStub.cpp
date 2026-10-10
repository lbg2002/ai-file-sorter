// Remote-API-only build: local inference entry points are intentionally unavailable.
#include "LocalLLMClient.hpp"
#include <stdexcept>
#include <utility>

namespace {
[[noreturn]] void local_unavailable()
{
    throw std::runtime_error("Local models are disabled in this REMOTE_API_ONLY build. Select a remote API provider.");
}
}

LocalLLMClient::LocalLLMClient(const std::string& path)
    : LocalLLMClient(path, {}, Options{}) {}
LocalLLMClient::LocalLLMClient(const std::string& path, FallbackDecisionCallback fallback)
    : LocalLLMClient(path, std::move(fallback), Options{}) {}
LocalLLMClient::LocalLLMClient(const std::string& path, FallbackDecisionCallback fallback, Options)
    : model_path(path), fallback_decision_callback_(std::move(fallback)) { local_unavailable(); }
LocalLLMClient::~LocalLLMClient() = default;
std::string LocalLLMClient::make_prompt(const std::string&, const std::string&, FileType, const std::string&)
{ local_unavailable(); }
std::string LocalLLMClient::generate_response(const std::string&, int, bool, const std::string&)
{ local_unavailable(); }
std::string LocalLLMClient::categorize_file(const std::string&, const std::string&, FileType, const std::string&)
{ local_unavailable(); }
std::string LocalLLMClient::complete_prompt(const std::string&, int)
{ local_unavailable(); }
void LocalLLMClient::set_prompt_logging_enabled(bool enabled)
{ prompt_logging_enabled = enabled; }
void LocalLLMClient::set_status_callback(StatusCallback callback)
{ status_callback_ = std::move(callback); }
void LocalLLMClient::set_fallback_decision_callback(FallbackDecisionCallback callback)
{ fallback_decision_callback_ = std::move(callback); }
