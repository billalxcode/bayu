// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "wayland/selection.h"

struct wl_seat;
struct zwp_primary_selection_device_manager_v1;
struct zwp_primary_selection_device_v1;

namespace bayu::ui::wayland {

struct PrimarySelectionListeners;

// The seat's primary selection, through zwp_primary_selection_v1.
class PrimarySelection final : public Selection
{
public:
    PrimarySelection(Connection& connection, base::EventLoop& loop,
                     zwp_primary_selection_device_manager_v1* manager, wl_seat* seat);
    ~PrimarySelection() override;

protected:
    void* createSource(const std::vector<std::string>& mimeTypes) override;
    void destroySource(void* source) override;
    void setSource(void* source) override;
    void receive(void* offer, const char* mimeType, int fd) override;
    void destroyOffer(void* offer) override;

private:
    friend struct PrimarySelectionListeners;

    zwp_primary_selection_device_manager_v1* m_manager;
    zwp_primary_selection_device_v1* m_device = nullptr;
};

} // namespace bayu::ui::wayland
