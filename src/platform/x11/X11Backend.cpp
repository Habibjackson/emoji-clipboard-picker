#include "X11Backend.h"

#include "app/AppController.h"

#include <QQuickWindow>
#include <QSocketNotifier>
#include <QTimer>

#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>

#include <algorithm>
#include <array>
#include <utility>

namespace {
Display *trappedDisplay = nullptr;
int trappedError = 0;

int captureXError(Display *display, XErrorEvent *event)
{
    if (display == trappedDisplay)
        trappedError = event->error_code;
    return 0;
}

template <typename Function>
bool checkedXRequest(Display *display, Function function)
{
    XSync(display, False);
    trappedDisplay = display;
    trappedError = 0;
    const auto previous = XSetErrorHandler(captureXError);
    function();
    XSync(display, False);
    XSetErrorHandler(previous);
    trappedDisplay = nullptr;
    return trappedError == 0;
}

constexpr std::array terminalClasses = {
    "alacritty", "foot", "gnome-terminal", "kitty", "konsole",
    "org.gnome.console", "org.gnome.ptyxis", "terminator", "tilix",
};
}

X11Backend::X11Backend(QQuickWindow *window, AppController *controller, QObject *parent)
    : DesktopIntegration(window, controller, parent)
{
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        m_error = tr("Could not open the X11 display");
        return;
    }
    int errorBase = 0;
    int eventBase = 0;
    int versionMajor = 0;
    int versionMinor = 0;
    if (!XFixesQueryExtension(m_display, &m_xfixesEventBase, &errorBase)
        || !XTestQueryExtension(m_display, &eventBase, &errorBase,
                                &versionMajor, &versionMinor)) {
        m_error = tr("XFixes or XTest is unavailable on this X11 server");
        return;
    }

    m_root = DefaultRootWindow(m_display);
    const Atom clipboard = XInternAtom(m_display, "CLIPBOARD", False);
    if (!checkedXRequest(m_display, [this, clipboard] {
            XFixesSelectSelectionInput(m_display, m_root, clipboard,
                                       XFixesSetSelectionOwnerNotifyMask);
        })) {
        m_error = tr("Could not watch the X11 clipboard");
        return;
    }
    m_emojiKeycode = XKeysymToKeycode(m_display, XK_period);
    m_clipboardKeycode = XKeysymToKeycode(m_display, XK_v);
    m_numLockMask = numLockMask();
    if (!m_emojiKeycode || !m_clipboardKeycode) {
        m_error = tr("The X11 keyboard layout does not provide the required keys");
        return;
    }
    m_emojiGrabbed = grabShortcut(m_emojiKeycode);
    m_clipboardGrabbed = grabShortcut(m_clipboardKeycode);
    if (!m_emojiGrabbed && m_controller)
        m_controller->reportShortcutFailure(QStringLiteral("Super+."));
    if (!m_clipboardGrabbed && m_controller)
        m_controller->reportShortcutFailure(QStringLiteral("Super+V"));

    m_notifier = new QSocketNotifier(XConnectionNumber(m_display), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &X11Backend::processEvents);
    connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (!visible) {
            QTimer::singleShot(0, this, [this] {
                if (!m_pastePending && (!m_window || !m_window->isVisible()))
                    cancel();
            });
        }
    });
    connect(m_controller, &AppController::itemCopied, this, [this] {
        if (!m_shortcutArmed) {
            if (m_controller)
                m_controller->discardEmojiClipboardSnapshot();
            return;
        }
        m_shortcutArmed = false;
        m_pastePending = true;
        const bool emoji = m_controller && m_controller->hasEmojiClipboardSnapshot();
        const quint64 generation = m_controller
            ? m_controller->emojiClipboardSnapshotGeneration() : 0;
        QTimer::singleShot(0, this, [this, emoji, generation] {
            pasteSelection(emoji, generation);
        });
    });
    XFlush(m_display);
}

X11Backend::~X11Backend()
{
    if (!m_display)
        return;
    if (m_notifier)
        m_notifier->setEnabled(false);
    if (m_emojiGrabbed)
        releaseShortcut(m_emojiKeycode);
    if (m_clipboardGrabbed)
        releaseShortcut(m_clipboardKeycode);
    XCloseDisplay(m_display);
}

bool X11Backend::isAvailable() const
{
    return m_notifier != nullptr;
}

QString X11Backend::errorString() const
{
    return m_error;
}

quint64 X11Backend::clipboardEventCount() const
{
    return m_clipboardEvents;
}

void X11Backend::cancel()
{
    m_shortcutArmed = false;
    m_target = None;
}

void X11Backend::captureClipboard(const QString &)
{
    if (m_controller)
        QTimer::singleShot(0, m_controller, &AppController::captureNow);
}

void X11Backend::processEvents()
{
    while (XPending(m_display)) {
        XEvent event;
        XNextEvent(m_display, &event);
        if (event.type == m_xfixesEventBase + XFixesSelectionNotify) {
            ++m_clipboardEvents;
            emit clipboardOwnerChanged();
            captureClipboard(QString());
        } else if (event.type == KeyPress && (event.xkey.state & Mod4Mask)) {
            if (m_emojiGrabbed && event.xkey.keycode == m_emojiKeycode) {
                activateShortcutView(QStringLiteral("emoji"));
            } else if (m_clipboardGrabbed && event.xkey.keycode == m_clipboardKeycode) {
                activateShortcutView(QStringLiteral("clipboard"));
            }
        }
    }
}

