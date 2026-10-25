// SPDX-License-Identifier: GPL-3.0-or-later
// bayu-welcome: a short tour after the first login, ending in Bayu Store.

#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/event_loop.h"
#include "base/i18n.h"
#include "base/paths.h"
#include "base/process.h"
#include "bayu/appearance/appearance.h"
#include "lapis.h"
#include "ui/application.h"
#include "ui/image_view.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/style.h"
#include "ui/switch.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "ui/window.h"
#include "ui/window_header.h"

namespace fs = std::filesystem;
using namespace bayu::ui;
using bayu::base::Process;
using bayu::i18n::tr;
using store::ActionButton;
using store::addText;
using store::style::Role;

namespace {

// The tour is not offered again once this exists.
std::string doneFile()
{
    return bayu::base::paths::configHome() + "/bayu/welcome-done";
}

void run(const std::string& program, const std::vector<std::string>& arguments = {})
{
    Process::startDetached(program, arguments);
}

std::string screenshot(const char* name)
{
    return std::string(WELCOME_DATA_DIR) + "/" + name + ".webp";
}

// Where the tour is: a dot for each slide, the current one long.
class Dots : public View
{
public:
    void set(size_t count, size_t current)
    {
        m_count = count;
        m_current = current;
        requestPaint();
    }
    SizeF sizeHint() const override { return {m_count * 14.0 + 12, 8}; }

protected:
    void paint(Painter& painter) override
    {
        double x = 0;
        for (size_t i = 0; i < m_count; ++i) {
            const double width = i == m_current ? 20 : 8;
            const Color color = i == m_current ? Theme::current().highlight : Theme::current().text.withAlpha(0.18);
            painter.drawRoundedRect({x, 0, width, 8}, {4, 0, Corners::Circular}, color);
            x += width + 6;
        }
    }

private:
    size_t m_count = 0;
    size_t m_current = 0;
};

// A wallpaper to pick, ringed while it is the current one.
class WallpaperTile : public store::Pressable
{
public:
    explicit WallpaperTile(const std::string& path)
    {
        m_image = addChild<ImageView>();
        m_image->setThumbnail(true);
        m_image->setFillMode(ImageView::FillMode::PreserveAspectCrop);
        m_image->setCornerRadius(8);
        m_image->setSource(path);
    }
    void setSelected(bool selected)
    {
        m_selected = selected;
        requestPaint();
    }
    SizeF sizeHint() const override { return {88, 56}; }

protected:
    void layout() override { m_image->setGeometry(localRect().adjusted(3, 3, -3, -3)); }
    void paint(Painter& painter) override
    {
        if (m_selected || isHovered() || hasVisibleFocus())
            painter.drawRoundedRect(localRect(), {10, 0, Corners::Circular}, Color{0, 0, 0, 0}, 2,
                                    Theme::current().highlight.withAlpha(m_selected ? 1 : 0.4));
    }

private:
    ImageView* m_image;
    bool m_selected = false;
};

struct Slide
{
    std::string eyebrow;
    std::string title;
    std::string text;
    const char* image;
    // Buttons and choices under the text.
    std::function<void(Column*)> extra;
};

class Welcome : public View
{
public:
    Welcome()
    {
        m_header = addChild<WindowHeader>(48);
        m_header->setButtonMargin(14);
        m_eyebrow = addText(this, {}, 11, 700);
        m_eyebrow->setColor(ColorRecipe(ColorId::Highlight));
        m_eyebrow->setLetterSpacing(2);
        m_title = addText(this, {}, 30, 700);
        m_title->setWrap(true);
        m_text = addText(this, {}, 15, 400, Role::SecondaryText);
        m_text->setWrap(true);
        m_extra = addChild<Column>();
        m_image = addChild<ImageView>();
        m_image->setFillMode(ImageView::FillMode::PreserveAspectFit);
        m_image->setCornerRadius(12);
        m_dots = addChild<Dots>();
        m_skip = addChild<ActionButton>(tr("Skip"), ActionButton::Kind::Link);
        m_back = addChild<ActionButton>(tr("Back"));
        m_back->setLeadingGlyph("arrow-left");
        m_next = addChild<ActionButton>(std::string(), ActionButton::Kind::Primary);
        m_skip->onClicked = [] { Application::instance()->quit(); };
        m_back->onClicked = [this] { show(m_current - 1); };
        m_next->onClicked = [this] {
            if (m_current + 1 < m_slides.size())
                show(m_current + 1);
            else
                Application::instance()->quit();
        };
        m_appearanceChanged = m_appearance.changed.connect([this] { updateAppearance(); });
        buildSlides();
    }

