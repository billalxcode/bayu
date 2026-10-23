// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>
#include <string_view>

// The SHA-1 digest of the bytes, in lower-case hex.
std::string sha1Hex(std::string_view data);
