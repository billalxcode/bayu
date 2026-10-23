// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

// Moves a file or directory into the user's trash, as the freedesktop.org
// trash specification has it, so it can be put back.
bool moveToTrash(const std::string& path);
