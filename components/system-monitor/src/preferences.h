// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

// The monitor's choices in ~/.config/bayu/system-monitor.conf.
struct Preferences
{
    // Milliseconds.
    int interval = 1000;
    int scope = 1;
    std::string sortColumn = "cpu";
    bool sortAscending = false;

    // Defaults for what the file does not hold.
    static Preferences load(const std::string& path);
    // Keeps whatever else the file holds.
    void save(const std::string& path) const;
    static std::string defaultPath();
};
