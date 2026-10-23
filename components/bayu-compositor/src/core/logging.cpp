// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/logging.h"

namespace bayu::core {

void setLoggingRules(std::string_view rules)
{
    base::logging::setRules(rules);
}

} // namespace bayu::core

const bayu::core::LogCategory cfWayland("bayu.wayland");
const bayu::core::LogCategory cfSession("bayu.session");
const bayu::core::LogCategory cfDrm("bayu.drm");
const bayu::core::LogCategory cfRender("bayu.render");
const bayu::core::LogCategory cfScene("bayu.scene");
const bayu::core::LogCategory cfCompositor("bayu.compositor");
const bayu::core::LogCategory cfInput("bayu.input");
const bayu::core::LogCategory cfInputTransitions("bayu.input.transitions");
const bayu::core::LogCategory cfCursorTransitions("bayu.cursor.transitions");
