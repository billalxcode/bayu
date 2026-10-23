// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef DIALOGS_H
#define DIALOGS_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace bayu::ui {
class DialogWindow;
class Window;
}

class EditorApplication;
class EditorPage;

// Save, Don't Save or Cancel for documents with unsaved changes. Several
// documents are listed with a check box each; Save saves the checked ones.
class SaveChangesDialog
{
public:
    explicit SaveChangesDialog(bayu::ui::Window *parent);
    ~SaveChangesDialog();

    void ask(const std::vector<EditorPage *> &pages, std::function<void()> then);
    bool isOpen() const { return m_window != nullptr; }
    bayu::ui::Window *window() const { return m_window.get(); }
    // The question in the language of the moment, its answers so far kept.
    void retranslate();

    // Saves one page; `done(saved)` follows, at once or after a Save As.
    std::function<void(EditorPage *page, std::function<void(bool saved)> done)> onSaveRequested;

private:
    class Content;

    void buildContent();
    void finish(bool proceed);
    void saveChecked();
    void release();
    // Expired once the page is closed, which another window may do meanwhile.
    std::weak_ptr<bool> lifetimeOf(const EditorPage *page) const;

    bayu::ui::Window *m_parent;
    std::unique_ptr<bayu::ui::Window> m_window;
    std::vector<EditorPage *> m_pages;
    std::vector<std::weak_ptr<bool>> m_lifetimes;
    std::vector<EditorPage *> m_checked;
    std::function<void()> m_action;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

// The global defaults; per-document settings live in the status bar.
class PreferencesDialog
{
public:
    PreferencesDialog(bayu::ui::Window *parent, EditorApplication &application);
    ~PreferencesDialog();

    void show();
    bayu::ui::Window *window() const;

private:
    class Content;

    bayu::ui::Window *m_parent;
    EditorApplication &m_application;
    std::unique_ptr<bayu::ui::DialogWindow> m_dialog;
};

// Every shortcut, by what it does, in four cards.
class ShortcutsDialog
{
public:
    explicit ShortcutsDialog(bayu::ui::Window *parent);
    ~ShortcutsDialog();

    void show();
    bayu::ui::Window *window() const;

private:
    class Content;

    bayu::ui::Window *m_parent;
    std::unique_ptr<bayu::ui::DialogWindow> m_dialog;
};

// The icon, the name, a line and the project's link.
class AboutDialog
{
public:
    explicit AboutDialog(bayu::ui::Window *parent);
    ~AboutDialog();

    void show();
    bayu::ui::Window *window() const { return m_window.get(); }

private:
    class Content;

    void close();

    bayu::ui::Window *m_parent;
    std::unique_ptr<bayu::ui::Window> m_window;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

#endif
