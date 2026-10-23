// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "base/geometry.h"
#include "ui/geometry.h"

namespace bayu::ui {

class Painter;
class Window;
struct AnimatedImagePrivate;

// An animated picture: GIF and animated WebP frame by frame, everything
// else the toolkit reads as one still frame (APNG included). Decoded off the UI thread,
// paced on the window's frame clock, so it stops while nothing is drawn.
// Draw the current frame wherever it goes with draw().
class AnimatedImage
{
public:
    explicit AnimatedImage(Window* window);
    ~AnimatedImage();

    AnimatedImage(const AnimatedImage&) = delete;
    AnimatedImage& operator=(const AnimatedImage&) = delete;

    // Starts decoding; onLoaded follows on the UI thread.
    void load(const std::string& path);
    bool isLoaded() const;
    // The first frame's size in pixels, for fitting; empty until loaded.
    base::Size size() const;
    int frameCount() const;
    int currentFrame() const;
    // The file's loop count: -1 forever, 0 played once.
    int loopCount() const;

    // Plays from the current frame (the default once loaded) or holds it.
    void setPlaying(bool playing);
    bool isPlaying() const;
    void jumpToFrame(int frame);

    // The current frame filling the rect, filtered when `smooth`.
    void draw(Painter& painter, const RectF& rect, bool smooth = true) const;

    std::function<void(bool ok)> onLoaded;
    // A new frame is current: repaint.
    std::function<void()> onFrameChanged;

private:
    std::unique_ptr<AnimatedImagePrivate> d;
};

} // namespace bayu::ui
