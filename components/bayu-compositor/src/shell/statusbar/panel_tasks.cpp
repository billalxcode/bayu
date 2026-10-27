// SPDX-License-Identifier: BSD-3-Clause

#include "shell/statusbar/panel_tasks.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "base/i18n.h"
#include "core/main_loop.h"
#include "shell/common/task_windows.h"
#include "shell/dock/dock_model.h"
#include "shell/statusbar/pitaloka.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"

namespace bayu::shell {

using bayu::i18n::tr;

namespace {

// A Pitaloka colour token in the theme's mode.
ui::Color token(const char *light, const char *dark)
{
    const char *hex = ui::Theme::current().darkMode ? dark : light;
    return ui::Color::rgb(uint32_t(std::strtoul(hex + 1, nullptr, 16)));
}
#define PIT_COLOR(name) token(PITALOKA_##name##_LIGHT, PITALOKA_##name##_DARK)

// The accent is the desktop's (Pitaloka's --pit-accent by default); hovered,
// a step darker as --pit-accent-hover.
ui::Color accent(bool hovered)
{
    const ui::Color color = ui::Theme::current().highlight;
    if (!hovered)
        return color;
    const auto darker = [](uint8_t c) { return uint8_t(std::lround(c * 0.9)); };
    return {darker(color.r), darker(color.g), darker(color.b), color.a};
}

constexpr double kGap = PITALOKA_SPACE_1;
constexpr double kDividerGap = PITALOKA_SPACE_2;
constexpr double kMaxWindowWidth = 200;
constexpr double kDot = 5;
constexpr double kFontSize = 13;

// Takes the button out now and lets it go on the next turn: its own click
// may be what changed the list.
void retire(ui::View *parent, ui::View *child)
{
    std::shared_ptr<ui::View> retired(parent->takeChild(child));
    retired->setVisible(false);
    core::mainLoop()->post([retired] {});
}

} // namespace

// A clickable tile of the panel: hovered, pressed, selected and focused by
// Pitaloka's States.
class PanelTasks::Button : public ui::View
{
public:
    Button() { setTracksHover(true); }

    std::function<void(const ui::PointerEvent &)> onClicked;
    void setSelected(bool selected)
    {
        if (m_selected != selected) {
            m_selected = selected;
            requestPaint();
        }
    }
    bool acceptsPointer() const override { return true; }

protected:
    bool m_selected = false;
    bool m_pressed = false;

    void hoverChanged() override { requestPaint(); }
    void pointerPress(const ui::PointerEvent &) override
    {
        m_pressed = true;
        requestPaint();
    }
    void pointerRelease(const ui::PointerEvent &event) override
    {
        const bool inside = localRect().contains(event.position);
        m_pressed = false;
        requestPaint();
        if (inside && onClicked)
            onClicked(event);
    }

    // The plate: --pit-bg-selected selected or pressed, --pit-bg-hover hovered
    // or `resting`.
    void paintPlate(ui::Painter &painter, bool resting = false)
    {
        std::optional<ui::Color> plate;
        if (m_selected || m_pressed)
            plate = PIT_COLOR(BG_SELECTED);
        else if (isHovered() || resting)
            plate = PIT_COLOR(BG_HOVER);
        if (plate)
            painter.fillRoundedRect(localRect(), PITALOKA_RADIUS_CONTROL, *plate);
    }
};

// Start: the launcher, in the accent with a grid glyph.
class PanelTasks::StartButton final : public PanelTasks::Button
{
public:
    StartButton()
    {
        m_label = addChild<ui::Label>(tr("Start"));
        m_label->setFontSize(kFontSize);
        m_label->setWeight(600);
        m_label->setColor(token(PITALOKA_TEXT_ON_ACCENT_LIGHT, PITALOKA_TEXT_ON_ACCENT_DARK));
        m_label->setElide(ui::Label::Elide::None);
    }

    double preferredWidth() const { return PITALOKA_SPACE_3 * 2 + kGlyph + PITALOKA_SPACE_2 + m_label->sizeHint().width; }
    void retranslate() { m_label->setText(tr("Start")); }

protected:
    static constexpr double kGlyph = 16;

    void layout() override
    {
        const ui::SizeF size = m_label->sizeHint();
        m_label->setGeometry({PITALOKA_SPACE_3 + kGlyph + PITALOKA_SPACE_2, (localRect().height - size.height) / 2,
                              size.width, size.height});
    }

