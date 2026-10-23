// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>
#include <string_view>

// Compares as the user's language sorts, < 0, 0 or > 0.
int localeCompare(std::string_view a, std::string_view b);
// As localeCompare, ignoring case and reading digits as numbers ("Item 9" before "Item 10").
int naturalCompare(std::string_view a, std::string_view b);

// Whether the text holds the needle, ignoring case.
bool containsIgnoringCase(std::string_view text, std::string_view needle);

// Without white space at either end.
std::string trimmed(std::string_view text);
