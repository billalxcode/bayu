// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "download.h"
#include "text.h"

#include <curl/curl.h>

#include <memory>
#include <mutex>
#include <strings.h>

// Far more than any published calendar needs.
static const size_t MaximumSize = 32 * 1024 * 1024;

namespace {

struct Transfer
{
    Download* result;
    bool tooLarge = false;
};

}

static size_t writeBody(char* data, size_t size, size_t count, void* user)
{
    auto* transfer = static_cast<Transfer*>(user);
    const size_t bytes = size * count;
    if (transfer->result->body.size() + bytes > MaximumSize) {
        transfer->tooLarge = true;
        return 0;
    }
    transfer->result->body.append(data, bytes);
    return bytes;
}

// Headers arrive once per response, redirects included; the last response's win.
static size_t readHeader(char* data, size_t size, size_t count, void* user)
{
    auto* transfer = static_cast<Transfer*>(user);
    const size_t bytes = size * count;
    const std::string_view line(data, bytes);
    if (line.starts_with("HTTP/")) {
        transfer->result->etag.clear();
        transfer->result->lastModified.clear();
        return bytes;
    }
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos)
        return bytes;
    const std::string name(line.substr(0, colon));
    const std::string value = trimmed(line.substr(colon + 1));
    if (strcasecmp(name.c_str(), "ETag") == 0)
        transfer->result->etag = value;
    else if (strcasecmp(name.c_str(), "Last-Modified") == 0)
        transfer->result->lastModified = value;
    return bytes;
}

Download download(const std::string& url, const std::string& etag, const std::string& lastModified)
{
    static std::once_flag initialized;
    std::call_once(initialized, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    Download result;
    const std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) {
        result.error = "The download could not start.";
        return result;
    }

    curl_slist* headers = nullptr;
    if (!etag.empty())
        headers = curl_slist_append(headers, ("If-None-Match: " + etag).c_str());
    if (!lastModified.empty())
        headers = curl_slist_append(headers, ("If-Modified-Since: " + lastModified).c_str());
    const std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headerList(headers, curl_slist_free_all);

    Transfer transfer{&result};
    char errorText[CURL_ERROR_SIZE] = {};
    CURL* handle = curl.get();
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);
    // Whatever compression the library can undo.
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, 60L);
    // Name resolution may not raise signals on a worker thread.
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "bayu-calendar/1.0");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headerList.get());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, readHeader);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &transfer);
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errorText);

    const CURLcode code = curl_easy_perform(handle);
    if (code != CURLE_OK) {
        result.status = 0;
        result.body.clear();
        if (transfer.tooLarge)
            result.error = "The calendar is too large.";
        else
            result.error = errorText[0] ? std::string(errorText) : std::string(curl_easy_strerror(code));
        return result;
    }
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &result.status);
    return result;
}
