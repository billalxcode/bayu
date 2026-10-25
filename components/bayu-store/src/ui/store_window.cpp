// SPDX-License-Identifier: BSD-3-Clause

#include "ui/store_window.h"

#include <chrono>
#include <cstdio>

#include "base/event_loop.h"
#include "base/i18n.h"
#include "ui/application.h"
#include "ui/dialog_window.h"
#include "ui/header_button.h"
#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/scroll_view.h"
#include "ui/search_field.h"
#include "ui/theme.h"
#include "ui/window.h"
#include "ui/window_header.h"

using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;

namespace store {

using style::Role;

StorePage::StorePage(StoreWindow& store)
    : m_store(store)
{
    setSpacing(28);
    setMargins({28, 22, 28, 28});
}

Backend& StorePage::backend() const
{
    return m_store.backend();
}

namespace {

// A line of the sidebar: glyph, label, and a count or a badge.
class SidebarItem : public Pressable
{
public:
    SidebarItem(std::string glyph, std::string text)
    {
        m_icon = addChild<Icon>(std::move(glyph), 19);
        m_label = addText(this, std::move(text), 16, 500);
        m_count = addText(this, {}, 13, 500, Role::SecondaryText);
        setSelected(false);
    }
    void setSelected(bool selected)
    {
        m_selected = selected;
        const ColorRecipe color = selected ? ColorRecipe(ColorId::Highlight) : style::color(Role::TonalText);
        m_label->setColor(color);
        m_icon->setColor(selected ? ColorRecipe(ColorId::Highlight) : style::color(Role::SecondaryText));
        requestPaint();
    }
    // A plain number ("8,420") or a blue badge ("3").
    void setCount(int count, bool badge, bool pill)
    {
        m_badge = badge && count > 0;
        m_pill = pill;
        m_count->setText(count > 0 || pill ? style::formatCount(count) : std::string());
        m_count->setColor(m_badge ? ColorRecipe(Color::rgb(0xFFFFFF))
                                  : style::color(pill ? Role::TonalText : Role::TertiaryText));
        requestLayout();
    }
    SizeF sizeHint() const override { return {180, 37}; }

protected:
    void layout() override
    {
        const double h = geometry().height;
        m_icon->setGeometry({12, (h - 19) / 2, 19, 19});
        const SizeF count = m_count->sizeHint();
        const double right = geometry().width - 12;
        m_count->setGeometry({right - count.width - (m_badge || m_pill ? 8 : 0), (h - count.height) / 2, count.width,
                              count.height});
        const SizeF label = m_label->sizeHint();
        m_label->setGeometry({44, (h - label.height) / 2, right - 52 - count.width, label.height});
    }
    void paint(Painter& painter) override
    {
        const RoundedShape shape{10, 0, Corners::Circular};
        if (m_selected)
            painter.drawRoundedRect(localRect(), shape, Theme::current().highlight.withAlpha(0.12));
        else if (isHovered())
            painter.drawRoundedRect(localRect(), shape, Theme::current().text.withAlpha(0.04));
        if (m_badge || (m_pill && !m_count->text().empty())) {
            const RectF count = m_count->geometry().adjusted(-8, -3, 8, 3);
            const RectF pill{count.x, count.y, std::max(count.width, count.height), count.height};
            painter.drawRoundedRect(pill, {pill.height / 2, 0, Corners::Circular},
                                    m_badge ? Theme::current().highlight : resolve(Role::Tonal));
        }
    }

private:
    bool m_selected = false;
    bool m_badge = false;
    bool m_pill = false;
    Icon* m_icon;
    Label* m_label;
    Label* m_count;
};

class Sidebar : public View
{
public:
    explicit Sidebar(StoreWindow& store)
        : m_store(store)
    {
        m_column = addChild<Column>();
        m_column->setSpacing(2);
        addSection(tr("BROWSE"), false);
        add("compass", tr("Discover"), {Route::Kind::Discover});
        add("flame", tr("Trending"), {Route::Kind::Trending});
        add("layout-grid", tr("Categories"), {Route::Kind::Categories});
        add("gamepad", tr("Games"), {Route::Kind::Category, "games"});
        add("sparkles", tr("Creative"), {Route::Kind::Category, "creative"});
        add("terminal", tr("Developer"), {Route::Kind::Category, "developer"});
        addSection(tr("LIBRARY"), true);
        m_installed = add("package", tr("Installed"), {Route::Kind::Library, "installed"});
        m_updates = add("download", tr("Updates"), {Route::Kind::Library, "updates"});
        add("history", tr("Recent"), {Route::Kind::Library, "recent"});
        addSection(tr("SOURCES"), true);
        m_flathub = add("package", "Flathub", {Route::Kind::Source, {}, Source::Flathub});
        m_fdroid = add("shield-check", "F-Droid", {Route::Kind::Source, {}, Source::FDroid});
        m_settings = addChild<ActionButton>(tr("Store settings"));
        m_settings->setLeadingGlyph("settings");
        m_settings->onClicked = [this] { m_store.go({Route::Kind::Settings}); };
    }

