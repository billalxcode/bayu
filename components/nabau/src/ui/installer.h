// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "backend.h"
#include "base/signal.h"
#include "model.h"

namespace bayu::ui {
class ConfirmDialog;
class Row;
class DialogWindow;
class Window;
}

namespace installer {

class Page;
class Root;

// What the user chose so far, shared by the pages.
struct Choices
{
    std::string language;
    std::string locale;
    std::string keyboardLayout;
    TimeSettings time;
    std::string hostname;
    std::string connection;
    StoragePlan storage;
    std::vector<LayoutPart> layout;
    std::string environment;
    std::vector<std::string> addons;
    Account account;
    // Disks the plan erases, for the review's warning.
    std::vector<std::string> erasedDisks;
};

// The installer's window and its steps: the sidebar, the page shown, and
// the footer that moves between pages.
class Installer
{
public:
    explicit Installer(std::unique_ptr<Backend> backend);
    ~Installer();

    Backend& backend() const { return *m_backend; }
    Choices& choices() { return m_choices; }
    bayu::ui::Window* window() const { return m_window.get(); }

    // Waits for the backend, then shows the first page.
    void start();
    void show();

    size_t pageCount() const { return m_pages.size(); }
    size_t currentIndex() const { return m_current; }
    Page* page(size_t index) const;
    // The footer's row for a page that owns the footer (ownsFooter()).
    bayu::ui::Row* footerActions() const;
    bool isDone(size_t index) const;

    // Commits the current page and goes to the next.
    void next();
    void back();
    // To a page done before, or the next one.
    void goTo(size_t index);
    // Review's Edit: back to a page, returning to Review after it.
    void edit(size_t index);

    void showHelp();
    void openConsole();
    void showLog();
    void requestQuit();
    // Every page done: the sidebar shows the installation as complete.
    void setFinished(bool finished);
    bool isFinished() const { return m_finished; }

    // Renders every page into PNGs, for screenshots of the design.
    int capture(const std::string& directory);

    // A page changed its state.
    void pageChanged(Page* page);

    // The installer in the language chosen in step 01: its pages are made
    // again in it, after the step that chose it has moved on.
    void setLanguage(const std::string& locale);

private:
    // The window's content, once the backend has started.
    void build();
    // The sidebar, the pages and the footer, in the current language.
    void buildRoot();
    void show(size_t index);
    void updateChrome();

    std::unique_ptr<Backend> m_backend;
    Choices m_choices;
    std::unique_ptr<bayu::ui::Window> m_window;
    Root* m_root = nullptr;
    std::vector<Page*> m_pages;
    std::vector<std::unique_ptr<Page>> m_owned;
    size_t m_current = 0;
    // Pages passed with Continue.
    size_t m_reached = 0;
    // Edit from Review returns there.
    bool m_editing = false;
    bool m_busy = false;
    bool m_finished = false;
    std::unique_ptr<bayu::ui::ConfirmDialog> m_confirm;
    std::unique_ptr<bayu::ui::DialogWindow> m_dialog;
    bayu::base::Subscription m_languageChanged;
};

} // namespace installer
