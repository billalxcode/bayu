// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "processespage.h"

#include "base/signal.h"
#include "ui/dialog_window.h"

#include <memory>

class Monitor;

// A process's or application's details, kept current while open.
class ProcessDetails
{
public:
    ProcessDetails(bayu::ui::Window* parent, Monitor& monitor, const Selection& target);

    void show();
    bayu::ui::Window* window() const { return m_dialog.window(); }
    const Selection& target() const { return m_target; }

    std::function<void()> onClosed;

private:
    class Content;

    void update();

    Monitor& m_monitor;
    Selection m_target;
    bayu::ui::DialogWindow m_dialog;
    Content* m_content;
    bayu::base::Subscription m_updated;
};
