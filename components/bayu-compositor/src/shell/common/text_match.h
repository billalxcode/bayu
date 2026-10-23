// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_SHELL_COMMON_TEXT_MATCH_H_
#define BAYU_SHELL_COMMON_TEXT_MATCH_H_

#include <string>
#include <string_view>

namespace bayu::shell {

// Case-insensitive search: each character compared by its simple Unicode case
// folding, so "É" finds "é" and "Σ" finds "ς".
bool containsIgnoringCase(std::string_view text, std::string_view needle);

// Unicode lower case, not only ASCII.
std::string toLower(std::string_view text);

} // namespace bayu::shell

#endif // BAYU_SHELL_COMMON_TEXT_MATCH_H_
