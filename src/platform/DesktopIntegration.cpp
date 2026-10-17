#include "DesktopIntegration.h"

#include "app/AppController.h"

#include <QGuiApplication>
#include <QQuickWindow>

#include <utility>

DesktopIntegration::DesktopIntegration(QQuickWindow *window, AppController *controller,
                                       QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_controller(controller)
{}

void DesktopIntegration::reportFailure(const QString &reason)
{
    if (m_controller)
        m_controller->reportPasteResult(reason, QString());
}

void DesktopIntegration::showWindow(const QString &view)
{
    if (!m_window)
        return;
    m_window->setProperty("currentView", view);
    m_window->show();
    if (QGuiApplication::platformName() == QStringLiteral("xcb"))
        m_window->raise();
    m_window->requestActivate();
}

CopyOnlyBackend::CopyOnlyBackend(QQuickWindow *window, AppController *controller,
                                 QString reason, QObject *parent)
    : DesktopIntegration(window, controller, parent)
    , m_reason(std::move(reason))
{
    connect(m_controller, &AppController::itemCopied, this, [this] {
        if (m_controller)
            m_controller->discardEmojiClipboardSnapshot();
    });
}

bool CopyOnlyBackend::isAvailable() const
{
    return false;
}

QString CopyOnlyBackend::errorString() const
{
    return m_reason;
}

void CopyOnlyBackend::captureClipboard(const QString &kind)
{
    if (m_controller && (kind == QStringLiteral("text") || kind == QStringLiteral("image")))
        m_controller->captureNow();
}

void CopyOnlyBackend::openView(const QString &view)
{
    showWindow(view);
}

bool CopyOnlyBackend::rememberTarget()
{
    reportFailure(QStringLiteral("bridge-unavailable"));
    return false;
}

void CopyOnlyBackend::pasteSelection(bool, quint64)
{
    reportFailure(QStringLiteral("bridge-unavailable"));
}

void CopyOnlyBackend::cancel()
{}
