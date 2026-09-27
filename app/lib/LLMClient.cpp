#include "LLMClient.hpp"
#include "PromptTemplateStore.hpp"
#include "Types.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include "RemoteApiError.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <filesystem>
#if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
    #include <json/json.h>
#else
    #error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <iostream>
#include <sstream>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>
#include <utility>

// Helper function to write the response from curl into a string
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *response)
{
    size_t totalSize = size * nmemb;
    response->append(static_cast<const char*>(contents), totalSize);
    return totalSize;
}

namespace {
std::string trim_ws(const std::string& value);

std::string escape_json(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                out += c;
        }
    }
    return out;
}

long resolve_custom_timeout_seconds() {
    const char* env = std::getenv("AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT");
    if (env && *env) {
        char* end = nullptr;
        const long value = std::strtol(env, &end, 10);
        if (end != env && value > 0) {
            return value;
        }
    }
    return 300L;
}

long resolve_openai_timeout_seconds() {
    return 120L;
}

long resolve_timeout_seconds(const std::string& base_url) {
    const std::string trimmed = trim_ws(base_url);
    if (trimmed.empty()) {
        return resolve_openai_timeout_seconds();
    }
    return resolve_custom_timeout_seconds();
}

std::string trim_ws(const std::string& value) {
    const char* whitespace = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(whitespace);
    const auto end = value.find_last_not_of(whitespace);
    if (start == std::string::npos || end == std::string::npos) {
        return std::string();
    }
    return value.substr(start, end - start + 1);
}

