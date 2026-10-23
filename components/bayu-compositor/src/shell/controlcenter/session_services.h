// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_SHELL_CONTROLCENTER_SESSION_SERVICES_H_
#define BAYU_SHELL_CONTROLCENTER_SESSION_SERVICES_H_

#include <memory>

#include "shell/controlcenter/control_center_services.h"

namespace bayu::shell {

class NotificationSettings;
class Shell;
class StatusBar;

// The session's: NetworkManager and BlueZ on the system bus, PipeWire,
// org.bayu.Privacy's switches, the Appearance service, and the bar's
// backend for MPRIS, brightness, battery and power profiles. `notifications`
// may be null (no Do Not Disturb).
std::unique_ptr<ControlCenterServices> createSessionServices(Shell *shell, StatusBar &statusBar,
                                                             NotificationSettings *notifications);

} // namespace bayu::shell

#endif // BAYU_SHELL_CONTROLCENTER_SESSION_SERVICES_H_
