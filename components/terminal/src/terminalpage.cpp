// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalpage.h"

#include <algorithm>
#include <cmath>

#include "base/event_loop.h"
#include "ui/painter.h"

#include "TerminalSession.h"
#include "TerminalView.h"

#include "terminalpane.h"
#include "terminalwindow.h"

using namespace bayu::ui;

// The gap between two panes, where the divider takes the pointer.
static constexpr double kDividerGrip = 6;
// The smallest a pane gets while a divider is dragged.
static constexpr double kMinimumPane = 80;

// A pane, or two halves of the space, first left of or above the second.
struct TerminalPage::Split
{
    TerminalPane* pane = nullptr;
    bool stacked = false;
    double ratio = 0.5;
    std::unique_ptr<Split> first;
    std::unique_ptr<Split> second;
    Split* parent = nullptr;
    Divider* divider = nullptr;
    RectF rect;
};

// The line between the two halves of a split, dragged to share the space differently.
class TerminalPage::Divider : public View
{
public:
    Divider(TerminalPage* page, Split* split)
        : m_page(page)
    {
        setName("splitDivider");
        setSplit(split);
    }

    // The split it divides, moved up the tree when a pane closes.
    void setSplit(Split* split)
    {
        m_split = split;
        setCursorShape(split->stacked ? CursorShape::ResizeVertical : CursorShape::ResizeHorizontal);
    }

    bool acceptsPointer() const override { return true; }

    void pointerPress(const PointerEvent&) override { m_dragging = true; }

    void pointerMove(const PointerEvent& event) override
    {
        if (!m_dragging)
            return;
        const RectF& rect = m_split->rect;
        const double length = m_split->stacked ? rect.height : rect.width;
        if (length <= 2 * kMinimumPane)
            return;
        // Where the pointer is in the page, across the split.
        const double at = m_split->stacked ? geometry().y + event.position.y - rect.y
                                           : geometry().x + event.position.x - rect.x;
        m_split->ratio = std::clamp(at, kMinimumPane, length - kMinimumPane) / length;
        m_page->requestLayout();
    }

    void pointerRelease(const PointerEvent&) override { m_dragging = false; }

protected:
    void paint(Painter& painter) override
    {
        const RectF bounds = localRect();
        const Color color = m_page->terminalWindow()->chromeForeground().withAlpha(0.15);
        if (m_split->stacked)
            painter.fillRect({0, std::floor(bounds.height / 2), bounds.width, 1}, color);
        else
            painter.fillRect({std::floor(bounds.width / 2), 0, 1, bounds.height}, color);
    }

private:
    TerminalPage* m_page;
    Split* m_split = nullptr;
    bool m_dragging = false;
};

TerminalPage::TerminalPage(TerminalWindow* window, const std::string& path, const std::vector<std::string>& command)
    : m_window(window)
    , m_root(std::make_unique<Split>())
{
    setName("terminalPage");
    m_root->pane = addPane(path, command);
    m_current = m_root->pane;
}

TerminalPage::~TerminalPage()
{
    *m_alive = false;
    m_paneConnections.clear();
}

Konsole::TerminalSession& TerminalPage::session() const
{
    return m_current->session();
}

Konsole::TerminalView* TerminalPage::terminal() const
{
    return m_current->terminal();
}

bool TerminalPage::hasActiveProcess() const
{
    return std::any_of(m_panes.begin(), m_panes.end(),
                       [](const TerminalPane* pane) { return pane->session().hasActiveProcess(); });
}

void TerminalPage::setTerminalWindow(TerminalWindow* window)
{
    m_window = window;
    applySettings();
}

void TerminalPage::setCustomTitle(const std::string& title)
{
    m_customTitle = title;
    titleChanged();
}

std::string TerminalPage::title() const
{
    return !m_customTitle.empty() ? m_customTitle : m_current->session().title();
}

void TerminalPage::applySettings()
{
    for (TerminalPane* pane : m_panes)
        pane->applySettings();
    requestPaint();
}

void TerminalPage::retranslate()
{
    for (TerminalPane* pane : m_panes)
        pane->retranslate();
}

void TerminalPage::startShell()
{
    m_current->startShell();
}

void TerminalPage::openFind()
{
    m_current->openFind();
}

void TerminalPage::focusTerminal()
{
    m_current->focusTerminal();
}

void TerminalPage::dropUrls(const std::vector<std::string>& urls)
{
    m_current->dropUrls(urls);
}

TerminalPane* TerminalPage::addPane(const std::string& path, const std::vector<std::string>& command)
{
    TerminalPane* pane = addChild<TerminalPane>(this, path, command);
    m_panes.push_back(pane);
    attach(pane);
    return pane;
}

void TerminalPage::attach(TerminalPane* pane)
{
    std::vector<bayu::base::Subscription>& connections = m_paneConnections[pane];
    connections.push_back(pane->focused.connect([this, pane] { setCurrent(pane); }));
    connections.push_back(pane->titleChanged.connect([this, pane] {
        if (pane == m_current)
            titleChanged();
    }));
    // Later, not from inside the session's own signal.
    connections.push_back(pane->terminalClosed.connect([this, pane] {
        bayu::base::EventLoop::current()->post([this, alive = std::weak_ptr<bool>(m_alive), pane] {
            if (alive.lock())
                closePane(pane);
        });
    }));
}

void TerminalPage::setCurrent(TerminalPane* pane)
{
    if (pane == m_current)
        return;
    m_current = pane;
    titleChanged();
}

