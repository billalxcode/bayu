// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/view.h"

namespace bayu::ui::test {

// A view with a fixed size hint.
class Box : public View
{
public:
    explicit Box(SizeF size)
        : m_size(size)
    {
    }

    SizeF sizeHint() const override { return m_size; }

private:
    SizeF m_size;
};

} // namespace bayu::ui::test
