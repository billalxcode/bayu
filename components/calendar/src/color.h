// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// An opaque colour as calendars store it, "#RRGGBB".
struct Rgb
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    // "#rgb" or "#rrggbb".
    static std::optional<Rgb> fromName(std::string_view name);
    // "#RRGGBB" in upper case.
    std::string name() const;

    bool operator==(const Rgb&) const = default;
};
