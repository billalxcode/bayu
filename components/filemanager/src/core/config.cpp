// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "core/config.h"

#include "base/paths.h"

namespace fm {

static std::string& overridden()
{
    static std::string path;
    return path;
}

std::string settingsFile()
{
    return overridden().empty() ? bayu::base::paths::bayuConfig("filemanager") : overridden();
}

void setSettingsFile(std::string path)
{
    overridden() = std::move(path);
}

} // namespace fm
