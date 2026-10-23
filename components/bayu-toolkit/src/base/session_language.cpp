// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "base/session_language.h"

#include <cstdlib>
#include <optional>

#include "base/dbus.h"
#include "base/locale.h"

namespace bayu::base {

static constexpr const char* kService = "org.bayu.Services";
static constexpr const char* kPath = "/org/bayu/Services/Language";
static constexpr const char* kInterface = "org.bayu.Services.Language";

struct SessionLanguage::Private
{
    explicit Private(dbus::Connection* bus)
        : proxy(bus, kService, kPath, kInterface)
    {
    }

    dbus::Proxy proxy;
    dbus::Match match;
    dbus::PendingCall call;
};

SessionLanguage::SessionLanguage(dbus::Connection* bus)
    : d(std::make_unique<Private>(bus))
{
    d->match = d->proxy.onSignal("languageChanged", [this](const dbus::Message&) {
        d->call = d->proxy.getPropertyAsync("environment", [this](const dbus::Reply& reply) {
            const auto value = reply.value<dbus::Variant>();
            if (const auto variables = value ? value->get<std::map<std::string, std::string>>() : std::nullopt) {
                if (apply(*variables))
                    changed.emit();
            }
        });
    });
}

SessionLanguage::~SessionLanguage() = default;

bool SessionLanguage::apply(const std::map<std::string, std::string>& variables)
{
    bool differs = false;
    for (const auto& [name, value] : variables) {
        const char* current = std::getenv(name.c_str());
        if (value.empty() ? current != nullptr : !current || value != current) {
            differs = true;
            break;
        }
    }
    if (!differs)
        return false;
    // Threads reading the environment meanwhile are safe as long as only set
    // variables are replaced: the C library keeps their previous strings.
    for (const auto& [name, value] : variables) {
        if (value.empty())
            unsetenv(name.c_str());
        else
            setenv(name.c_str(), value.c_str(), 1);
    }
    Locale::setProcessLocale();
    Locale::reloadSystem();
    return true;
}

} // namespace bayu::base
