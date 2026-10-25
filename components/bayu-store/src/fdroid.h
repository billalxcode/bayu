// SPDX-License-Identifier: BSD-3-Clause
// F-Droid's catalog: its index-v2.json, read as a stream (it is tens of
// megabytes), each package's newest version that runs here.

#pragma once

#include <string>
#include <vector>

#include "model.h"

namespace store::fdroid {

inline constexpr const char* kRepo = "https://f-droid.org/repo";
// LineageOS 20 in lapis.
inline constexpr int kSdk = 33;

// The apps in an index; `locale` like "id" or "en-US", the texts' first
// choice. ARM-only apps are marked, not dropped.
std::vector<App> parseIndex(const std::string& json, const std::string& locale, std::string* error);

} // namespace store::fdroid
