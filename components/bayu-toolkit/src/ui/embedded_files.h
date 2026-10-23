// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>

namespace bayu::ui {

// Files built into the library, such as the glyph icons under data/icons.
struct EmbeddedFile
{
    const char* name;
    const unsigned char* data;
    size_t size;
};

const EmbeddedFile* findEmbeddedFile(const char* name);

} // namespace bayu::ui
