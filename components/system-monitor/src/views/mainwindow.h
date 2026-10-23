// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "processespage.h"

#include "base/signal.h"
#include "ui/menu.h"
#include "ui/toast.h"
#include "ui/window.h"

#include <memory>

class Monitor;
class ProcessDetails;
class ResourcePage;

namespace bayu::ui {
class ConfirmDialog;
}

// The system monitor's window: the resources in a sidebar, the process list
// or a resource's page beside it.
class MainWindow
{
public:
    explicit MainWindow(Monitor& monitor);
    ~MainWindow();

    bayu::ui::Window& window() { return m_window; }
    int currentPage() const { return m_page; }
    void setCurrentPage(int page);
    // Shows, raises and activates the window.
    void activate();
    void openDetails();
    void confirmEnd(bool force);
    // The selected process or application, while it runs.
    const ProcessRecord* selectedProcess() const;
    ProcessesPage* processesPage() const { return m_processes; }
    ResourcePage* resourcePage() const { return m_resource; }
    ProcessDetails* details() const { return m_details.get(); }
    bayu::ui::Menu& contextMenu() { return m_contextMenu; }
    bayu::ui::Menu& intervalMenu() { return m_intervalMenu; }
    bayu::ui::ConfirmDialog* confirmDialog() const { return m_confirm.get(); }

private:
    class Root;

    // The views and menus, all of which show text.
    void buildContent();
    void retranslate();
    void update();
    void updateContextMenu();

    Monitor& m_monitor;
    bayu::ui::Window m_window;
    // Errors from ending processes, as FishUI's passive notification.
    bayu::ui::Toast m_toast;
    Root* m_root;
    Selection m_selection;
    int m_page = 0;
    ProcessesPage* m_processes;
    ResourcePage* m_resource = nullptr;
    bayu::ui::Menu m_intervalMenu;
    bayu::ui::Menu m_contextMenu;
    std::unique_ptr<bayu::ui::ConfirmDialog> m_confirm;
    std::unique_ptr<ProcessDetails> m_details;
    bayu::base::Subscription m_updated;
    bayu::base::Subscription m_intervalChanged;
    bayu::base::Subscription m_languageChanged;
};
