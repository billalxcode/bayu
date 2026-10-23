#pragma once

#include "base/dbus.h"

#include <cstdint>
#include <string>

class SettingsPortal
{
public:
    // `servicesConfig` is bayu-services' settings file, read once for
    // the dark mode until the live value arrives.
    SettingsPortal(bayu::base::dbus::Connection *bus, const std::string &servicesConfig);

    bool publish();

    uint32_t version() const { return 1; }

private:
    void readAll(bayu::base::dbus::MethodCall &call) const;
    void read(bayu::base::dbus::MethodCall &call) const;
    void setDarkMode(bool darkMode);
    void refreshAppearance();

    bayu::base::dbus::Connection *m_bus;
    bayu::base::dbus::Object m_object;
    bayu::base::dbus::Proxy m_appearance;
    bayu::base::dbus::Match m_darkModeChanged;
    bayu::base::dbus::NameWatch m_appearanceOwner;
    bayu::base::dbus::PendingCall m_refresh;
    uint32_t m_colorScheme = 2;
};