    bool showAtLogin() const { return m_showAtLogin; }
    size_t count() const { return m_slides.size(); }

    void show(size_t index)
    {
        m_current = std::min(index, m_slides.size() - 1);
        const Slide& slide = m_slides[m_current];
        m_eyebrow->setText(slide.eyebrow);
        m_title->setText(slide.title);
        m_text->setText(slide.text);
        m_image->setSource(screenshot(slide.image));
        while (!m_extra->children().empty())
            m_extra->takeChild(m_extra->children().back().get());
        m_dark = nullptr;
        m_wallpapers.clear();
        if (slide.extra)
            slide.extra(m_extra);
        updateAppearance();
        const bool first = m_current == 0;
        const bool last = m_current + 1 == m_slides.size();
        m_back->setVisible(!first);
        m_skip->setVisible(!last);
        m_next->setText(first ? tr("Start the tour") : last ? tr("Get started") : tr("Next"));
        m_next->setTrailingGlyph(last ? std::string() : "arrow-right");
        m_dots->set(m_slides.size(), m_current);
        requestLayout();
        requestPaint();
    }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        m_header->setGeometry({0, 0, width, 48});
        const double margin = 48;
        const double footerY = height - 40 - 32;
        const double textWidth = std::round((width - 2 * margin) * 0.42);
        double y = 64;
        auto place = [&](View* view, double gap) {
            const double h = view->heightForWidth(textWidth);
            view->setGeometry({margin, y, textWidth, h});
            y += h + gap;
        };
        place(m_eyebrow, 14);
        place(m_title, 14);
        place(m_text, 24);
        place(m_extra, 0);
        const double imageX = margin + textWidth + 40;
        m_image->setGeometry({imageX, 64, width - imageX - margin, footerY - 64 - 32});

        const SizeF dots = m_dots->sizeHint();
        m_dots->setGeometry({margin, footerY + (40 - dots.height) / 2, dots.width, dots.height});
        double x = width - margin;
        for (ActionButton* button : {m_next, m_back, m_skip}) {
            if (!button->isVisible())
                continue;
            const double w = button->sizeHint().width;
            x -= w;
            button->setGeometry({x, footerY, w, 40});
            x -= 12;
        }
    }

    void paint(Painter& painter) override { painter.fillRect(localRect(), store::resolve(Role::Content)); }

private:
    void buildSlides()
    {
        m_slides.push_back({tr("WELCOME"), tr("Welcome to IGN 26"),
                            tr("Bayu is ready to use. Take a short tour, under a minute, to find your way around."),
                            "desktop", {}});

        m_slides.push_back({tr("BAYU STORE"), tr("Install your first app"),
                            tr("Bayu Store brings thousands of apps from Flathub and F-Droid together. Search, "
                               "click Install, and it is done. Some to start with:"),
                            "store", [](Column* extra) {
                                extra->setSpacing(16);
                                auto* list = extra->addChild<store::ListCard>();
                                // It scrolls and fills its column otherwise.
                                list->setLayoutHints({.fillWidth = true, .preferredHeight = 3 * store::style::listRowHeight + 14});
                                struct Pick
                                {
                                    const char* id;
                                    const char* glyph;
                                    const char* name;
                                    std::string detail;
                                };
                                for (const Pick& pick : {Pick{"org.mozilla.firefox", "globe", "Firefox",
                                                              tr("Browse the web")},
                                                         Pick{"org.libreoffice.LibreOffice", "briefcase",
                                                              "LibreOffice", tr("Documents, spreadsheets and slides")},
                                                         Pick{"org.videolan.VLC", "film", "VLC",
                                                              tr("Plays any video or music")}}) {
                                    store::ListRow* row = list->addRow();
                                    row->setGlyph(pick.glyph);
                                    row->setTitle(pick.name);
                                    row->setDetail(pick.detail);
                                    row->setTrailingGlyph("chevron-right-sm");
                                    const std::string id = pick.id;
                                    row->onClicked = [id] { run("bayu-store", {"--app=" + id}); };
                                }
                                auto* open = extra->addChild<ActionButton>(tr("Open Bayu Store"),
                                                                           ActionButton::Kind::Primary);
                                open->setLeadingGlyph("download");
                                open->onClicked = [] { run("bayu-store"); };
                            }});

        m_slides.push_back({tr("LAUNCHER AND DOCK"), tr("Find every app"),
                            tr("Click the rocket in the dock to see all your apps, and type to search. Apps you "
                               "install appear there at once. Drag one to the dock to keep it at hand."),
                            "launcher", {}});

        m_slides.push_back({tr("ANDROID APPS"), tr("Run Android apps"),
                            tr("Android apps run in their own windows, next to your other apps, through lapis. "
                               "Turning it on downloads Android once, about 1.3 GB."),
                            "android", [this](Column* extra) { buildAndroid(extra); }});

        m_slides.push_back({tr("APPEARANCE"), tr("Make it yours"),
                            tr("Choose light or dark, and a wallpaper. More choices are in Settings."), "settings",
                            [this](Column* extra) { buildAppearance(extra); }});

        m_slides.push_back({tr("UPDATES"), tr("Stay up to date"),
                            tr("App updates are in Bayu Store, under Updates. For the system, run "
                               "\"sudo dnf upgrade\" in Terminal."),
                            "updates", [](Column* extra) {
                                extra->setSpacing(8);
                                auto* updates = extra->addChild<ActionButton>(tr("Open Updates"));
                                updates->setLeadingGlyph("refresh");
                                updates->onClicked = [] { run("bayu-store", {"--updates"}); };
                                auto* issues = extra->addChild<ActionButton>(tr("Report a problem"),
                                                                             ActionButton::Kind::Link);
                                issues->setTrailingGlyph("external-link");
                                issues->onClicked = [] {
                                    run("xdg-open", {"https://github.com/igos-nusantara/bayu/issues"});
                                };
                            }});

        m_slides.push_back({tr("ALL SET"), tr("Enjoy IGN 26"),
                            tr("Open this tour again any time from the launcher: look for Welcome."), "desktop",
                            [this](Column* extra) {
                                auto* again = extra->addChild<store::CheckRow>(
                                    tr("Show at login"), tr("Open Welcome each time you sign in"));
                                again->setLayoutHints({.fillWidth = true});
                                again->setChecked(m_showAtLogin);
                                again->onToggled = [this](bool checked) { m_showAtLogin = checked; };
                            }});
    }

