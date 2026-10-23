// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_SHELL_OSD_LEVEL_TRACK_H_
#define BAYU_SHELL_OSD_LEVEL_TRACK_H_

#include <memory>
#include <string>

#include "ui/view.h"

namespace bayu::ui {
class Animation;
}

namespace bayu::shell {

// A pill track whose fill ends in the knob, the
// icon in its left cap. The on-screen display's, and the control centre's
// sliders'.
class LevelTrack : public ui::View
{
public:
    LevelTrack();
    ~LevelTrack() override;

    // From 0 to 1.
    void setPosition(double position);
    double position() const { return m_target; }
    // What is drawn now, during a glide.
    double drawnPosition() const { return m_drawn; }
    // Artwork for the cap: an SVG file of the light variant, tinted light in
    // dark mode.
    void setIcon(std::string file);
    const std::string &icon() const { return m_icon; }
    // Whether a change of position glides (120 ms, OutCubic) rather than snaps.
    void setAnimated(bool animated) { m_animated = animated; }

    // The icon's side and its distance from the cap's edge.
    static constexpr double kIconSize = 14;

protected:
    void paint(ui::Painter &painter) override;

private:
    double m_target = 0;
    double m_drawn = 0;
    double m_from = 0;
    bool m_animated = false;
    std::string m_icon;
    std::unique_ptr<ui::Animation> m_animation;
};

} // namespace bayu::shell

#endif // BAYU_SHELL_OSD_LEVEL_TRACK_H_
