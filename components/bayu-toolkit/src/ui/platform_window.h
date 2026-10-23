// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ui/drag.h"
#include "ui/event.h"
#include "ui/geometry.h"
#include "ui/path.h"
#include "ui/window.h"

class SkCanvas;

namespace bayu::ui {

class Window;

// The window-system side of a Window: its surface, its buffers and the events
// that come back. Window does layout, painting, pacing and input routing.
class PlatformWindow
{
public:
    // Draws into a buffer that holds the frame presented `bufferAge` frames
    // ago, or anything when 0, and returns the device rects that changed.
    using PaintFunction = std::function<std::vector<Rect>(SkCanvas* canvas, int bufferAge)>;

    // Headless windows report glass as a compositor that draws it would.
    static void setHeadlessGlassAvailable(bool available);
    // For the application's platform; a popup when it has a parent.
    static std::unique_ptr<PlatformWindow> create(Window* window, PlatformWindow* parent,
                                                  const PopupPlacement& placement, bool grab);

    explicit PlatformWindow(Window* window)
        : m_window(window)
    {
    }
    virtual ~PlatformWindow() = default;

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

    // Headless windows have no frame clock; they draw only when grabbed.
    virtual bool isHeadless() const = 0;
    // Rendered when the host process asks, see EmbeddedWindow.
    virtual bool isEmbedded() const { return false; }
    // Embedded only: tells the host a frame is wanted.
    virtual void requestRender() {}

    virtual void setTitle(const std::string& title) = 0;
    virtual void setAppId(const std::string& appId) = 0;
    virtual void setMinimumSize(int width, int height) = 0;
    virtual void setMaximumSize(int width, int height) = 0;
    virtual void setMaximized(bool maximized) = 0;
    virtual void setFullScreen(bool fullScreen) = 0;
    virtual void minimize() = 0;
    // Asks the compositor to focus and raise the window.
    virtual void activate() {}
    // As activate(), with an xdg-activation token from another process.
    virtual void activateWithToken(const std::string&) { activate(); }
    // "wayland:<handle>" for other clients to parent to; empty when none.
    virtual void exportHandle(std::function<void(const std::string&)> done) { done({}); }
    // An xdg-activation token for a process this one starts; empty when none.
    virtual void requestActivationToken(std::function<void(const std::string&)> done) { done({}); }
    // The logical size of the screen the window is on; empty when unknown.
    virtual SizeF screenSize() const { return {}; }
    virtual void setTransientParent(PlatformWindow* parent, bool modal) = 0;
    // The size a window shown maximized returns to; before show().
    virtual void setRestoreSize(int, int) {}
    virtual void show(int width, int height) = 0;
    // A popup: placed again at the size, see Window::setPopupPlacement().
    virtual void setPopupPlacement(const PopupPlacement&, int /*width*/, int /*height*/) {}
    // Embedded only: off screen until destroyed.
    virtual void hide() {}
    virtual void startMove() = 0;
    // Hands the pointer to a drag; false when the platform cannot drag.
    virtual bool startDrag(DragSource) { return false; }
    virtual void setCursorShape(CursorShape shape) = 0;
    virtual void setBlur(const RectF& rect, double radius) = 0;
    // The outline masking the blur.
    virtual void setBlurShape(const Path&) {}
    // Wayland only: glass instead of the plain blur, or none.
    virtual void setGlass(const std::optional<GlassMaterial>&) {}
    // Whether the compositor draws blur and glass behind this window.
    virtual bool glassAvailable() const { return false; }
    virtual void setResizeAroundCenter(bool) {}
    virtual void setSkipTaskbar(bool) {}
    virtual void setAlwaysOnTop(bool) {}
    virtual void setRestoreId(const std::string&) {}
    virtual void setForeignParent(const std::string&, bool) {}
    // On the trusted connection: bayu_shell_v1's desktop role.
    virtual bool setDesktopRole(const std::string&) { return false; }
    virtual void setShellOutput(const std::string&) {}
    virtual void activateShellSurface() {}
    // The focused text view's state for input methods, or none.
    virtual void setTextInput(const TextInputState* state) = 0;

    // Draws a frame of the logical size at the scale and presents it.
    virtual bool present(SizeF size, double scale, bool opaque, const PaintFunction& paint) = 0;
    // Headless only: the buffer last presented.
    virtual const Image* presentedImage() const { return nullptr; }
    // Headless only: the moves the window asked for.
    virtual int moveRequests() const { return 0; }
    // Headless only: the glass last asked for.
    virtual std::optional<GlassMaterial> requestedGlass() const { return std::nullopt; }

protected:
    // Another window's platform side, such as a window a drag carries.
    static PlatformWindow* platformOf(Window* window);

    void notifyConfigure(int width, int height, const WindowStates& states);
    void notifyConfigureBounds(int width, int height);
    void notifyInteractiveResize(bool active);
    void notifyScaleChanged(double scale);
    void notifyCloseRequested();
    void notifyFrameDone();
    void notifyPresented(uint64_t timestampNs, uint32_t refreshNs);
    void notifyDiscarded();
    void notifyPointerEnter(double x, double y);
    void notifyPointerLeave();
    void notifyPointerMotion(double x, double y);
    void notifyPointerButton(MouseButton button, bool pressed, uint32_t modifiers);
    void notifyPointerScroll(const ScrollEvent& event);
    void notifyKeyboardEnter();
    void notifyKeyboardLeave();
    void notifyKey(const KeyEvent& event, bool pressed);
    void notifyHold();
    void notifyTouchDown(int32_t id, double x, double y);
    void notifyTouchMotion(int32_t id, double x, double y);
    void notifyTouchUp(int32_t id);
    void notifyTouchCancel();
    void notifyTextInput(const TextInputEvent& event);
    uint32_t notifyDragEnter(double x, double y, DropOffer* offer, uint32_t modifiers);
    uint32_t notifyDragMotion(double x, double y, uint32_t modifiers);
    void notifyDragLeave();
    uint32_t notifyDrop();
    void notifyWorkArea(const RectF& area);

    Window* m_window;
};

} // namespace bayu::ui
