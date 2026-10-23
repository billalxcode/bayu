// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "color.h"

#include <cstdio>

static int hexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

std::optional<Rgb> Rgb::fromName(std::string_view name)
{
    if (name.empty() || name.front() != '#')
        return std::nullopt;
    name.remove_prefix(1);
    for (char c : name) {
        if (hexDigit(c) < 0)
            return std::nullopt;
    }
    const size_t digits = name.size() == 3 ? 1 : name.size() == 6 ? 2 : 0;
    if (digits == 0)
        return std::nullopt;
    const auto channel = [&](size_t index) {
        const std::string_view part = name.substr(index * digits, digits);
        int value = hexDigit(part[0]) * 16 + (digits > 1 ? hexDigit(part[1]) : hexDigit(part[0]));
        return uint8_t(value);
    };
    return Rgb{channel(0), channel(1), channel(2)};
}

std::string Rgb::name() const
{
    char text[8];
    std::snprintf(text, sizeof(text), "#%02X%02X%02X", r, g, b);
    return text;
}
