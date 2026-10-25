// SPDX-License-Identifier: GPL-3.0-or-later
// Flathub's catalog, through its web API: the apps (with installs, ratings
// and verification Flatpak's own metadata lacks) and each app's details.
// Blocking: call from a worker thread.

#pragma once

#include <string>
#include <vector>

#include "model.h"

namespace store::flathub {

inline constexpr const char* kApi = "https://flathub.org/api/v2";

std::vector<App> fetchCatalog(std::string* error);

// Screenshots, release notes, sizes, permissions and all-time installs.
void fetchDetails(App& app);

// Today's app and this week's, newest first.
std::vector<std::string> fetchFeatured();

} // namespace store::flathub
