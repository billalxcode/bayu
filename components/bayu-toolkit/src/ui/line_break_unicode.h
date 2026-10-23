// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "include/core/SkRefCnt.h"

class SkUnicode;

namespace bayu::ui {

// ICU for everything but soft line breaks, which come from line_breaks.h.
sk_sp<SkUnicode> makeLineBreakUnicode(sk_sp<SkUnicode> icu);

} // namespace bayu::ui
