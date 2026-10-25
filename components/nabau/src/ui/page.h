// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>

#include "ui/layout.h"

namespace installer {

class Installer;

// One step of the installer: its heading and its body. The window puts the
// step's eyebrow, title and description above the body, and the footer's
// buttons below it.
class Page : public bayu::ui::Column
{
public:
    explicit Page(Installer& installer)
        : m_installer(installer)
    {
    }

    // "Language", in the sidebar.
    virtual std::string stepName() const = 0;
    // "LANGUAGE" after "STEP 01 —"; the step name unless set.
    virtual std::string eyebrow() const { return {}; }
    virtual std::string title() const = 0;
    virtual std::string description() const = 0;
    // What was chosen, under the step's name once it is done: "English (US)".
    virtual std::string summary() const { return {}; }

    // The page is about to show.
    virtual void enter() {}
    // Continue is enabled.
    virtual bool canContinue() const { return true; }
    // Continue's text when the page wants another: "Begin installation".
    virtual std::string continueText() const { return {}; }
    virtual std::string continueGlyph() const { return "arrow-right"; }
    // Applies the page's choices before the next page; may wait for the
    // backend. done(false) stays on the page.
    virtual void commit(std::function<void(bool ok)> done) { done(true); }
    // Back inside the page, such as out of a sub-view; false when the
    // window should go to the previous page.
    virtual bool handleBack() { return false; }
    // The window may go back from this page.
    virtual bool canGoBack() const { return true; }
    // A page that runs on its own (the installation) shows its own footer
    // buttons instead of Back and Continue.
    virtual bool ownsFooter() const { return false; }
    // Review: Edit links come back here.
    virtual bool isReview() const { return false; }
    // Help shown for F1.
    virtual std::string help() const { return description(); }

    // The page's state changed: Continue, the summary or the footer.
    std::function<void()> onChanged;

protected:
    Installer& installer() const { return m_installer; }
    void changed()
    {
        if (onChanged)
            onChanged();
    }

private:
    Installer& m_installer;
};

} // namespace installer
