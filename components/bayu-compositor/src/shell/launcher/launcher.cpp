// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shell/launcher/launcher.h"

#include <algorithm>

#include "base/logging.h"
#include "shell/common/task_manager.h"
#include "shell/common/task_windows.h"
#include "shell/dock/dock.h"

namespace bayu::shell {

static const base::LogCategory cfLauncher("bayu.shell.launcher");

Launcher::Launcher(base::dbus::Connection *bus, TaskManager &tasks, ApplicationLauncher &launcher, Dock *dock,
                   std::string settingsPath)
    : m_bus(bus)
    , m_tasks(tasks)
    , m_dock(dock)
    , m_applications(tasks.applicationSource(), launcher, std::move(settingsPath))
{
    const auto note = [this](const std::string &application) { noteApplicationOpened(application); };
    m_subscriptions.push_back(m_tasks.applicationAdded.connect(note));
    // A window can open before the registry has its new entry.
    m_subscriptions.push_back(m_tasks.applicationChanged.connect(note));
    const auto noteRunning = [this] {
        for (const std::string &application : std::vector<std::string>(m_tasks.applications()))
            noteApplicationOpened(application);
    };
    m_subscriptions.push_back(m_tasks.readyChanged.connect(noteRunning));
    if (m_tasks.isReady())
        noteRunning();

    m_applications.restoreRunning = [this](const std::string &desktopId) {
        const std::vector<uint64_t> windows = m_tasks.windows(desktopId);
        TaskWindows &taskWindows = m_tasks.taskWindows();
        for (uint64_t id : windows) {
            const TaskWindow *window = taskWindows.window(id);
            if (window && !window->minimized && !window->hidden)
                return false;
        }
        if (windows.empty())
            return false;
        // Activating restores the window, the last the application opened.
        taskWindows.activate(windows.back());
        return true;
    };

    // Opened applications lose their new mark.
    m_subscriptions.push_back(applicationOpened.connect(
            [this](const std::string &desktopId) { m_applications.clearNewInstalled({desktopId}); }));
}

Launcher::~Launcher()
{
    if (m_object && m_bus)
        m_bus->releaseName("org.bayu.Launcher");
}

bool Launcher::registerService()
{
    if (m_object)
        return true;
    if (!m_bus)
        return false;
    auto object = std::make_unique<base::dbus::Object>(m_bus, "/Launcher", "org.bayu.Launcher");
    object->addMethod("show", [this] { show(); });
    object->addMethod("hide", [this] { hide(); });
    object->addMethod("toggle", [this] { toggle(); });
    if (!object->publish()
        || !m_bus->requestName("org.bayu.Launcher", base::dbus::Connection::DoNotQueue)) {
        CF_WARNING(cfLauncher) << "Cannot own org.bayu.Launcher";
        return false;
    }
    m_object = std::move(object);
    return true;
}

void Launcher::show()
{
    m_shown = true;
    if (showWindow)
        showWindow();
}

void Launcher::hide()
{
    m_shown = false;
    setDragging(false);
    if (hideWindow)
        hideWindow();
}

void Launcher::toggle()
{
    m_shown ? hide() : show();
}

void Launcher::deactivated()
{
    if (m_shown && !m_dragging)
        hide();
}

bool Launcher::isPinnedInDock(const std::string &desktop) const
{
    return m_dock && m_dock->applications().isDesktopPinned(desktop);
}

void Launcher::sendToDock(const std::string &desktop)
{
    if (m_dock)
        m_dock->applications().addItem(desktop);
}

void Launcher::removeFromDock(const std::string &desktop)
{
    if (m_dock)
        m_dock->applications().removeItem(desktop);
}

void Launcher::noteApplicationOpened(const std::string &application)
{
    const DesktopEntryPtr entry = m_tasks.entry(application);
    if (!entry)
        return;
    const std::string &desktopId = entry->id;
    if (!m_openedTaken
        && std::find(m_openedApplications.begin(), m_openedApplications.end(), desktopId)
                == m_openedApplications.end())
        m_openedApplications.push_back(desktopId);
    applicationOpened(desktopId);
}

std::vector<std::string> Launcher::takeOpenedApplications()
{
    m_openedTaken = true;
    return std::exchange(m_openedApplications, {});
}

void Launcher::setDragging(bool dragging)
{
    m_dragging = dragging;
}

} // namespace bayu::shell
