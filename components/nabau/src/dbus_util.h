// SPDX-License-Identifier: BSD-3-Clause
// Small helpers over the toolkit's D-Bus types for Anaconda's structures.

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/dbus.h"

namespace installer::dbus {

using bayu::base::dbus::Variant;
using bayu::base::dbus::VariantMap;

// A key of an a{sv} as a type, or the fallback.
template<typename T>
T get(const VariantMap& map, const std::string& key, T fallback = T())
{
    const auto it = map.find(key);
    if (it == map.end())
        return fallback;
    return it->second.get<T>().value_or(fallback);
}

// The message of a failed call, without the error's name.
inline std::string errorText(const bayu::base::dbus::Reply& reply)
{
    if (!reply.isError())
        return {};
    const std::string& message = reply.error().message;
    return message.empty() ? reply.error().name : message;
}

} // namespace installer::dbus