std::string trim_trailing_slashes(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

bool ends_with(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

struct CurlRequest {
    CURL* handle{nullptr};
    curl_slist* headers{nullptr};
    std::array<char, CURL_ERROR_SIZE> error_buffer{};

    CurlRequest() = default;
    CurlRequest(const CurlRequest&) = delete;
    CurlRequest& operator=(const CurlRequest&) = delete;

    CurlRequest(CurlRequest&& other) noexcept
        : handle(other.handle),
          headers(other.headers)
    {
        other.handle = nullptr;
        other.headers = nullptr;
    }

    CurlRequest& operator=(CurlRequest&& other) noexcept
    {
        if (this != &other) {
            cleanup();
            handle = other.handle;
            headers = other.headers;
            other.handle = nullptr;
            other.headers = nullptr;
        }
        return *this;
    }

    ~CurlRequest() {
        cleanup();
    }

private:
    void cleanup()
    {
        if (handle) {
            curl_easy_cleanup(handle);
            handle = nullptr;
        }
        if (headers) {
            curl_slist_free_all(headers);
            headers = nullptr;
        }
    }
};

struct HttpResponseInfo {
    long status_code{0};
    std::string retry_after;
};

size_t HeaderCallback(char* buffer, size_t size, size_t nitems, std::string* retry_after)
{
    const size_t total_size = size * nitems;
    std::string line(buffer, total_size);
    const std::string prefix = "retry-after:";
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (lower.rfind(prefix, 0) == 0) {
        *retry_after = trim_ws(line.substr(prefix.size()));
    }
    return total_size;
}

CurlRequest create_curl_request(const std::shared_ptr<spdlog::logger>& logger)
{
    CurlRequest request;
    request.handle = curl_easy_init();
    if (!request.handle) {
        if (logger) {
            logger->critical("Failed to initialize cURL handle for remote request");
        }
        throw std::runtime_error("Initialization Error: Failed to initialize cURL.");
    }

#ifdef _WIN32
    try {
        const auto cert_path = Utils::ensure_ca_bundle();
        curl_easy_setopt(request.handle, CURLOPT_CAINFO, cert_path.string().c_str());
    } catch (const std::exception& ex) {
        throw std::runtime_error(std::string("Failed to stage CA bundle: ") + ex.what());
    }
#endif
    return request;
}

void configure_request_payload(CurlRequest& request,
                               const std::string& api_url,
                               const std::string& payload,
                               const std::string& api_key,
                               long timeout_seconds,
                               std::string& response_buffer,
                               std::string& retry_after_header)
{
    curl_easy_setopt(request.handle, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(request.handle, CURLOPT_POST, 1L);
    curl_easy_setopt(request.handle, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(request.handle, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(request.handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(request.handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(request.handle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(request.handle, CURLOPT_TCP_KEEPIDLE, 30L);
    curl_easy_setopt(request.handle, CURLOPT_TCP_KEEPINTVL, 15L);
    request.error_buffer.fill('\0');
    curl_easy_setopt(request.handle, CURLOPT_ERRORBUFFER, request.error_buffer.data());

    request.headers = curl_slist_append(request.headers, "Content-Type: application/json");
    if (!api_key.empty()) {
        const std::string auth = "Authorization: Bearer " + api_key;
        request.headers = curl_slist_append(request.headers, auth.c_str());
    }
    curl_easy_setopt(request.handle, CURLOPT_HTTPHEADER, request.headers);

    curl_easy_setopt(request.handle, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(request.handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(request.handle, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(request.handle, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(request.handle, CURLOPT_HEADERDATA, &retry_after_header);
}

bool retryable_transport_error(CURLcode code)
{
    return code == CURLE_RECV_ERROR ||
           code == CURLE_SEND_ERROR ||
           code == CURLE_GOT_NOTHING ||
           code == CURLE_PARTIAL_FILE;
}

HttpResponseInfo perform_request(CurlRequest& request,
                                 std::string& response_buffer,
                                 std::string& retry_after_header,
                                 const std::shared_ptr<spdlog::logger>& logger)
{
    constexpr int kMaxAttempts = 2;
    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
        request.error_buffer.fill('\0');
        const CURLcode res = curl_easy_perform(request.handle);
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(request.handle, CURLINFO_RESPONSE_CODE, &http_code);
            return HttpResponseInfo{http_code, retry_after_header};
        }

        const std::string detail = request.error_buffer[0] != '\0'
            ? std::string(request.error_buffer.data())
            : std::string(curl_easy_strerror(res));

        if (logger) {
            logger->error("cURL request attempt {}/{} failed: {}",
                          attempt,
                          kMaxAttempts,
                          detail);
        }

        if (attempt < kMaxAttempts && retryable_transport_error(res)) {
            if (logger) {
                logger->warn("Retrying transient remote LLM transport failure once.");
            }
            response_buffer.clear();
            retry_after_header.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
            continue;
        }

        std::string message = "Network Error: " + detail;
        if (res == CURLE_RECV_ERROR || res == CURLE_GOT_NOTHING) {
            message +=
                ". The model server closed the connection before a complete response was received. "
                "Check the model server/reverse-proxy logs, available memory, and request timeout.";
        }
        throw std::runtime_error(message);
    }

    throw std::runtime_error("Network Error: remote request failed.");
}

std::string text_from_json_value(const Json::Value& value)
{
    if (value.isString()) {
        return trim_ws(value.asString());
    }
    if (value.isArray()) {
        std::string combined;
        for (const auto& part : value) {
            if (part.isString()) {
                combined += part.asString();
                continue;
            }
            if (!part.isObject()) {
                continue;
            }
            for (const char* key : {"text", "content", "output_text"}) {
                if (part[key].isString()) {
                    combined += part[key].asString();
                    break;
                }
            }
        }
        return trim_ws(combined);
    }
    if (value.isObject()) {
        for (const char* key : {"text", "content", "output_text"}) {
            if (value[key].isString()) {
                const std::string text = trim_ws(value[key].asString());
                if (!text.empty()) {
                    return text;
                }
            }
        }
    }
    return {};
}

std::string parse_category_response(const std::string& payload,
                                    const std::shared_ptr<spdlog::logger>& logger)
{
    Json::CharReaderBuilder reader_builder;
    Json::Value root;
    std::istringstream response_stream(payload);
    std::string errors;

    if (!Json::parseFromStream(reader_builder, response_stream, &root, &errors)) {
        if (logger) {
            logger->error("Failed to parse JSON response: {}", errors);
        }
        throw std::runtime_error("Response Error: Failed to parse JSON response. " + errors);
    }

    std::string finish_reason;
    const Json::Value& choices = root["choices"];
    if (choices.isArray() && !choices.empty()) {
        const Json::Value& choice = choices[0];
        if (choice["finish_reason"].isString()) {
            finish_reason = choice["finish_reason"].asString();
        }

        const Json::Value& message = choice["message"];
        if (message.isObject()) {
            const std::string content = text_from_json_value(message["content"]);
            if (!content.empty()) {
                return content;
            }

            for (const char* key : {"reasoning_content", "reasoning", "analysis", "thinking"}) {
                const std::string reasoning = text_from_json_value(message[key]);
                if (!reasoning.empty()) {
                    if (logger) {
                        logger->warn(
                            "Remote LLM returned empty final content; using '{}' fallback.",
                            key);
                    }
                    return reasoning;
                }
            }
        }

        for (const char* key : {"text", "content", "output_text"}) {
            const std::string text = text_from_json_value(choice[key]);
            if (!text.empty()) {
                return text;
            }
        }
    }

    for (const char* key : {"response", "output_text", "text", "content"}) {
        const std::string text = text_from_json_value(root[key]);
        if (!text.empty()) {
            return text;
        }
    }

    if (root["message"].isObject()) {
        const std::string text = text_from_json_value(root["message"]["content"]);
        if (!text.empty()) {
            return text;
        }
    }

    if (root["output"].isArray()) {
        for (const auto& output_item : root["output"]) {
            if (!output_item.isObject()) {
                continue;
            }
            const std::string direct = text_from_json_value(output_item);
            if (!direct.empty()) {
                return direct;
            }
            const std::string nested = text_from_json_value(output_item["content"]);
            if (!nested.empty()) {
                return nested;
            }
        }
    }

    if (logger) {
        logger->error("Remote LLM response contained no usable text content. Raw envelope: {}", payload);
    }

    if (finish_reason == "length" || finish_reason == "max_tokens") {
        throw std::runtime_error(
            "Response Error: The model used the completion budget before producing final text "
            "(finish_reason=" + finish_reason +
            "). This commonly happens with reasoning/thinking models. "
            "Use a larger output budget or disable thinking for this request.");
    }

    std::string suffix;
    if (!finish_reason.empty()) {
        suffix = " finish_reason=" + finish_reason + ".";
    }
    throw std::runtime_error(
        "Response Error: The model server returned a successful response but no usable text content." + suffix +
        " The endpoint may be using a non-standard OpenAI-compatible response schema.");
}
}


LLMClient::LLMClient(std::string api_key,
                     std::string model,
                     std::string base_url,
                     long timeout_override_seconds)
    : api_key(std::move(api_key)),
      model(std::move(model)),
      base_url(std::move(base_url)),
      timeout_override_seconds_(timeout_override_seconds)
{}


LLMClient::~LLMClient() = default;


void LLMClient::set_prompt_logging_enabled(bool enabled)
{
    prompt_logging_enabled = enabled;
}


std::string LLMClient::send_api_request(std::string json_payload) {
    std::string response_string;
    std::string retry_after_header;
    const std::string api_url = resolve_api_url();
    auto logger = Logger::get_logger("core_logger");

    if (logger) {
        logger->debug("Dispatching remote LLM request to {}", api_url);
    }

    CurlRequest request = create_curl_request(logger);
    configure_request_payload(request,
                              api_url,
                              json_payload,
                              api_key,
                              timeout_override_seconds_ > 0
                                  ? timeout_override_seconds_
                                  : resolve_timeout_seconds(base_url),
                              response_string,
                              retry_after_header);

    const HttpResponseInfo response =
        perform_request(request, response_string, retry_after_header, logger);
    if (response.status_code >= 400) {
        RemoteApiError::throw_for_http_error("Remote LLM",
                                             response.status_code,
                                             response_string,
                                             response.retry_after,
                                             logger);
    }
    return parse_category_response(response_string, logger);
}

std::string LLMClient::effective_model() const
{
    return model.empty() ? "gpt-4o-mini" : model;
}

std::string LLMClient::resolve_api_url() const
{
    static const std::string kDefaultApi = "https://api.openai.com/v1/chat/completions";
    static const std::string kChatSuffix = "/chat/completions";

    if (base_url.empty()) {
        return kDefaultApi;
    }

    std::string trimmed = trim_ws(base_url);
    if (trimmed.empty()) {
        return kDefaultApi;
    }

    trimmed = trim_trailing_slashes(trimmed);
    if (ends_with(trimmed, kChatSuffix)) {
        return trimmed;
    }

    return trimmed + kChatSuffix;
}


std::string LLMClient::categorize_file(const std::string& file_name,
                                       const std::string& file_path,
                                       FileType file_type,
                                       const std::string& consistency_context)
{
    if (auto logger = Logger::get_logger("core_logger")) {
        if (!file_path.empty()) {
            logger->debug("Requesting remote categorization for '{}' ({}) at '{}'",
                          file_name, to_string(file_type), file_path);
        } else {
            logger->debug("Requesting remote categorization for '{}' ({})", file_name, to_string(file_type));
        }
    }
    std::string json_payload = make_payload(file_name, file_path, file_type, consistency_context);

    if (prompt_logging_enabled && !last_prompt.empty()) {
        std::cout << "\n[DEV][PROMPT] Categorization request\n" << last_prompt << "\n";
    }

    std::string category = send_api_request(json_payload);

    if (prompt_logging_enabled) {
        std::cout << "[DEV][RESPONSE] Categorization reply\n" << category << "\n";
    }

    return category;
}


std::string LLMClient::make_payload(const std::string& file_name,
                                    const std::string& file_path,
                                    const FileType file_type,
                                    const std::string& consistency_context)
{
    std::string prompt;
    std::string sanitized_path = file_path;

    if (!sanitized_path.empty()) {
        prompt = "Categorize the item with full path: " + sanitized_path + "\n";
        prompt += "File name: " + file_name;
    } else {
        prompt = "Categorize file: " + file_name;
    }

    if (file_type == FileType::File) {
        // already set above
    } else {
        if (!sanitized_path.empty()) {
            prompt = "Categorize the directory with full path: " + sanitized_path + "\nDirectory name: " + file_name;
        } else {
            prompt = "Categorize directory: " + file_name;
        }
    }

    if (!consistency_context.empty()) {
        prompt += "\n\n" + consistency_context;
    }

    last_prompt = prompt;
    const std::string escaped_prompt = escape_json(prompt);
    const std::string default_system_prompt =
        "You are a file categorization assistant. If it's an installer, describe the type of software it installs. "
        "Consider the filename, extension, and any directory context provided. If the user prompt includes an "
        "'Allowed main categories' list, choose the main category from that list only. Use Other only when it is "
        "listed and none of the other listed main categories clearly fits. Always reply with one line in the "
        "format <Main category> : <Subcategory>. Main category must be broad (one or two words, plural). "
        "Subcategory must be specific, relevant, and must not repeat the main category.";
    const std::string system_prompt = PromptTemplateStore::render_or_default(
        default_system_prompt, file_name, file_path, file_type, consistency_context);
    const std::string escaped_system = escape_json(system_prompt);

    std::ostringstream payload;
    payload << "{\n"
            << "    \"model\": \"" << escape_json(effective_model()) << "\",\n"
            << "    \"messages\": [\n"
            << "        {\"role\": \"system\", \"content\": \"" << escaped_system << "\"},\n"
            << "        {\"role\": \"user\", \"content\": \"" << escaped_prompt << "\"}\n"
            << "    ]\n"
            << "}";

    return payload.str();
}

std::string LLMClient::make_generic_payload(const std::string& system_prompt,
                                            const std::string& user_prompt,
                                            int max_tokens) const
{
    std::ostringstream payload;
    payload << "{\"model\": \"" << escape_json(effective_model()) << "\",";
    payload << "\"messages\": [";
    payload << "{\"role\": \"system\", \"content\": \""
            << escape_json(system_prompt) << "\"},";
    payload << "{\"role\": \"user\", \"content\": \""
            << escape_json(user_prompt) << "\"}]";
    if (max_tokens > 0) {
        payload << ",\"max_tokens\": " << max_tokens;
    }
    payload << "}";
    return payload.str();
}

std::string LLMClient::complete_prompt(const std::string& prompt,
                                       int max_tokens)
{
    static const std::string kSystem =
        "You are a precise filesystem organization assistant. "
        "Return only one valid JSON object. Do not include Markdown fences, reasoning, commentary, or text outside the JSON. "
        "The first non-whitespace character must be { and the last must be }.";
    if (prompt_logging_enabled) {
        std::cout << "\n[DEV][PROMPT] Completion request\n"
                  << prompt << "\n";
    }
    std::string json_payload = make_generic_payload(kSystem, prompt, max_tokens);
    std::string response = send_api_request(json_payload);
    if (prompt_logging_enabled) {
        std::cout << "[DEV][RESPONSE] Completion reply\n" << response << "\n";
    }
    return response;
}
