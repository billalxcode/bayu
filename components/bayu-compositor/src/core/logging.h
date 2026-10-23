// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_CORE_LOGGING_H_
#define BAYU_CORE_LOGGING_H_

#include <string_view>

#include "base/logging.h"

namespace bayu::core {

// Written to with CF_DEBUG(cfInput) and friends.
using LogCategory = base::LogCategory;

// Applies `rules` (base::logging::setRules syntax) to every category.
void setLoggingRules(std::string_view rules);

} // namespace bayu::core

// Startup and operational messages are on; routine window and input
// diagnostics need a debug rule (--log-rules).
extern const bayu::core::LogCategory cfWayland;
extern const bayu::core::LogCategory cfSession;
extern const bayu::core::LogCategory cfDrm;
extern const bayu::core::LogCategory cfRender;
extern const bayu::core::LogCategory cfScene;
extern const bayu::core::LogCategory cfCompositor;
extern const bayu::core::LogCategory cfInput;
// Opt-in button/capture transitions; no pointer-motion or keyboard text logging.
extern const bayu::core::LogCategory cfInputTransitions;
extern const bayu::core::LogCategory cfCursorTransitions;

#endif // BAYU_CORE_LOGGING_H_
