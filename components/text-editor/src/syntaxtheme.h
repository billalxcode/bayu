// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef SYNTAXTHEME_H
#define SYNTAXTHEME_H

#include "syntax.h"

#include "ui/geometry.h"

// The text area's own colours and one per syntax style, in a light and a dark
// variant that follow the system appearance.
struct SyntaxTheme
{
    bayu::ui::Color background;
    bayu::ui::Color text;
    bayu::ui::Color gutterText;
    bayu::ui::Color currentLine;
    bayu::ui::Color selection;
    bayu::ui::Color match;
    bayu::ui::Color currentMatch;
    bayu::ui::Color bracket;
    bayu::ui::Color whitespace;
    bayu::ui::Color styles[int(SyntaxStyle::Count)];

    static SyntaxTheme theme(bool dark, bayu::ui::Color accent);
    // The background the theme gives the text area, which the window takes.
    static bayu::ui::Color windowBackground(bool dark);
};

#endif
