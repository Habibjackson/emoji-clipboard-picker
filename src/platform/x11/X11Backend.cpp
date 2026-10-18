#include "X11Backend.h"

#include "app/AppController.h"
#include "platform/ShortcutSettings.h"

#include <QQuickWindow>
#include <QClipboard>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
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

unsigned long singleProperty(Display *display, Window window, const char *name,
                             Atom expectedType)
{
    const Atom property = XInternAtom(display, name, True);
    if (property == None)
        return 0;
    Atom actualType = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char *data = nullptr;
    int result = BadValue;
    const bool noError = checkedXRequest(display, [&] {
        result = XGetWindowProperty(display, window, property, 0, 1, False,
                                    expectedType, &actualType, &format, &count,
                                    &remaining, &data);
    });
    unsigned long value = 0;
    if (noError && result == Success && actualType == expectedType
        && format == 32 && count == 1 && data)
        value = *reinterpret_cast<unsigned long *>(data);
    if (data)
        XFree(data);
    return value;
}

constexpr std::array terminalClasses = {
    "alacritty", "com.mitchellh.ghostty", "foot", "gnome-terminal",
    "io.elementary.terminal", "kitty", "konsole", "org.gnome.console",
    "org.gnome.ptyxis", "org.gnome.terminal", "org.wezfurlong.wezterm",
    "ptyxis", "terminator", "tilix",
};

unsigned int xModifiers(Qt::KeyboardModifiers modifiers)
{
    unsigned int mask = 0;
    if (modifiers & Qt::MetaModifier)
        mask |= Mod4Mask;
    if (modifiers & Qt::ControlModifier)
        mask |= ControlMask;
    if (modifiers & Qt::AltModifier)
        mask |= Mod1Mask;
    if (modifiers & Qt::ShiftModifier)
        mask |= ShiftMask;
    return mask;
}
}

