// SPDX-License-Identifier: BSD-3-Clause

#ifndef BAYU_SHELL_STATUSBAR_PANEL_TASKS_H_
#define BAYU_SHELL_STATUSBAR_PANEL_TASKS_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "base/signal.h"
#include "ui/view.h"

namespace bayu::shell {

class DockModel;
class TaskWindows;

// The panel's left side, in the panel layout (Shell/Layout): the Start
// button, the dock's pinned applications, then a button for each window of
// the task list, the active one selected. Pitaloka's metrics and colours.
class PanelTasks : public ui::View
{
public:
    PanelTasks(TaskWindows &windows, DockModel &applications);
    ~PanelTasks() override;

    // The Start button.
    std::function<void()> onStart;
    // A pinned application or a window was clicked, so the bar's popups close.
    std::function<void()> onActivated;

    // The tile the icons sit on: Panel/IconSize floating, 4 less docked.
    void setTileSize(double size);
    // Where each window's button shows its icon, in the window's coordinates.
    std::map<uint64_t, ui::RectF> windowTargets() const;
    // The buttons moved.
    base::Signal<> targetsChanged;

    // The pinned applications and the windows, for tests.
    int pinCount() const { return int(m_pins.size()); }
    std::vector<uint64_t> windowButtons() const;

protected:
    void layout() override;
    void paint(ui::Painter &painter) override;

private:
    class Button;
    class StartButton;
    class PinButton;
    class WindowButton;

    void rebuildPins();
    void rebuildWindows();
    void updateWindow(uint64_t id);
    bool listed(uint64_t id) const;

    TaskWindows &m_windows;
    DockModel &m_applications;
    double m_tileSize = 36;
    StartButton *m_start = nullptr;
    std::vector<PinButton *> m_pins;
    std::vector<WindowButton *> m_windowButtons;
    // Where the dividers are drawn, after the Start button and the pins.
    std::vector<double> m_dividers;
    std::vector<base::Subscription> m_subscriptions;
};

} // namespace bayu::shell

#endif // BAYU_SHELL_STATUSBAR_PANEL_TASKS_H_
