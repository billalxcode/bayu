// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_WAYLAND_WINDOW_HINTS_H
#define BAYU_WAYLAND_WINDOW_HINTS_H

#include <wayland-server-core.h>

namespace bayu::wayland {

wl_global *createWindowHintsGlobal(wl_display *display);

// bayu_window_hints_v1.interactive_resize, to every manager the surface's
// client has bound.
void sendInteractiveResize(wl_resource *surface, bool active);

} // namespace bayu::wayland

#endif