X11Backend::X11Backend(QQuickWindow *window, AppController *controller,
                       ShortcutSettings *shortcuts, QObject *parent)
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
    m_clipboardSelection = XInternAtom(m_display, "CLIPBOARD", False);
    if (!checkedXRequest(m_display, [this] {
            XFixesSelectSelectionInput(m_display, m_root, m_clipboardSelection,
                                       XFixesSetSelectionOwnerNotifyMask);
        })) {
        m_error = tr("Could not watch the X11 clipboard");
        return;
    }
    const ShortcutSpec emoji = shortcuts ? shortcuts->spec(QStringLiteral("emoji"))
                                         : ShortcutSpec::emojiDefault();
    const ShortcutSpec clipboard = shortcuts ? shortcuts->spec(QStringLiteral("clipboard"))
                                             : ShortcutSpec::clipboardDefault();
    m_emojiKeycode = XKeysymToKeycode(m_display,
        XStringToKeysym(emoji.keyName.toLatin1().constData()));
    m_clipboardKeycode = XKeysymToKeycode(m_display,
        XStringToKeysym(clipboard.keyName.toLatin1().constData()));
    m_emojiModifiers = xModifiers(emoji.modifiers);
    m_clipboardModifiers = xModifiers(clipboard.modifiers);
    m_emojiLabel = emoji.label;
    m_clipboardLabel = clipboard.label;
    m_numLockMask = numLockMask();
    m_emojiGrabbed = m_emojiKeycode && grabShortcut(m_emojiKeycode, m_emojiModifiers);
    m_clipboardGrabbed = m_clipboardKeycode
        && grabShortcut(m_clipboardKeycode, m_clipboardModifiers);
    if (!m_emojiGrabbed && m_controller)
        m_controller->reportShortcutFailure(m_emojiLabel);
    if (!m_clipboardGrabbed && m_controller)
        m_controller->reportShortcutFailure(m_clipboardLabel);

    m_notifier = new QSocketNotifier(XConnectionNumber(m_display), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &X11Backend::processEvents);
    connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (!visible) {
            QTimer::singleShot(0, this, [this] {
                if (m_window && !m_window->isVisible())
                    cancel();
            });
        }
    });
    connect(m_controller, &AppController::itemCopied, this, [this] {
        if (!m_shortcutArmed) {
            if (m_pastePending)
                cancel();
            if (m_controller)
                m_controller->discardEmojiClipboardSnapshot();
            return;
        }
        m_shortcutArmed = m_window && m_window->isVisible();
        m_pastePending = true;
        XSync(m_display, False);
        processEvents();
        m_selectedClipboardOwner = XGetSelectionOwner(m_display, m_clipboardSelection);
        m_selectedClipboardRevision = m_clipboardEvents;
        const quint64 pasteGeneration = ++m_pasteGeneration;
        const bool emoji = m_controller && m_controller->hasEmojiClipboardSnapshot();
        const quint64 generation = m_controller
            ? m_controller->emojiClipboardSnapshotGeneration() : 0;
        QTimer::singleShot(0, this, [this, emoji, generation, pasteGeneration] {
            if (pasteGeneration == m_pasteGeneration)
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
        releaseShortcut(m_emojiKeycode, m_emojiModifiers);
    if (m_clipboardGrabbed)
        releaseShortcut(m_clipboardKeycode, m_clipboardModifiers);
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

bool X11Backend::setShortcut(const QString &view, const ShortcutSpec &spec)
{
    if (!isAvailable() || !spec.isValid()
        || (view != QStringLiteral("emoji") && view != QStringLiteral("clipboard")))
        return false;
    const auto keycode = XKeysymToKeycode(m_display,
        XStringToKeysym(spec.keyName.toLatin1().constData()));
    if (!keycode)
        return false;
    const unsigned int modifiers = xModifiers(spec.modifiers);
    const bool emoji = view == QStringLiteral("emoji");
    unsigned int &oldKeycode = emoji ? m_emojiKeycode : m_clipboardKeycode;
    unsigned int &oldModifiers = emoji ? m_emojiModifiers : m_clipboardModifiers;
    bool &oldGrabbed = emoji ? m_emojiGrabbed : m_clipboardGrabbed;
    QString &oldLabel = emoji ? m_emojiLabel : m_clipboardLabel;
    if (keycode == (emoji ? m_clipboardKeycode : m_emojiKeycode)
        && modifiers == (emoji ? m_clipboardModifiers : m_emojiModifiers))
        return false;
    if (keycode == oldKeycode && modifiers == oldModifiers) {
        if (oldGrabbed)
            return true;
        oldGrabbed = grabShortcut(keycode, modifiers);
        if (oldGrabbed && m_controller)
            m_controller->clearShortcutFailure(oldLabel);
        return oldGrabbed;
    }
    XGrabServer(m_display);
    if (oldGrabbed)
        releaseShortcut(oldKeycode, oldModifiers);
    if (!grabShortcut(keycode, modifiers)) {
        if (oldGrabbed && !grabShortcut(oldKeycode, oldModifiers)) {
            oldGrabbed = false;
            if (m_controller)
                m_controller->reportShortcutFailure(oldLabel);
        }
        XUngrabServer(m_display);
        XFlush(m_display);
        return false;
    }
    XUngrabServer(m_display);
    XFlush(m_display);
    if (m_controller)
        m_controller->clearShortcutFailure(oldLabel);
    oldKeycode = keycode;
    oldModifiers = modifiers;
    oldLabel = spec.label;
    oldGrabbed = true;
    return true;
}

void X11Backend::cancel()
{
    ++m_pasteGeneration;
    if (m_focusTimer) {
        m_focusTimer->stop();
        m_focusTimer->deleteLater();
        m_focusTimer = nullptr;
    }
    if (m_pastePending && m_controller)
        m_controller->discardEmojiClipboardSnapshot();
    m_pastePending = false;
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
        } else if (event.type == KeyPress) {
            const unsigned int modifiers = event.xkey.state
                & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);
            m_lastShortcutTime = event.xkey.time;
            if (m_emojiGrabbed && event.xkey.keycode == m_emojiKeycode
                && modifiers == m_emojiModifiers) {
                activateShortcutView(QStringLiteral("emoji"));
            } else if (m_clipboardGrabbed && event.xkey.keycode == m_clipboardKeycode
                       && modifiers == m_clipboardModifiers) {
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
        cancel();
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
    const bool pickerFocused = focused == Window(m_window->winId());
    const Window active = hasEwmhWindowManager()
        ? singleProperty(m_display, m_root, "_NET_ACTIVE_WINDOW", XA_WINDOW) : None;
    m_target = pickerFocused || focused == PointerRoot || focused == m_root
        ? None : active && active != Window(m_window->winId()) && windowExists(active)
            ? active : focused;
    m_shortcutArmed = true;
    return m_target != None;
}

void X11Backend::openView(const QString &view)
{
    showWindow(view);
}

void X11Backend::pasteSelection(bool, quint64 snapshotGeneration)
{
    const Window target = m_target;
    if (!m_shortcutArmed)
        m_target = None;
    if (m_controller && m_controller->emojiClipboardSnapshotGeneration()
        != snapshotGeneration) {
        m_pastePending = false;
        return;
    }
    if (!m_controller || !target || !windowExists(target)) {
        m_pastePending = false;
        if (m_controller)
            m_controller->discardEmojiClipboardSnapshot();
        reportFailure(QStringLiteral("target-missing"));
        return;
    }
    const QString shortcut = m_controller->pasteShortcutFor(targetIsTerminal(target));
    if (!hasEwmhWindowManager()) {
        if (!checkedXRequest(m_display, [this, target] {
                XRaiseWindow(m_display, target);
                XSetInputFocus(m_display, target, RevertToPointerRoot, CurrentTime);
            })) {
            m_pastePending = false;
            m_controller->discardEmojiClipboardSnapshot();
            reportFailure(QStringLiteral("focus-failed"));
            return;
        }
    } else {
        const Atom activeWindow = XInternAtom(m_display, "_NET_ACTIVE_WINDOW", False);
        XEvent request{};
        request.xclient.type = ClientMessage;
        request.xclient.window = target;
        request.xclient.message_type = activeWindow;
        request.xclient.format = 32;
        request.xclient.data.l[0] = 1;
        const unsigned long userTime = m_window
            ? singleProperty(m_display, Window(m_window->winId()),
                             "_NET_WM_USER_TIME", XA_CARDINAL) : 0;
        request.xclient.data.l[1] = long(userTime ? userTime : m_lastShortcutTime);
        request.xclient.data.l[2] = m_window ? long(m_window->winId()) : 0;
        if (!checkedXRequest(m_display, [this, &request] {
                XSendEvent(m_display, m_root, False,
                           SubstructureRedirectMask | SubstructureNotifyMask, &request);
            })) {
            m_pastePending = false;
            m_controller->discardEmojiClipboardSnapshot();
            reportFailure(QStringLiteral("focus-failed"));
            return;
        }
    }

    QElapsedTimer started;
    started.start();
    const quint64 pasteGeneration = m_pasteGeneration;
    auto *timer = new QTimer(this);
    m_focusTimer = timer;
    timer->setInterval(25);
    connect(timer, &QTimer::timeout, this,
            [this, timer, target, emoji = m_controller->hasEmojiClipboardSnapshot(),
             snapshotGeneration, shortcut, started, pasteGeneration] {
        if (pasteGeneration != m_pasteGeneration) {
            timer->stop();
            timer->deleteLater();
            return;
        }
        if (!windowExists(target)) {
            timer->stop();
            timer->deleteLater();
            m_focusTimer = nullptr;
            m_pastePending = false;
            m_controller->discardEmojiClipboardSnapshot();
            reportFailure(QStringLiteral("target-missing"));
            return;
        }
        if (targetHasFocus(target)) {
            timer->stop();
            timer->deleteLater();
            m_focusTimer = nullptr;
            m_pastePending = false;
            sendPaste(target, emoji, snapshotGeneration, shortcut);
        } else if (started.elapsed() >= 750) {
            timer->stop();
            timer->deleteLater();
            m_focusTimer = nullptr;
            m_pastePending = false;
            m_controller->discardEmojiClipboardSnapshot();
            reportFailure(QStringLiteral("focus-failed"));
        }
    });
    timer->start();
}

void X11Backend::sendPaste(unsigned long target, bool emoji, quint64 snapshotGeneration,
                           const QString &shortcut)
{
    if (!m_controller || m_controller->emojiClipboardSnapshotGeneration()
        != snapshotGeneration)
        return;
    XSync(m_display, False);
    processEvents();
    if (!QGuiApplication::clipboard()->ownsClipboard()
        || !m_selectedClipboardOwner
        || XGetSelectionOwner(m_display, m_clipboardSelection) != m_selectedClipboardOwner
        || m_clipboardEvents != m_selectedClipboardRevision) {
        m_controller->discardEmojiClipboardSnapshot();
        reportFailure(QStringLiteral("clipboard-changed"));
        return;
    }
    if (!targetHasFocus(target)) {
        m_controller->discardEmojiClipboardSnapshot();
        reportFailure(QStringLiteral("focus-failed"));
        return;
    }
    const bool terminal = shortcut == QStringLiteral("Ctrl+Shift+V");
    const auto control = XKeysymToKeycode(m_display, XK_Control_L);
    const auto shift = XKeysymToKeycode(m_display, XK_Shift_L);
    const auto v = XKeysymToKeycode(m_display, XK_v);
    if (!control || !v || (terminal && !shift)) {
        m_controller->discardEmojiClipboardSnapshot();
        reportFailure(QStringLiteral("input-failed"));
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
        m_controller->discardEmojiClipboardSnapshot();
        reportFailure(QStringLiteral("input-failed"));
        return;
    }
    XFlush(m_display);
    m_controller->reportPasteResult(QStringLiteral("sent"), shortcut);
    emit pasteSent(shortcut);
    if (m_window && m_window->isVisible()) {
        QTimer::singleShot(0, this, [this] {
            if (m_window && m_window->isVisible())
                m_window->raise();
        });
    }
    if (!emoji) {
        m_controller->discardEmojiClipboardSnapshot();
        return;
    }
    XSync(m_display, False);
    processEvents();
    if (m_controller->emojiClipboardSnapshotGeneration() != snapshotGeneration)
        return;
    const Window owner = XGetSelectionOwner(m_display, m_clipboardSelection);
    const quint64 revision = m_clipboardEvents;
    QTimer::singleShot(750, this, [this, snapshotGeneration, owner, revision] {
        restoreClipboard(snapshotGeneration, owner, revision);
    });
}

void X11Backend::restoreClipboard(quint64 snapshotGeneration, unsigned long temporaryOwner,
                                  quint64 clipboardRevision)
{
    if (!m_controller || m_controller->emojiClipboardSnapshotGeneration()
        != snapshotGeneration)
        return;
    XSync(m_display, False);
    processEvents();
    QString temporaryText;
    QString mimeType;
    QByteArray content;
    const bool snapshotAvailable = m_controller->emojiClipboardSnapshot(
        &temporaryText, &mimeType, &content);
    if (!snapshotAvailable) {
        m_controller->discardEmojiClipboardSnapshot();
        m_controller->reportClipboardRestoreResult(
            AppController::ClipboardRestoreResult::Unavailable);
        return;
    }
    if (!temporaryOwner || m_clipboardEvents != clipboardRevision
        || XGetSelectionOwner(m_display, m_clipboardSelection) != temporaryOwner
        || QGuiApplication::clipboard()->text() != temporaryText) {
        m_controller->discardEmojiClipboardSnapshot();
        m_controller->reportClipboardRestoreResult(
            AppController::ClipboardRestoreResult::Changed);
        return;
    }
    m_controller->expectClipboardRestore(mimeType, content);
    if (mimeType == QStringLiteral("image/png")) {
        const QImage image = QImage::fromData(content, "PNG");
        if (image.isNull()) {
            m_controller->discardEmojiClipboardSnapshot();
            m_controller->reportClipboardRestoreResult(
                AppController::ClipboardRestoreResult::Unavailable);
            return;
        }
        QGuiApplication::clipboard()->setImage(image);
    } else if (mimeType == QStringLiteral("text/plain;charset=utf-8")) {
        QGuiApplication::clipboard()->setText(QString::fromUtf8(content));
    } else {
        m_controller->discardEmojiClipboardSnapshot();
        m_controller->reportClipboardRestoreResult(
            AppController::ClipboardRestoreResult::Unavailable);
        return;
    }
    m_controller->discardEmojiClipboardSnapshot();
    m_controller->reportClipboardRestoreResult(
        AppController::ClipboardRestoreResult::Restored);
}

bool X11Backend::grabShortcut(unsigned int keycode, unsigned int modifierMask)
{
    std::array<unsigned int, 4> modifiers = {modifierMask, modifierMask | LockMask,
                                             modifierMask | m_numLockMask,
                                             modifierMask | LockMask | m_numLockMask};
    std::sort(modifiers.begin(), modifiers.end());
    const auto end = std::unique(modifiers.begin(), modifiers.end());
    for (auto it = modifiers.begin(); it != end; ++it) {
        if (!checkedXRequest(m_display, [this, keycode, mask = *it] {
                XGrabKey(m_display, int(keycode), mask, m_root, False,
                         GrabModeAsync, GrabModeAsync);
            })) {
            releaseShortcut(keycode, modifierMask);
            return false;
        }
    }
    return true;
}

void X11Backend::releaseShortcut(unsigned int keycode, unsigned int modifierMask)
{
    std::array<unsigned int, 4> modifiers = {modifierMask, modifierMask | LockMask,
                                             modifierMask | m_numLockMask,
                                             modifierMask | LockMask | m_numLockMask};
    std::sort(modifiers.begin(), modifiers.end());
    const auto end = std::unique(modifiers.begin(), modifiers.end());
    for (auto it = modifiers.begin(); it != end; ++it)
        XUngrabKey(m_display, int(keycode), *it, m_root);
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

bool X11Backend::targetHasFocus(unsigned long target) const
{
    Window focused = None;
    int revert = 0;
    XGetInputFocus(m_display, &focused, &revert);
    while (focused != None && focused != PointerRoot && focused != m_root) {
        if (focused == target)
            return true;
        Window root = None;
        Window parent = None;
        Window *children = nullptr;
        unsigned int count = 0;
        int found = 0;
        const bool noError = checkedXRequest(m_display, [&] {
            found = XQueryTree(m_display, focused, &root, &parent, &children, &count);
        });
        if (children)
            XFree(children);
        if (!noError || !found || parent == focused)
            break;
        focused = parent;
    }
    return false;
}

bool X11Backend::hasEwmhWindowManager() const
{
    const Window manager = singleProperty(m_display, m_root,
                                          "_NET_SUPPORTING_WM_CHECK", XA_WINDOW);
    return manager != None && windowExists(manager)
        && singleProperty(m_display, manager, "_NET_SUPPORTING_WM_CHECK", XA_WINDOW)
            == manager;
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
