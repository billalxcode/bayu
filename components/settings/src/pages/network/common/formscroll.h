// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <memory>

#include "ui/scroll_view.h"

namespace settings::network {

// A long form in a dialog: as tall as the form up to a limit, then scrolling.
// It reaches `bleed` past its own sides, so the scroll bar runs along the
// container's edge rather than the fields', which keep their inset.
class FormScroll : public bayu::ui::View
{
public:
    FormScroll(std::unique_ptr<bayu::ui::View> form, double bleed);

    bayu::ui::View* form() const { return m_form; }
    void scrollToTop();

    // The tallest it may be; unbounded while unset.
    std::function<double()> maximumHeight;

    bayu::ui::SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void layout() override;

private:
    double m_bleed;
    bayu::ui::ScrollView* m_scroll;
    bayu::ui::View* m_form;
};

} // namespace settings::network
