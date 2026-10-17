#include "DesktopIntegration.h"

#include "app/AppController.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>

#include <algorithm>
#include <utility>

namespace {
int popupCoordinate(int pointer, int extent, int areaStart, int areaExtent)
{
    constexpr int gap = 12;
    constexpr int margin = 8;
    const int edge = areaExtent >= extent + 2 * margin ? margin : 0;
    const int minimum = areaStart + edge;
    const int maximum = areaStart + std::max(0, areaExtent - extent - edge);
    int position = pointer + gap;
    if (position + extent > areaStart + areaExtent - edge)
        position = pointer - extent - gap;
    return std::clamp(position, minimum, maximum);
}
}

DesktopIntegration::DesktopIntegration(QQuickWindow *window, AppController *controller,
                                       QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_controller(controller)
{}

void DesktopIntegration::captureClipboardPayload(const QString &, const QString &,
                                                  const QByteArray &)
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
    if (!m_window->isVisible() && QGuiApplication::platformName() == QStringLiteral("xcb")) {
        const QPoint pointer = QCursor::pos();
        QScreen *screen = QGuiApplication::screenAt(pointer);
        if (!screen)
            screen = m_window->screen() ? m_window->screen() : QGuiApplication::primaryScreen();
        if (screen) {
            const QRect area = screen->availableGeometry();
            m_window->setPosition(popupCoordinate(pointer.x(), m_window->width(),
                                                  area.x(), area.width()),
                                  popupCoordinate(pointer.y(), m_window->height(),
                                                  area.y(), area.height()));
        }
    }
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