void X11Backend::activateShortcutView(const QString &view)
{
    Window focused = None;
    int revert = 0;
    XGetInputFocus(m_display, &focused, &revert);
    const bool pickerFocused = m_window && m_window->isVisible()
        && focused == Window(m_window->winId());
    if (pickerFocused) {
        if (m_window->property("currentView").toString() == view) {
            cancel();
            m_window->hide();
        } else {
            openView(view);
        }
    } else {
        rememberTarget();
        openView(view);
    }
    emit shortcutTriggered(view);
}

bool X11Backend::rememberTarget()
{
    if (!m_window)
        return false;
    Window focused = None;
    int revert = 0;
    XGetInputFocus(m_display, &focused, &revert);
    m_target = focused == Window(m_window->winId()) || focused == PointerRoot
        || focused == m_root ? None : focused;
    m_shortcutArmed = true;
    return m_target != None;
}

void X11Backend::openView(const QString &view)
{
    showWindow(view);
}

void X11Backend::pasteSelection(bool, quint64 snapshotGeneration)
{
    m_pastePending = false;
    const Window target = std::exchange(m_target, None);
    if (m_controller && m_controller->emojiClipboardSnapshotGeneration()
        != snapshotGeneration) {
        return;
    }
    if (m_controller)
        m_controller->discardEmojiClipboardSnapshot();
    if (!m_controller || !target || !windowExists(target)) {
        reportFailure(QStringLiteral("target-missing"));
        return;
    }
    const bool terminal = targetIsTerminal(target);
    const auto control = XKeysymToKeycode(m_display, XK_Control_L);
    const auto shift = XKeysymToKeycode(m_display, XK_Shift_L);
    const auto v = XKeysymToKeycode(m_display, XK_v);
    if (!control || !v || (terminal && !shift)) {
        reportFailure(QStringLiteral("input-failed"));
        return;
    }
    if (!checkedXRequest(m_display, [this, target] {
            XRaiseWindow(m_display, target);
            XSetInputFocus(m_display, target, RevertToPointerRoot, CurrentTime);
        })) {
        reportFailure(QStringLiteral("focus-failed"));
        return;
    }
    bool sent = XTestFakeKeyEvent(m_display, control, True, CurrentTime);
    if (terminal)
        sent = XTestFakeKeyEvent(m_display, shift, True, CurrentTime) && sent;
    sent = XTestFakeKeyEvent(m_display, v, True, CurrentTime) && sent;
    sent = XTestFakeKeyEvent(m_display, v, False, CurrentTime) && sent;
    if (terminal)
        sent = XTestFakeKeyEvent(m_display, shift, False, CurrentTime) && sent;
    sent = XTestFakeKeyEvent(m_display, control, False, CurrentTime) && sent;
    if (!sent) {
        reportFailure(QStringLiteral("input-failed"));
        return;
    }
    XFlush(m_display);
    const QString shortcut = terminal ? QStringLiteral("Ctrl+Shift+V") : QStringLiteral("Ctrl+V");
    m_controller->reportPasteResult(QStringLiteral("sent"), shortcut);
    emit pasteSent(shortcut);
}

bool X11Backend::grabShortcut(unsigned int keycode)
{
    std::array<unsigned int, 4> modifiers = {Mod4Mask, Mod4Mask | LockMask,
                                             Mod4Mask | m_numLockMask,
                                             Mod4Mask | LockMask | m_numLockMask};
    std::sort(modifiers.begin(), modifiers.end());
    const auto end = std::unique(modifiers.begin(), modifiers.end());
    for (auto it = modifiers.begin(); it != end; ++it) {
        if (!checkedXRequest(m_display, [this, keycode, mask = *it] {
                XGrabKey(m_display, int(keycode), mask, m_root, False,
                         GrabModeAsync, GrabModeAsync);
            })) {
            releaseShortcut(keycode);
            return false;
        }
    }
    return true;
}

void X11Backend::releaseShortcut(unsigned int keycode)
{
    XUngrabKey(m_display, int(keycode), AnyModifier, m_root);
    XFlush(m_display);
}

bool X11Backend::windowExists(unsigned long window) const
{
    XWindowAttributes attributes;
    int found = 0;
    const bool noError = checkedXRequest(m_display, [this, window, &attributes, &found] {
        found = XGetWindowAttributes(m_display, window, &attributes);
    });
    return noError && found;
}

bool X11Backend::targetIsTerminal(unsigned long window) const
{
    XClassHint hint{};
    int found = 0;
    const bool noError = checkedXRequest(m_display, [this, window, &hint, &found] {
        found = XGetClassHint(m_display, window, &hint);
    });
    if (!noError || !found) {
        if (hint.res_name)
            XFree(hint.res_name);
        if (hint.res_class)
            XFree(hint.res_class);
        return false;
    }
    const QString name = QString::fromUtf8(hint.res_name ? hint.res_name : "").toLower();
    const QString klass = QString::fromUtf8(hint.res_class ? hint.res_class : "").toLower();
    if (hint.res_name)
        XFree(hint.res_name);
    if (hint.res_class)
        XFree(hint.res_class);
    return std::any_of(terminalClasses.begin(), terminalClasses.end(),
                       [&name, &klass](const char *id) { return name == id || klass == id; });
}

unsigned int X11Backend::numLockMask() const
{
    unsigned int mask = 0;
    XModifierKeymap *mapping = XGetModifierMapping(m_display);
    if (!mapping)
        return mask;
    for (int modifier = 0; modifier < 8; ++modifier) {
        for (int key = 0; key < mapping->max_keypermod; ++key) {
            const KeyCode code = mapping->modifiermap[modifier * mapping->max_keypermod + key];
            if (code && XkbKeycodeToKeysym(m_display, code, 0, 0) == XK_Num_Lock)
                mask |= 1u << modifier;
        }
    }
    XFreeModifiermap(mapping);
    return mask;
}
