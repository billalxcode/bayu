#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/dbus.h"
#include "base/signal.h"
#include "ui/view.h"

namespace bayu::ui {
class DialogButton;
class Label;
class Window;
}

// One open prompt window. The content for the kind supplies the wording and
// the choice details.
class PermissionPrompt
{
public:
    // What a kind shows between the header and the buttons, and its wording.
    class Content : public bayu::ui::View
    {
    public:
        virtual std::string title() const = 0;
        virtual std::string description() const = 0;
        virtual std::string accept() const = 0;
        virtual bool ready() const = 0;
        virtual bayu::base::dbus::VariantMap details() const = 0;
        // Sets its own text again, in the session's new language.
        virtual void retranslate() = 0;

        // A double click on a choice.
        std::function<void()> onAccepted;
    };

    // Known kinds have a content; an unknown kind is refused.
    static bool supports(const std::string &kind);

    PermissionPrompt(const std::string &kind, const std::string &appId, const std::string &parentWindow,
                     const bayu::base::dbus::VariantMap &options, bayu::base::dbus::MethodCall call);
    // Replies "failed" when nothing answered yet.
    ~PermissionPrompt();

    PermissionPrompt(const PermissionPrompt &) = delete;
    PermissionPrompt &operator=(const PermissionPrompt &) = delete;

    std::string caller() const { return m_call.message().sender(); }
    // Dismisses the prompt once the caller leaves the bus.
    void watchCaller(bayu::base::dbus::Connection *bus);
    void show();
    void dismiss();

    void allow();
    void deny();

    bayu::ui::Window *window() const { return m_window.get(); }
    Content *content() const { return m_content; }

    // Answered or dismissed: the prompt can go.
    std::function<void()> onFinished;

    // The name and icon of a desktop entry; the id itself and no icon for an
    // application without one.
    struct Application {
        std::string id;
        std::string name;
        std::string icon;
    };
    static Application describeApplication(const std::string &appId);

private:
    void retranslate();
    void reply(uint32_t response, const bayu::base::dbus::VariantMap &results);

    bayu::base::dbus::MethodCall m_call;
    std::string m_kindIcon;
    std::string m_parentWindow;
    Application m_application;
    bool m_finished = false;
    bayu::base::dbus::NameWatch m_callerWatch;
    std::unique_ptr<bayu::ui::Window> m_window;
    std::unique_ptr<Content> m_ownedContent;
    Content *m_content = nullptr;
    bayu::ui::Label *m_title = nullptr;
    bayu::ui::Label *m_description = nullptr;
    bayu::ui::DialogButton *m_cancel = nullptr;
    bayu::ui::DialogButton *m_accept = nullptr;
    bayu::base::Subscription m_languageChanged;
};
