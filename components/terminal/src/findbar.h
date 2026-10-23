// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>

#include "base/signal.h"
#include "ui/view.h"

namespace Konsole {
class TerminalView;
}

namespace bayu::ui {
class HeaderButton;
}

class FindField;

// Floats over a terminal while searching its output and history. Enter goes to
// the next older match, Shift+Enter to the next newer one. Drawn in the
// terminal's colours; the field itself has no frame, only the bar does.
class FindBar : public bayu::ui::View
{
public:
    explicit FindBar(Konsole::TerminalView* terminal);
    ~FindBar() override;

    // The colour scheme's own colours.
    void setColors(bayu::ui::Color background, bayu::ui::Color text, bool darkMode,
                   bayu::ui::Color error, bayu::ui::Color accent);

    void open(const std::string& text);
    void close();
    void retranslate();

    std::function<void()> onClosed;

    bayu::ui::SizeF sizeHint() const override { return {320, 40}; }
    bool acceptsPointer() const override { return true; }

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    void search(bool forwards, bool fromSelection);
    void updateColors();

    Konsole::TerminalView* m_terminal;
    FindField* m_field;
    bayu::ui::HeaderButton* m_previous;
    bayu::ui::HeaderButton* m_next;
    bayu::ui::HeaderButton* m_close;
    bayu::base::Subscription m_findResult;
    bool m_notFound = false;
    bayu::ui::Color m_background = bayu::ui::Color::rgb(0x1A1B26);
    bayu::ui::Color m_text = bayu::ui::Color::rgb(0xC0CAF5);
    bayu::ui::Color m_error = bayu::ui::Color::rgb(0xF7768E);
    bayu::ui::Color m_accent = bayu::ui::Color::rgb(0x7AA2F7);
    bool m_darkMode = true;
};