    void update()
    {
        const Route& route = m_store.route();
        for (auto& [item, target] : m_items) {
            bool selected = route.kind == target.kind && route.param == target.param
                            && (route.kind != Route::Kind::Source || route.source == target.source);
            // The library's pages share the sidebar's line of their tab.
            if (route.kind == Route::Kind::Library && target.kind == Route::Kind::Library)
                selected = route.param == target.param || (route.param == "downloads" && target.param == "updates");
            if (route.kind == Route::Kind::App || route.kind == Route::Kind::Search)
                selected = target.kind == Route::Kind::Discover;
            item->setSelected(selected);
        }
        Backend& backend = m_store.backend();
        m_installed->setCount(int(backend.installed().size()), false, true);
        m_updates->setCount(int(backend.updates().size()), true, false);
        m_flathub->setCount(backend.count(Source::Flathub), false, false);
        m_fdroid->setCount(backend.count(Source::FDroid), false, false);
    }

protected:
    void layout() override
    {
        const double w = geometry().width;
        const double h = geometry().height;
        m_column->setGeometry({12, 18, w - 24, m_column->heightForWidth(w - 24)});
        m_settings->setGeometry({12, h - 18 - 44, w - 24, 44});
    }
    void paint(Painter& painter) override
    {
        painter.fillRect(localRect(), resolve(Role::Sidebar));
        painter.fillRect({geometry().width - 1, 0, 1, geometry().height}, resolve(Role::Divider));
    }

private:
    void addSection(std::string text, bool gap)
    {
        if (gap)
            m_column->addChild<View>()->setLayoutHints({.fillWidth = true, .preferredHeight = 12});
        Label* label = addSectionLabel(m_column, std::move(text));
        label->setLayoutHints({.margins = {12, 0, 0, 6}});
    }
    SidebarItem* add(std::string glyph, std::string text, Route route)
    {
        SidebarItem* item = m_column->addChild<SidebarItem>(std::move(glyph), std::move(text));
        item->setLayoutHints({.fillWidth = true});
        item->onClicked = [this, route] { m_store.go(route); };
        m_items.push_back({item, route});
        return item;
    }

    StoreWindow& m_store;
    Column* m_column;
    std::vector<std::pair<SidebarItem*, Route>> m_items;
    SidebarItem* m_installed;
    SidebarItem* m_updates;
    SidebarItem* m_flathub;
    SidebarItem* m_fdroid;
    ActionButton* m_settings;
};

} // namespace

// The window's content: the sidebar, the header and the page.
class Root : public View
{
public:
    explicit Root(StoreWindow& store)
    {
        sidebar = addChild<Sidebar>(store);
        scroll = addChild<ScrollView>();
        scroll->setVerticalBarPolicy(ScrollView::BarPolicy::AsNeeded);
        header = addChild<WindowHeader>(style::headerHeight);
        header->setButtonMargin(14);
        Row* bar = header->setContent<Row>();
        bar->setSpacing(6);
        back = bar->addChild<HeaderButton>(std::string(), 32);
        back->setGlyph("chevron-left-sm", 20);
        back->setLayoutHints({.margins = {14, 0, 0, 0}});
        forward = bar->addChild<HeaderButton>(std::string(), 32);
        forward->setGlyph("chevron-right-sm", 20);
        title = addText(bar, {}, 19, 600);
        title->setLayoutHints({.margins = {8, 0, 0, 0}});
        bar->addStretch();
        menu = bar->addChild<HeaderButton>(std::string(), 32);
        menu->setGlyph("menu", 18);
        menu->setLayoutHints({.margins = {0, 0, 4, 0}});
        search = addChild<SearchField>();
        search->setPlaceholder(tr("Search apps and games"));
        search->setFontSize(15);
        search->setFill(style::color(Role::Tonal));
    }