TerminalPage::Split* TerminalPage::leafOf(const TerminalPane* pane) const
{
    std::vector<Split*> pending {m_root.get()};
    while (!pending.empty()) {
        Split* split = pending.back();
        pending.pop_back();
        if (split->pane == pane)
            return split;
        if (split->first)
            pending.push_back(split->first.get());
        if (split->second)
            pending.push_back(split->second.get());
    }
    return nullptr;
}

void TerminalPage::split(TerminalPane* pane, Direction direction)
{
    Split* leaf = leafOf(pane);
    if (!leaf)
        return;

    const std::string directory = pane->session().currentDir();
    TerminalPane* created = addPane(directory.empty() ? "$HOME" : directory, {});

    auto kept = std::make_unique<Split>();
    kept->pane = pane;
    auto added = std::make_unique<Split>();
    added->pane = created;
    const bool before = direction == Direction::Left || direction == Direction::Up;
    leaf->first = before ? std::move(added) : std::move(kept);
    leaf->second = before ? std::move(kept) : std::move(added);
    leaf->first->parent = leaf;
    leaf->second->parent = leaf;
    leaf->pane = nullptr;
    leaf->stacked = direction == Direction::Up || direction == Direction::Down;
    leaf->ratio = 0.5;
    leaf->divider = addChild<Divider>(this, leaf);

    requestLayout();
    created->startShell();
    setCurrent(created);
}

void TerminalPage::closePane(TerminalPane* pane)
{
    Split* leaf = leafOf(pane);
    if (!leaf)
        return;
    if (leaf == m_root.get()) {
        terminalClosed();
        return;
    }

    // The other half takes the parent's place, and with it all of the space.
    Split* parent = leaf->parent;
    std::unique_ptr<Split> sibling = parent->first.get() == leaf ? std::move(parent->second)
                                                                 : std::move(parent->first);
    takeChild(parent->divider).reset();
    parent->pane = sibling->pane;
    parent->stacked = sibling->stacked;
    parent->ratio = sibling->ratio;
    parent->first = std::move(sibling->first);
    parent->second = std::move(sibling->second);
    parent->divider = sibling->divider;
    if (parent->first)
        parent->first->parent = parent;
    if (parent->second)
        parent->second->parent = parent;
    if (parent->divider)
        parent->divider->setSplit(parent);

    m_panes.erase(std::find(m_panes.begin(), m_panes.end(), pane));
    m_paneConnections.erase(pane);
    // It may be closing from its own menu or shortcut: it goes once that returns.
    std::shared_ptr<View> closed(takeChild(pane));
    bayu::base::EventLoop::current()->post([closed] {});

    if (pane == m_current) {
        Split* next = parent;
        while (!next->pane)
            next = next->first.get();
        setCurrent(next->pane);
        next->pane->focusTerminal();
    }
    requestLayout();
}

bool TerminalPage::focusNeighbour(TerminalPane* pane, Direction direction)
{
    const RectF from = pane->geometry();
    TerminalPane* best = nullptr;
    double bestDistance = 0;
    double bestOverlap = 0;
    for (TerminalPane* other : m_panes) {
        if (other == pane)
            continue;
        const RectF to = other->geometry();
        const bool across = direction == Direction::Left || direction == Direction::Right;
        // How far it is that way, and how much of this pane's side it faces.
        double distance = 0;
        switch (direction) {
        case Direction::Left:
            distance = from.x - (to.x + to.width);
            break;
        case Direction::Right:
            distance = to.x - (from.x + from.width);
            break;
        case Direction::Up:
            distance = from.y - (to.y + to.height);
            break;
        case Direction::Down:
            distance = to.y - (from.y + from.height);
            break;
        }
        const double overlap = across
                ? std::min(from.y + from.height, to.y + to.height) - std::max(from.y, to.y)
                : std::min(from.x + from.width, to.x + to.width) - std::max(from.x, to.x);
        if (distance < -1 || overlap <= 0)
            continue;
        if (!best || distance < bestDistance || (distance == bestDistance && overlap > bestOverlap)) {
            best = other;
            bestDistance = distance;
            bestOverlap = overlap;
        }
    }
    if (!best)
        return false;
    best->focusTerminal();
    return true;
}

void TerminalPage::layout()
{
    layoutSplit(*m_root, localRect());
}

void TerminalPage::layoutSplit(Split& split, RectF rect)
{
    split.rect = rect;
    if (split.pane) {
        split.pane->setGeometry(rect);
        return;
    }

    // The divider has a gap of its own between the halves, so a pane added
    // later never covers it.
    const double length = split.stacked ? rect.height : rect.width;
    const double firstLength = std::round(length * split.ratio - kDividerGrip / 2);
    const double secondStart = firstLength + kDividerGrip;
    if (split.stacked) {
        layoutSplit(*split.first, {rect.x, rect.y, rect.width, firstLength});
        layoutSplit(*split.second, {rect.x, rect.y + secondStart, rect.width, rect.height - secondStart});
        split.divider->setGeometry({rect.x, rect.y + firstLength, rect.width, kDividerGrip});
    } else {
        layoutSplit(*split.first, {rect.x, rect.y, firstLength, rect.height});
        layoutSplit(*split.second, {rect.x + secondStart, rect.y, rect.width - secondStart, rect.height});
        split.divider->setGeometry({rect.x + firstLength, rect.y, kDividerGrip, rect.height});
    }
}