    void buildAndroid(Column* extra)
    {
        extra->setSpacing(10);
        if (::access("/usr/bin/lapis", X_OK) != 0) {
            addText(extra, tr("Android apps are not available on this system."), 14, 500, Role::SecondaryText);
            return;
        }
        m_androidStatus = addText(extra, {}, 14, 500, Role::Success);
        m_androidStatus->setWrap(true);
        m_androidStatus->setLayoutHints({.fillWidth = true});
        m_androidButton = extra->addChild<ActionButton>(tr("Turn on Android apps"), ActionButton::Kind::Primary);
        m_androidButton->setLeadingGlyph("smartphone");
        m_androidButton->onClicked = [this] { enableAndroid(); };
        updateAndroid({});
    }

    void updateAndroid(const std::string& error)
    {
        const bool on = store::lapis::state() != store::lapis::State::Missing;
        const bool busy = m_androidSetup && m_androidSetup->isRunning();
        m_androidStatus->setText(on ? tr("Android apps are on. Find them in Bayu Store under F-Droid.") : error);
        m_androidStatus->setColor(store::style::color(on ? Role::Success : Role::Danger));
        m_androidStatus->setVisible(on || !error.empty());
        m_androidButton->setVisible(!on);
        m_androidButton->setBusy(busy);
        m_androidButton->setText(busy ? tr("Setting up Android") : tr("Turn on Android apps"));
        requestLayout();
    }

    // As Bayu Store does: the control service and Android's images as root,
    // then Android for this session.
    void enableAndroid()
    {
        m_androidSetup = std::make_unique<Process>();
        m_androidSetup->onFinished = [this](int exitCode, bool crashed) {
            if (exitCode == 0 && !crashed)
                run("systemctl", {"--user", "start", "lapis-session"});
            if (m_androidButton)
                updateAndroid(exitCode == 0 && !crashed ? std::string()
                                                        : tr("Android could not be set up. Try again from Bayu Store."));
        };
        m_androidSetup->start("pkexec", {"/usr/libexec/bayu-store/android-setup"});
        updateAndroid({});
    }

    void buildAppearance(Column* extra)
    {
        extra->setSpacing(16);
        m_dark = extra->addChild<Switch>(tr("Dark mode"));
        m_dark->onToggled = [this](bool dark) {
            m_appearance.setDarkModeSchedule(bayu::appearance::Appearance::Never);
            m_appearance.setDarkMode(dark);
        };
        auto* tiles = extra->addChild<Row>();
        tiles->setSpacing(6);
        std::vector<std::string> paths;
        for (const std::string& data : bayu::base::paths::dataDirectories()) {
            std::error_code error;
            for (const auto& entry : fs::directory_iterator(data + "/backgrounds/bayuos", error))
                paths.push_back(entry.path().string());
        }
        std::sort(paths.begin(), paths.end());
        paths.resize(std::min<size_t>(paths.size(), 4));
        for (const std::string& path : paths) {
            auto* tile = tiles->addChild<WallpaperTile>(path);
            tile->onClicked = [this, path] { m_appearance.setWallpaper(path); };
            m_wallpapers.push_back({path, tile});
        }
        auto* settings = extra->addChild<ActionButton>(tr("Open Settings"));
        settings->setLeadingGlyph("settings");
        settings->onClicked = [] { run("bayu-settings", {"-m", "appearance"}); };
    }

