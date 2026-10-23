// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/dbus.h"
#include "base/signal.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace bayu::files {
class Task;
}

class DaemonTask;

// org.bayu.FileOperations.Manager1 at /org/bayu/FileOperations: runs
// the file manager's operations so they outlive its windows, with one undo
// history for every client.
class FileOperationsService
{
public:
    explicit FileOperationsService(bayu::base::dbus::Connection *bus);
    ~FileOperationsService();

    bool publish();

private:
    void submit(bayu::base::dbus::MethodCall &call);
    void submitArchive(bayu::base::dbus::MethodCall &call);
    void history(bayu::base::dbus::MethodCall &call, bool undo);
    std::string nextPath();
    // Registers the task and announces it; false when it could not go on the bus.
    bool announce(std::unique_ptr<DaemonTask> task, const std::string &owner);
    void watchOwner(const std::string &owner);
    void ownerLost(const std::string &owner);
    void retire(DaemonTask *task);
    void remove(const std::string &path);

    bayu::base::dbus::Connection *m_bus;
    bayu::base::dbus::Object m_object;
    uint64_t m_nextTaskId = 1;
    std::vector<std::string> m_taskPaths;
    std::vector<std::unique_ptr<DaemonTask>> m_tasks;
    std::map<std::string, bayu::base::dbus::NameWatch> m_owners;
    std::vector<bayu::base::Subscription> m_subscriptions;
    std::map<DaemonTask *, bayu::base::Subscription> m_finishedSubscriptions;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
