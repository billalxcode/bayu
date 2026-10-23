// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_SHELL_SECRETS_VPN_SECRET_FIELDS_H_
#define BAYU_SHELL_SECRETS_VPN_SECRET_FIELDS_H_

#include <map>
#include <string>
#include <vector>

namespace bayu::shell {

struct VpnSecretField {
    std::string key;
    std::string label;
    bool secret = true;
};

// What to ask for to connect a VPN, as the KDE and GNOME authentication
// dialogs decide it: the hinted secrets when NetworkManager names them, else
// the plugin's own rules (bayu-framework's VpnUtils::secretFields).
std::vector<VpnSecretField> vpnSecretFields(const std::string &serviceType,
                                            const std::map<std::string, std::string> &data,
                                            const std::vector<std::string> &hints);
// The server's message among the hints, if it sent one.
std::string vpnHintMessage(const std::vector<std::string> &hints);

} // namespace bayu::shell

#endif // BAYU_SHELL_SECRETS_VPN_SECRET_FIELDS_H_
