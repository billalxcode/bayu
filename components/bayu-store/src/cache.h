// SPDX-License-Identifier: BSD-3-Clause
// The catalog kept on disk between runs (~/.cache/bayu-store), so the store
// opens at once and offline: one app per line, as JSON.

#pragma once

#include <string>
#include <vector>

#include "model.h"

namespace store::cache {

// ~/.cache/bayu-store, made if missing.
std::string directory();

bool save(const std::string& path, const std::vector<App>& apps);
std::vector<App> load(const std::string& path);

// A file in the cache for a URL: icons, screenshots, APKs.
std::string fileFor(const std::string& url, const std::string& folder);

} // namespace store::cache
