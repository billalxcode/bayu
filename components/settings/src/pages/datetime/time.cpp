// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pages/datetime/time.h"

#include "base/paths.h"
#include "base/settings.h"
#include "bayu/timedate/time_date.h"

using namespace bayu;
using namespace bayu::base;

namespace settings {

TimeSettings::TimeSettings(timedate::TimeDate& timeDate)
    : m_timeDate(timeDate)
    , m_statusBar(dbus::Connection::session(), "org.bayu.Statusbar", "/Statusbar", "org.bayu.Statusbar")
    , m_canNtp(timeDate.canNtp())
    , m_useNtp(timeDate.ntp())
{
    const Settings shell(paths::bayuConfig("shell"));
    m_twentyFour = shell.boolValue("Statusbar/TwentyFourTime", false);
    m_showSeconds = shell.boolValue("Statusbar/ShowSeconds", false);
    m_timeDateChanged = m_timeDate.changed.connect([this] {
        if (m_timeDate.ntp() != m_useNtp) {
            m_useNtp = m_timeDate.ntp();
            useNtpChanged.emit();
        }
    });
}

void TimeSettings::setUseNtp(bool enabled)
{
    if (m_useNtp == enabled)
        return;
    m_useNtp = enabled;
    m_timeDate.setNtp(enabled, [this, enabled](const std::string& error) {
        // A cancelled or failed polkit prompt leaves timedated as it was.
        if (!error.empty() && m_useNtp == enabled) {
            m_useNtp = !enabled;
            useNtpChanged.emit();
        }
    });
    useNtpChanged.emit();
}

void TimeSettings::setTwentyFour(bool enabled)
{
    if (m_twentyFour == enabled)
        return;
    m_twentyFour = enabled;
    // The status bar keeps the setting; it writes shell.conf itself.
    m_statusBar.send("setTwentyFourTime", enabled);
    twentyFourChanged.emit();
}

void TimeSettings::setShowSeconds(bool enabled)
{
    if (m_showSeconds == enabled)
        return;
    m_showSeconds = enabled;
    m_statusBar.send("setShowSeconds", enabled);
    showSecondsChanged.emit();
}

} // namespace settings
