// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "userdirs.h"

#include <base/file.h>
#include <base/paths.h>

#include <sstream>

std::string documentsDirectory()
{
    const std::string home = bayu::base::paths::home();
    const std::optional<std::string> contents =
        bayu::base::file::read(bayu::base::paths::configHome() + "/user-dirs.dirs");
    if (contents) {
        std::istringstream lines(*contents);
        std::string line;
        while (std::getline(lines, line)) {
            const std::string prefix = "XDG_DOCUMENTS_DIR=\"";
            if (!line.starts_with(prefix) || line.size() < prefix.size() + 1 || line.back() != '"')
                continue;
            std::string path = line.substr(prefix.size(), line.size() - prefix.size() - 1);
            if (path.starts_with("$HOME"))
                path = home + path.substr(5);
            if (path.starts_with('/'))
                return path;
        }
    }
    return home + "/Documents";
}