    Sidebar* sidebar;
    ScrollView* scroll;
    WindowHeader* header;
    HeaderButton* back;
    HeaderButton* forward;
    HeaderButton* menu;
    Label* title;
    SearchField* search;

protected:
    void layout() override
    {
        const double w = geometry().width;
        const double h = geometry().height;
        const double left = style::sidebarWidth;
        // The header spans the window; the sidebar starts under it.
        header->setGeometry({0, 0, w, style::headerHeight});
        sidebar->setGeometry({0, style::headerHeight, left, h - style::headerHeight});
        scroll->setGeometry({left, style::headerHeight, w - left, h - style::headerHeight});
        const double searchW = std::min(480.0, w - 2 * 330);
        search->setGeometry({(w - searchW) / 2, (style::headerHeight - 38) / 2, std::max(220.0, searchW), 38});
    }
    void paint(Painter& painter) override
    {
        painter.fillRect(localRect(), resolve(Role::Content));
        painter.fillRect({0, style::headerHeight - 1, geometry().width, 1}, resolve(Role::Divider));
    }
};

StoreWindow::StoreWindow(Backend& backend)
    : m_backend(backend)
    , m_window(std::make_unique<Window>())
{
    m_window->setTitle(tr("Bayu Store"));
    m_window->resize({style::windowWidth, style::windowHeight});
    m_window->setMinimumSize({960, 620});
    auto root = std::make_unique<Root>(*this);
    m_root = root.get();
    m_window->setContentView(std::move(root));
    m_root->back->onClicked = [this] { back(); };
    m_root->forward->onClicked = [this] { forward(); };
    m_window->addShortcut(AltModifier, key::Left, [this] { back(); });
    m_window->addShortcut(AltModifier, key::Right, [this] { forward(); });
    m_root->menu->onClicked = [this] {
        auto* menu = new Menu;
        menu->addItem(tr("Check for updates"), [this] { m_backend.checkUpdates(); });
        menu->addItem(tr("Store settings"), [this] { go({Route::Kind::Settings}); });
        menu->addSeparator();
        menu->addItem(tr("Quit"), [] { Application::instance()->quit(); });
        menu->onClosed = [menu] { bayu::base::EventLoop::current()->post([menu] { delete menu; }); };
        menu->popupFor(m_root->menu, 4);
    };
    m_search.setSingleShot(true);
    m_search.onTimeout = [this] {
        const std::string text = m_root->search->text();
        if (text.empty()) {
            if (m_route.kind == Route::Kind::Search)
                back();
            return;
        }
        Route route{Route::Kind::Search, text};
        // Typing refines the search in place instead of stacking history.
        if (m_route.kind == Route::Kind::Search) {
            m_route = route;
            build(false);
        } else {
            go(route);
        }
    };
    m_root->search->onTextEdited = [this] { m_search.start(std::chrono::milliseconds(250)); };
    m_root->search->onAccepted = [this] {
        m_search.stop();
        m_search.onTimeout();
    };
    m_rebuild.setSingleShot(true);
    m_rebuild.onTimeout = [this] { build(true); };
    m_backend.onChanged = [this] { scheduleRebuild(); };
}

StoreWindow::~StoreWindow()
{
    m_backend.onChanged = nullptr;
}

void StoreWindow::show()
{
    build(false);
    m_window->show();
}

void StoreWindow::go(const Route& route)
{
    if (route == m_route && m_page)
        return;
    m_history.push_back(m_route);
    m_future.clear();
    m_route = route;
    if (route.kind != Route::Kind::Search && !m_root->search->text().empty())
        m_root->search->setText({});
    build(false);
}

void StoreWindow::replace(const Route& route)
{
    m_route = route;
    build(false);
}

void StoreWindow::back()
{
    if (m_history.empty())
        return;
    m_future.push_back(m_route);
    m_route = m_history.back();
    m_history.pop_back();
    if (m_route.kind == Route::Kind::Search)
        m_root->search->setText(m_route.param);
    build(false);
}

void StoreWindow::forward()
{
    if (m_future.empty())
        return;
    m_history.push_back(m_route);
    m_route = m_future.back();
    m_future.pop_back();
    build(false);
}

void StoreWindow::openApp(const App& app)
{
    go({Route::Kind::App, app.id, app.source});
}

void StoreWindow::scheduleRebuild()
{
    // Progress arrives many times a second: once in a while is enough.
    if (!m_rebuild.isActive())
        m_rebuild.start(std::chrono::milliseconds(300));
}

void StoreWindow::build(bool keepScroll)
{
    const PointF position = m_root->scroll->scrollPosition();
    auto page = createPage(*this, m_route);
    m_page = page.get();
    m_root->scroll->setContentView(std::move(page));
    if (keepScroll)
        m_root->scroll->setScrollPosition(position);
    updateChrome();
}

void StoreWindow::updateChrome()
{
    m_root->title->setText(m_page ? m_page->title() : std::string());
    m_root->back->setEnabled(!m_history.empty());
    m_root->forward->setEnabled(!m_future.empty());
    m_root->sidebar->update();
    m_root->requestLayout();
    const std::string error = m_backend.takeError();
    if (!error.empty())
        showError(tr("That did not work"), error);
}

AppAction StoreWindow::actionFor(const App& app) const
{
    AppAction action;
    if (const std::optional<Job> job = m_backend.job(app.id, app.source)) {
        action.progress = std::max(0.0, job->progress);
        action.text = job->paused ? tr("PAUSED")
                      : job->progress >= 0 ? std::to_string(int(job->progress * 100)) + "%"
                                           : tr("QUEUED");
        action.does = AppAction::Does::Pause;
        return action;
    }
    if (const std::optional<InstalledApp> installed = m_backend.installedApp(app.id, app.source)) {
        if (!installed->newVersion.empty()) {
            action.text = tr("UPDATE");
            action.does = AppAction::Does::Update;
            return action;
        }
        action.text = tr("OPEN");
        action.kind = PillButton::Kind::Neutral;
        action.does = AppAction::Does::Open;
        return action;
    }
    action.text = tr("INSTALL");
    action.does = app.source == Source::FDroid && !m_backend.androidAvailable() ? AppAction::Does::EnableAndroid
                                                                                : AppAction::Does::Install;
    return action;
}

void StoreWindow::act(const App& app)
{
    switch (actionFor(app).does) {
    case AppAction::Does::Install:
        m_backend.install(app);
        break;
    case AppAction::Does::Open:
        m_backend.launch(app.id, app.source);
        break;
    case AppAction::Does::Update:
        m_backend.update(app.id, app.source);
        break;
    case AppAction::Does::Pause:
        if (const std::optional<Job> job = m_backend.job(app.id, app.source)) {
            if (job->paused)
                m_backend.resume(app.id, app.source);
            else
                m_backend.pause(app.id, app.source);
        }
        break;
    case AppAction::Does::EnableAndroid:
        askToEnableAndroid(app);
        break;
    case AppAction::Does::None:
        break;
    }
}

void StoreWindow::askToRemove(const std::string& id, Source source)
{
    const std::optional<App> app = m_backend.app(id, source);
    m_confirm = std::make_unique<ConfirmDialog>(
        m_window.get(), format(tr("Uninstall {name}?"), {{"name", app ? app->name : id}}),
        source == Source::FDroid ? tr("The app and everything it saved in Android are removed.")
                                 : tr("You can install it again from the store at any time."));
    m_confirm->setConfirmText(tr("Uninstall"));
    m_confirm->onAccepted = [this, id, source] { m_backend.remove(id, source); };
    m_confirm->show();
}

void StoreWindow::askToEnableAndroid(const App& app)
{
    m_confirm = std::make_unique<ConfirmDialog>(
        m_window.get(), tr("Turn on Android apps?"),
        tr("F-Droid apps are Android apps: they run in Android (lapis) beside the desktop. Turning it on downloads "
           "Android once (about 1.3 GB) and may ask for your password."));
    m_confirm->setConfirmText(tr("Turn on"));
    m_confirm->onAccepted = [this, app] {
        m_backend.enableAndroid([this, app](const std::string& error) {
            if (!error.empty())
                showError(tr("Android apps could not be turned on"), error);
            else
                m_backend.install(app);
        });
    };
    m_confirm->show();
}

void StoreWindow::showError(const std::string& title, const std::string& text)
{
    m_confirm = std::make_unique<ConfirmDialog>(m_window.get(), title, text);
    m_confirm->setConfirmText(tr("OK"));
    m_confirm->setCancelVisible(false);
    m_confirm->show();
}

int StoreWindow::capture(const std::string& directory)
{
    m_window->resize({style::windowWidth, style::windowHeight});
    m_window->show();
    const std::vector<std::pair<const char*, Route>> shots{
        {"01-discover", {Route::Kind::Discover}},
        {"02-app-detail", {Route::Kind::App, "org.northlight.Lumen", Source::Flathub}},
        {"03-browse-category", {Route::Kind::Category, "creative"}},
        {"04-library-updates", {Route::Kind::Library, "updates"}},
        {"05-search-results", {Route::Kind::Search, "photo editor"}},
        {"06-categories", {Route::Kind::Categories}},
        {"07-trending", {Route::Kind::Trending}},
        {"08-source-fdroid", {Route::Kind::Source, {}, Source::FDroid}},
        {"09-settings", {Route::Kind::Settings}},
    };
    int failures = 0;
    for (const auto& [name, route] : shots) {
        m_route = route;
        if (route.kind == Route::Kind::Search)
            m_root->search->setText(route.param);
        else
            m_root->search->setText({});
        build(false);
        if (!m_window->grab().savePng(directory + "/" + name + ".png"))
            ++failures;
    }
    return failures ? 1 : 0;
}

} // namespace store