    void paint(ui::Painter &painter) override
    {
        painter.fillRoundedRect(localRect(), PITALOKA_RADIUS_CONTROL, accent(isHovered() || m_pressed));
        // Four rounded squares, as the launcher's grid.
        const ui::Color ink = token(PITALOKA_TEXT_ON_ACCENT_LIGHT, PITALOKA_TEXT_ON_ACCENT_DARK);
        const double left = PITALOKA_SPACE_3;
        const double top = std::round((localRect().height - kGlyph) / 2);
        const double cell = 7;
        for (int row = 0; row < 2; ++row) {
            for (int column = 0; column < 2; ++column)
                painter.strokeRoundedRect({left + column * 9 + 0.5, top + row * 9 + 0.5, cell - 1, cell - 1}, 1.5, ink,
                                          2);
        }
    }

private:
    ui::Label *m_label = nullptr;
};

// A pinned application: its icon on a tile.
class PanelTasks::PinButton final : public PanelTasks::Button
{
public:
    explicit PinButton(const DockItem &item)
        : id(item.id)
    {
        m_icon = addChild<ui::ThemeIcon>(item.iconName.empty() ? "application-x-desktop" : item.iconName, 24);
    }

    std::string id;

protected:
    void layout() override
    {
        const double size = std::round(localRect().height * 0.72);
        m_icon->setSize(size);
        m_icon->setGeometry({(localRect().width - size) / 2, (localRect().height - size) / 2, size, size});
    }
    void paint(ui::Painter &painter) override { paintPlate(painter); }

private:
    ui::ThemeIcon *m_icon = nullptr;
};

// A window: a dot, its application's icon and its title.
class PanelTasks::WindowButton final : public PanelTasks::Button
{
public:
    explicit WindowButton(uint64_t window)
        : id(window)
    {
        m_icon = addChild<ui::ThemeIcon>(std::string(), kIcon);
        m_title = addChild<ui::Label>();
        m_title->setFontSize(kFontSize);
        m_title->setElide(ui::Label::Elide::Right);
    }

    uint64_t id;

    void update(const std::string &icon, const std::string &title, bool active)
    {
        m_icon->setIconName(icon.empty() ? "application-x-desktop" : icon);
        m_title->setText(title);
        m_title->setWeight(active ? 500 : 400);
        m_title->setColor(PIT_COLOR(TEXT_PRIMARY));
        setSelected(active);
        requestPaint();
    }

    double preferredWidth() const
    {
        return std::min(kMaxWindowWidth, kTextStart + m_title->sizeHint().width + PITALOKA_SPACE_3);
    }
    // The icon, in the window's coordinates.
    ui::RectF iconTarget() const
    {
        const ui::PointF origin = mapToWindow({m_icon->geometry().x, m_icon->geometry().y});
        return {origin.x, origin.y, kIcon, kIcon};
    }

protected:
    static constexpr double kIcon = 18;
    static constexpr double kTextStart = 10 + kDot + 8 + kIcon + 8;

    void layout() override
    {
        const double height = localRect().height;
        m_icon->setGeometry({10 + kDot + 8, std::round((height - kIcon) / 2), kIcon, kIcon});
        const ui::SizeF size = m_title->sizeHint();
        m_title->setGeometry({kTextStart, (height - size.height) / 2,
                              std::max(0.0, localRect().width - kTextStart - PITALOKA_SPACE_3), size.height});
    }

