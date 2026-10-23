// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "textmetrics.h"

#include "ui/window.h"

#include <cmath>

using namespace bayu::ui;

TextMetrics textMetrics(const Label* label)
{
    return {label->baselineOffset(), label->sizeHint().height, label->capHeight()};
}

double snapToDevice(const View* view, double value)
{
    const Window* window = view->window();
    const double scale = window ? window->scale() : 1.0;
    return std::round(value * scale) / scale;
}
