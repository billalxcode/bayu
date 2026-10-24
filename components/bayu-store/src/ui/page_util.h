// SPDX-License-Identifier: BSD-3-Clause
// Pieces the store's pages share.

#pragma once

#include <string>
#include <vector>

#include "ui/store_window.h"

namespace store {

// Children in columns of equal width, as many as fit at minimumWidth, each
// row as tall as its tallest child.
class Grid : public View
{
public:
    Grid(double minimumWidth, double spacing = 18, int maximumColumns = 4);
    void setColumns(int columns) { m_fixed = columns; }
    double heightForWidth(double width) const override;
    bayu::ui::SizeF sizeHint() const override { return {m_minimum, heightForWidth(m_minimum * 4)}; }

protected:
    void layout() override;

private:
    int columnsFor(double width) const;
    double m_minimum;
    double m_spacing;
    int m_maximum;
    int m_fixed = 0;
};

AppCard* addAppCard(StoreWindow& store, View* parent, const App& app);
AppRow* addAppRow(StoreWindow& store, View* parent, const App& app, AppRow::Options options = {});

// "Categories › Creative".
View* addBreadcrumb(View* parent, const std::vector<std::pair<std::string, std::function<void()>>>& parts);

// "4 min ago", "2 days ago".
std::string ago(int64_t when);

// A thin line across a card.
View* addDivider(View* parent);

} // namespace store
