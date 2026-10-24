// SPDX-License-Identifier: BSD-3-Clause

#include "net.h"

#include <curl/curl.h>
#include <sys/stat.h>

#include <cstdio>
#include <memory>
#include <mutex>

namespace store::net {

namespace {

constexpr const char* kUserAgent = "bayu-store/0.1 (+https://github.com/igos-nusantara/bayu)";

void initialize()
{
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

size_t append(char* data, size_t size, size_t count, void* user)
{
    static_cast<std::string*>(user)->append(data, size * count);
    return size * count;
}

Response perform(const std::string& url, const std::string* json, long timeout)
{
    initialize();
    Response response;
    const std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) {
        response.error = "Cannot start a download.";
        return response;
    }
    curl_slist* headers = nullptr;
    if (json)
        headers = curl_slist_append(headers, "Content-Type: application/json");
    const std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headerList(headers, curl_slist_free_all);
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, kUserAgent);
    curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 20L);
    // A stalled connection is dropped instead of waiting out the timeout.
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 100L);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, append);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response.body);
    if (headers)
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
    if (json)
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, json->c_str());
    const CURLcode code = curl_easy_perform(curl.get());
    if (code != CURLE_OK)
        response.error = curl_easy_strerror(code);
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response.status);
    if (response.error.empty() && (response.status < 200 || response.status >= 300))
        response.error = "The server answered " + std::to_string(response.status) + ".";
    return response;
}

struct Transfer
{
    FILE* file;
    uint64_t offset;
    std::function<bool(uint64_t, uint64_t, uint64_t)>* progress;
    CURL* curl;
};

size_t writeFile(char* data, size_t size, size_t count, void* user)
{
    return fwrite(data, size, count, static_cast<Transfer*>(user)->file) * size;
}

int reportProgress(void* user, curl_off_t total, curl_off_t done, curl_off_t, curl_off_t)
{
    auto* transfer = static_cast<Transfer*>(user);
    curl_off_t speed = 0;
    curl_easy_getinfo(transfer->curl, CURLINFO_SPEED_DOWNLOAD_T, &speed);
    const uint64_t all = total > 0 ? uint64_t(total) + transfer->offset : 0;
    return (*transfer->progress)(transfer->offset + uint64_t(done), all, uint64_t(speed)) ? 0 : 1;
}

} // namespace

Response get(const std::string& url, long timeoutSeconds)
{
    return perform(url, nullptr, timeoutSeconds);
}

Response post(const std::string& url, const std::string& json, long timeoutSeconds)
{
    return perform(url, &json, timeoutSeconds);
}

std::string download(const std::string& url, const std::string& path,
                     std::function<bool(uint64_t, uint64_t, uint64_t)> progress)
{
    initialize();
    struct stat st{};
    const uint64_t offset = ::stat(path.c_str(), &st) == 0 ? uint64_t(st.st_size) : 0;
    FILE* file = fopen(path.c_str(), offset ? "ab" : "wb");
    if (!file)
        return "Cannot write " + path + ".";
    const std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    Transfer transfer{file, offset, &progress, curl.get()};
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, kUserAgent);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeFile);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, reportProgress);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &transfer);
    if (offset)
        curl_easy_setopt(curl.get(), CURLOPT_RESUME_FROM_LARGE, curl_off_t(offset));
    const CURLcode code = curl_easy_perform(curl.get());
    fclose(file);
    if (code == CURLE_OK)
        return {};
    // A finished file asked again from its end: the server refuses the range.
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (status == 416)
        return {};
    return code == CURLE_ABORTED_BY_CALLBACK ? "stopped" : curl_easy_strerror(code);
}

} // namespace store::net
