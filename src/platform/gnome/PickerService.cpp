#include "PickerService.h"

#include "app/AppController.h"
#include "platform/DesktopIntegration.h"

#include <QCoreApplication>
#include <QQuickWindow>

PickerService::PickerService(QQuickWindow *window, AppController *controller,
                             DesktopIntegration *backend, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_controller(controller)
    , m_backend(backend)
{}

void PickerService::ShowEmoji()
{
    if (!m_backend)
        return;
    if (IsVisible() && CurrentView() == QStringLiteral("emoji")) {
        Hide();
        return;
    }
    m_backend->cancel();
    m_backend->openView(QStringLiteral("emoji"));
}

void PickerService::ShowClipboard()
{
    if (!m_backend)
        return;
    if (IsVisible() && CurrentView() == QStringLiteral("clipboard")) {
        Hide();
        return;
    }
    m_backend->cancel();
    m_backend->openView(QStringLiteral("clipboard"));
}

uint PickerService::ShowFromShortcut(const QString &view, bool pickerFocused)
{
    if (!m_backend || (view != QStringLiteral("emoji") && view != QStringLiteral("clipboard")))
        return 0;
    if (IsVisible() && pickerFocused) {
        if (CurrentView() == view)
            Hide();
        else
            m_backend->openView(view);
        return IsVisible() ? uint(QCoreApplication::applicationPid()) : 0;
    }
    const bool targetKnown = m_backend->rememberTarget();
    m_backend->openView(view);
    return targetKnown ? uint(QCoreApplication::applicationPid()) : 0;
}

void PickerService::NotifyClipboardChanged(const QString &kind)
{
    if (m_backend)
        m_backend->captureClipboard(kind);
}

void PickerService::NotifyClipboardPayload(const QString &kind, const QString &mimeType,
                                           const QByteArray &content)
{
    if (m_backend)
        m_backend->captureClipboardPayload(kind, mimeType, content);
}

void PickerService::Hide()
{
    if (m_backend)
        m_backend->cancel();
    if (m_window)
        m_window->hide();
}

void PickerService::Quit()
{
    QCoreApplication::exit(0);
}

QString PickerService::CurrentView() const
{
    return m_window ? m_window->property("currentView").toString() : QString();
}

QString PickerService::CurrentStatus() const
{
    return m_controller ? m_controller->status() : QString();
}

bool PickerService::IsVisible() const
{
    return m_window && m_window->isVisible();
}
