// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ui/view.h"

namespace bayu::ui {
class ColumnLayout;
class Label;
}

namespace settings {

class Picture;
class SelectionPreview;

// A choice shown as a picture with its name below, ringed in the accent
// colour while checked.
class ImageSelectionItem : public bayu::ui::View
{
public:
    // A picture filling the preview inside the ring's inset.
    ImageSelectionItem(std::string path, std::string text, bayu::ui::SizeF previewSize);
    // A view of its own size centred in the preview, such as an icon.
    ImageSelectionItem(std::unique_ptr<bayu::ui::View> icon, std::string text, bayu::ui::SizeF previewSize,
                       double previewInset = 4);

    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }

    bayu::ui::SizeF sizeHint() const override;
    double heightForWidth(double width) const override;
    bool acceptsPointer() const override { return true; }

    std::function<void()> onClicked;

protected:
    void layout() override;
    void pointerPress(const bayu::ui::PointerEvent& event) override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;

private:
    void build(std::string text);

    bayu::ui::ColumnLayout* m_layout;
    SelectionPreview* m_preview;
    bayu::ui::Label* m_label;
    bool m_checked = false;
    bool m_pressed = false;
};

} // namespace settings