    void paint(ui::Painter &painter) override
    {
        // Every window rests on --pit-bg-hover; the active one is selected.
        paintPlate(painter, true);
        const ui::Color dot = m_selected ? accent(false) : PIT_COLOR(TEXT_TERTIARY);
        painter.fillEllipse({10, (localRect().height - kDot) / 2, kDot, kDot}, dot);
    }

private:
    ui::ThemeIcon *m_icon = nullptr;
    ui::Label *m_title = nullptr;
};

PanelTasks::PanelTasks(TaskWindows &windows, DockModel &applications)
    : m_windows(windows)
    , m_applications(applications)
{
    m_start = addChild<StartButton>();
    m_start->onClicked = [this](const ui::PointerEvent &) {
        if (onStart)
            onStart();
    };

    const auto pins = [this](auto &&...) { rebuildPins(); };
    m_subscriptions.push_back(m_applications.rowsInserted.connect(pins));
    m_subscriptions.push_back(m_applications.rowsRemoved.connect(pins));
    m_subscriptions.push_back(m_applications.rowMoved.connect(pins));
    m_subscriptions.push_back(m_applications.dataChanged.connect(pins));
    m_subscriptions.push_back(m_applications.modelReset.connect(pins));
    m_subscriptions.push_back(m_applications.sectionsChanged.connect(pins));
    m_subscriptions.push_back(m_applications.loaded.connect(pins));

    m_subscriptions.push_back(m_windows.windowAdded.connect([this](uint64_t) { rebuildWindows(); }));
    m_subscriptions.push_back(m_windows.windowRemoved.connect([this](uint64_t) { rebuildWindows(); }));
    m_subscriptions.push_back(m_windows.windowChanged.connect([this](uint64_t id, uint32_t changes) {
        if (changes & (kSkipTaskbarChange | kAppIdChange))
            rebuildWindows();
        else if (changes & (kTitleChange | kStateChange))
            updateWindow(id);
    }));
    m_subscriptions.push_back(m_windows.activeWindowChanged.connect([this](uint64_t) {
        for (WindowButton *button : m_windowButtons)
            updateWindow(button->id);
    }));

    rebuildPins();
    rebuildWindows();
}

PanelTasks::~PanelTasks()
{
    m_subscriptions.clear();
}

void PanelTasks::setTileSize(double size)
{
    if (m_tileSize == size)
        return;
    m_tileSize = size;
    requestLayout();
}

bool PanelTasks::listed(uint64_t id) const
{
    const TaskWindow *window = m_windows.window(id);
    return window && window->isTaskListWindow() && !window->parent;
}

void PanelTasks::rebuildPins()
{
    for (PinButton *pin : m_pins)
        retire(this, pin);
    m_pins.clear();
    for (const auto &item : m_applications.items()) {
        if (!item->isPinned || item->fixed)
            continue;
        PinButton *pin = addChild<PinButton>(*item);
        pin->onClicked = [this, id = item->id](const ui::PointerEvent &) {
            if (onActivated)
                onActivated();
            m_applications.clicked(id);
        };
        m_pins.push_back(pin);
    }
    requestLayout();
    requestPaint();
}

void PanelTasks::rebuildWindows()
{
    for (WindowButton *button : m_windowButtons)
        retire(this, button);
    m_windowButtons.clear();
    for (uint64_t id : m_windows.windows()) {
        if (!listed(id))
            continue;
        WindowButton *button = addChild<WindowButton>(id);
        button->onClicked = [this, id](const ui::PointerEvent &event) {
            if (onActivated)
                onActivated();
            const TaskWindow *window = m_windows.window(id);
            if (!window)
                return;
            if (event.button == ui::MouseButton::Middle)
                m_windows.close(id);
            else if (m_windows.activeWindow() == id && !window->minimized && !window->hidden)
                m_windows.minimize(id);
            else
                m_windows.activate(id);
        };
        m_windowButtons.push_back(button);
        updateWindow(id);
    }
    requestLayout();
    requestPaint();
}

void PanelTasks::updateWindow(uint64_t id)
{
    const auto it = std::find_if(m_windowButtons.begin(), m_windowButtons.end(),
                                 [id](const WindowButton *button) { return button->id == id; });
    const TaskWindow *window = m_windows.window(id);
    if (it == m_windowButtons.end() || !window)
        return;
    const bool active = m_windows.activeWindow() == id && !window->minimized && !window->hidden;
    (*it)->update(m_applications.iconNameForWindow(id), window->title, active);
    requestLayout();
}

void PanelTasks::layout()
{
    const ui::RectF bounds = localRect();
    const double tile = std::min(m_tileSize, bounds.height);
    const double top = std::round((bounds.height - tile) / 2);
    m_dividers.clear();
    double x = 0;
    m_start->setGeometry({x, top, m_start->preferredWidth(), tile});
    x += m_start->preferredWidth() + kDividerGap;
    m_dividers.push_back(x);
    x += kDividerGap + 1;
    for (PinButton *pin : m_pins) {
        pin->setGeometry({x, top, tile, tile});
        x += tile + kGap;
    }
    if (!m_pins.empty() && !m_windowButtons.empty()) {
        x += kDividerGap - kGap;
        m_dividers.push_back(x);
        x += kDividerGap + 1;
    }
    // The windows share what is left, each up to its title's width.
    const double room = std::max(0.0, bounds.width - x);
    const size_t count = m_windowButtons.size();
    const double share = count ? (room - kGap * double(count - 1)) / double(count) : 0;
    for (WindowButton *button : m_windowButtons) {
        const double width = std::max(0.0, std::floor(std::min(button->preferredWidth(), share)));
        button->setVisible(width >= tile);
        button->setGeometry({x, top, width, tile});
        x += width + kGap;
    }
    targetsChanged();
}

void PanelTasks::paint(ui::Painter &painter)
{
    // The dividers: 1 px of --pit-border, as tall as an icon.
    const double height = std::round(std::min(m_tileSize, localRect().height) * 0.55);
    for (double x : m_dividers)
        painter.fillRect({x, std::round((localRect().height - height) / 2), 1, height}, PIT_COLOR(BORDER));
}

std::map<uint64_t, ui::RectF> PanelTasks::windowTargets() const
{
    std::map<uint64_t, ui::RectF> targets;
    for (const WindowButton *button : m_windowButtons) {
        if (button->isVisible())
            targets[button->id] = button->iconTarget();
    }
    return targets;
}

std::vector<uint64_t> PanelTasks::windowButtons() const
{
    std::vector<uint64_t> ids;
    for (const WindowButton *button : m_windowButtons)
        ids.push_back(button->id);
    return ids;
}

} // namespace bayu::shell
