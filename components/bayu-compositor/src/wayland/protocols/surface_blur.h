// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_WAYLAND_SURFACE_BLUR_H_
#define BAYU_WAYLAND_SURFACE_BLUR_H_

#include <wayland-server-core.h>

namespace bayu::wayland {

wl_global* createSurfaceBlurGlobal(wl_display* display);

} // namespace bayu::wayland

#endif
