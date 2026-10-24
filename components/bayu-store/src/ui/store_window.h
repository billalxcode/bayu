// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "backend.h"
#include "base/timer.h"
#include "ui/layout.h"
#include "ui/store_widgets.h"

namespace bayu::ui {
class ConfirmDialog;
class DialogWindow;
class ScrollView;
class SearchField;
class Window;
}

namespace store {

// A place in the store, for the history and the sidebar.
struct Route
{
    enum class Kind {
        Discover,
        Trending,
        Categories,
        Category,
        App,
        Library,
        Search,
        Source,
        Settings,
    };
    Kind kind = Kind::Discover;
    // Category: its id. App: its id. Search: the text. Library: the tab.
    std::string param;
    Source source = Source::Flathub;
    // Lists: the chip on ("Photo", "flathub", "open-source"...), the page,
    // the order.
    std::string filter;
    int page = 1;
    Sort sort = Sort::Popular;

    bool operator==(const Route&) const = default;
};

class StoreWindow;

// A page of the store, built for a route and rebuilt when the backend
// changes.
class StorePage : public Column
{
public:
    explicit StorePage(StoreWindow& store);
    virtual std::string title() const = 0;

protected:
    StoreWindow& store() const { return m_store; }
    Backend& backend() const;

private:
    StoreWindow& m_store;
};

std::unique_ptr<StorePage> createPage(StoreWindow& store, const Route& route);

class Root;

class StoreWindow
{
public:
    explicit StoreWindow(Backend& backend);
    ~StoreWindow();

    Backend& backend() const { return m_backend; }
    bayu::ui::Window* window() const { return m_window.get(); }
    const Route& route() const { return m_route; }

    void show();
    void go(const Route& route);
    // The same page with other filters: no new history entry.
    void replace(const Route& route);
    void back();
    void forward();
    void openApp(const App& app);

    // What an app's button says, and doing it.
    AppAction actionFor(const App& app) const;
    void act(const App& app);
    void showError(const std::string& title, const std::string& text);
    // Uninstalls after the user agrees.
    void askToRemove(const std::string& id, Source source);

    // Renders the main pages into PNGs, for screenshots of the design.
    int capture(const std::string& directory);

private:
    void build(bool keepScroll);
    void scheduleRebuild();
    void updateChrome();
    void askToEnableAndroid(const App& app);

    Backend& m_backend;
    std::unique_ptr<bayu::ui::Window> m_window;
    Root* m_root = nullptr;
    Route m_route;
    std::vector<Route> m_history;
    std::vector<Route> m_future;
    StorePage* m_page = nullptr;
    bayu::base::Timer m_rebuild;
    bayu::base::Timer m_search;
    std::unique_ptr<bayu::ui::ConfirmDialog> m_confirm;
    std::unique_ptr<bayu::ui::DialogWindow> m_dialog;
};

} // namespace store