    void updateAppearance()
    {
        if (m_dark)
            m_dark->setChecked(m_appearance.darkMode());
        const std::string current = m_appearance.wallpaper();
        for (const auto& [path, tile] : m_wallpapers)
            tile->setSelected(path == current);
    }

    WindowHeader* m_header;
    Label* m_eyebrow;
    Label* m_title;
    Label* m_text;
    Column* m_extra;
    ImageView* m_image;
    Dots* m_dots;
    ActionButton* m_skip;
    ActionButton* m_back;
    ActionButton* m_next;
    std::vector<Slide> m_slides;
    size_t m_current = 0;
    bool m_showAtLogin = false;

    bayu::appearance::Appearance m_appearance;
    bayu::base::Subscription m_appearanceChanged;
    Switch* m_dark = nullptr;
    std::vector<std::pair<std::string, WallpaperTile*>> m_wallpapers;
    Label* m_androidStatus = nullptr;
    ActionButton* m_androidButton = nullptr;
    std::unique_ptr<Process> m_androidSetup;
};

bool isLive()
{
    std::stringstream cmdline;
    cmdline << std::ifstream("/proc/cmdline").rdbuf();
    return cmdline.str().find("rd.live.image") != std::string::npos;
}

void printHelp(const char* program)
{
    printf("Usage: %s [options]\n"
           "A short tour of Bayu.\n"
           "\n"
           "Options:\n"
           "  -h, --help              Displays help on commandline options.\n"
           "  --autostart             Only once after the first login, and not on live media\n"
           "  --capture=<directory>   Write every slide as a PNG, headless\n"
           "  --dark                  The dark theme, for captures\n",
           program);
}

} // namespace

int main(int argc, char** argv)
{
    bool dark = false;
    std::string capture;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "-h" || argument == "--help") {
            printHelp(argv[0]);
            return 0;
        } else if (argument == "--autostart") {
            if (isLive() || fs::exists(doneFile()))
                return 0;
        } else if (argument == "--dark") {
            dark = true;
        } else if (argument.starts_with("--capture=")) {
            capture = argument.substr(10);
        } else {
            fprintf(stderr, "Unknown option '%s'.\n", argument.c_str());
            return 1;
        }
    }

    Application application(argc, argv,
                            capture.empty() ? Application::Platform::Wayland : Application::Platform::Headless);
    application.setApplicationId("bayu-welcome");
    bayu::i18n::setDomain("bayu-store");
    if (dark)
        Theme::setCurrent(Theme::dark());
    store::style::install();

    Window window;
    window.setTitle(tr("Welcome"));
    window.resize({1000, 640});
    window.setMinimumSize({900, 600});
    auto content = std::make_unique<Welcome>();
    Welcome* welcome = content.get();
    window.setContentView(std::move(content));
    welcome->show(0);
    window.show();

    if (!capture.empty()) {
        // One slide at a time, once its screenshot has decoded on its thread.
        auto failures = std::make_shared<int>(0);
        auto next = std::make_shared<std::function<void(size_t)>>();
        *next = [&, failures, next](size_t i) {
            if (i == welcome->count()) {
                Application::instance()->quit(*failures ? 1 : 0);
                return;
            }
            welcome->show(i);
            // The first paint starts decoding the screenshot.
            window.grab();
            bayu::base::EventLoop::current()->startTimer(std::chrono::milliseconds(600), false, [&, failures, next, i] {
                char file[64];
                snprintf(file, sizeof(file), "/welcome-%02zu.png", i + 1);
                if (!window.grab().savePng(capture + file))
                    ++*failures;
                (*next)(i + 1);
            });
        };
        (*next)(0);
        return application.run();
    }

    window.onCloseRequested = [] { Application::instance()->quit(); };
    const int status = application.run();
    std::error_code error;
    if (welcome->showAtLogin()) {
        fs::remove(doneFile(), error);
    } else {
        fs::create_directories(fs::path(doneFile()).parent_path(), error);
        std::ofstream(doneFile()).put('\n');
    }
    return status;
}
