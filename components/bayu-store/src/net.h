// SPDX-License-Identifier: BSD-3-Clause
// HTTP for the catalog and downloads (libcurl). Blocking: call from a worker
// thread.

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace store::net {

struct Response
{
    std::string body;
    std::string error;
    long status = 0;
    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

Response get(const std::string& url, long timeoutSeconds = 60);
Response post(const std::string& url, const std::string& json, long timeoutSeconds = 60);

// Downloads to a file, resuming a partial one. progress(done, total)
// returns false to stop (pause or cancel). An error text, empty on success.
std::string download(const std::string& url, const std::string& path,
                     std::function<bool(uint64_t done, uint64_t total, uint64_t bytesPerSecond)> progress);

} // namespace store::net
