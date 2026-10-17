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
    m_backend->cancel();
    m_backend->openView(QStringLiteral("emoji"));
}

void PickerService::ShowClipboard()
{
    if (!m_backend)
        return;
    m_backend->cancel();
    m_backend->openView(QStringLiteral("clipboard"));
}

uint PickerService::ShowFromShortcut(const QString &view)
{
    if (!m_backend || (view != QStringLiteral("emoji") && view != QStringLiteral("clipboard")))
        return 0;
    const bool targetKnown = m_backend->rememberTarget();
    m_backend->openView(view);
    return targetKnown ? uint(QCoreApplication::applicationPid()) : 0;
}

void PickerService::NotifyClipboardChanged(const QString &kind)
{
    if (m_backend)
        m_backend->captureClipboard(kind);
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
