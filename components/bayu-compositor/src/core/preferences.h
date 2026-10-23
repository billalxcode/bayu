// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_CORE_PREFERENCES_H_
#define BAYU_CORE_PREFERENCES_H_

#include <string>
#include <string_view>

namespace bayu {

// ~/.config/bayu, where every preference file of the session lives.
std::string preferencesDirectory();

// ~/.config/bayu/<process>.conf. Each file has one writer, the process it
// is named after; the compositor writes "compositor" and only reads the others.
std::string preferencesFile(std::string_view process);

// Tests: preferences under `configHome`/bayu instead of the user's;
// empty restores the user's. The test main sets a scratch one for every test.
void setConfigHomeForTesting(std::string configHome);

} // namespace bayu

#endif // BAYU_CORE_PREFERENCES_H_
