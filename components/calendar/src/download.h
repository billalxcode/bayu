// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

struct Download
{
    // The HTTP status; 0 when no answer came.
    long status = 0;
    std::string body;
    // Validators to send with the next request for the same address.
    std::string etag;
    std::string lastModified;
    // Why no answer came.
    std::string error;

    bool notModified() const { return status == 304; }
    bool succeeded() const { return status >= 200 && status < 300; }
};

// An HTTP or HTTPS GET, blocking until it is done: for a worker thread.
// With validators the server may answer 304 and send nothing.
Download download(const std::string& url, const std::string& etag = {}, const std::string& lastModified = {});
