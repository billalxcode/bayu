// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "preferences.h"

#include "base/paths.h"
#include "base/settings.h"

#include <algorithm>

using bayu::base::Settings;

Preferences Preferences::load(const std::string& path)
{
    Preferences preferences;
    const Settings settings(path);
    const int64_t interval = settings.intValue("UpdateInterval", 1000);
    preferences.interval = interval == 2000 || interval == 5000 ? int(interval) : 1000;
    preferences.scope = int(std::clamp<int64_t>(settings.intValue("Processes/Scope", preferences.scope), 0, 2));
    preferences.sortColumn = settings.stringValue("Processes/SortColumn", preferences.sortColumn);
    preferences.sortAscending = settings.boolValue("Processes/SortAscending", preferences.sortAscending);
    return preferences;
}

void Preferences::save(const std::string& path) const
{
    Settings settings(path);
    settings.setValue("UpdateInterval", interval);
    settings.setValue("Processes/Scope", scope);
    settings.setValue("Processes/SortColumn", sortColumn);
    settings.setValue("Processes/SortAscending", sortAscending);
}

std::string Preferences::defaultPath()
{
    return bayu::base::paths::bayuConfig("system-monitor");
}
