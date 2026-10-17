#include "PickerService.h"

#include "app/AppController.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQuickWindow>

PickerService::PickerService(QQuickWindow *window, AppController *controller, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_controller(controller)
{
}

void PickerService::ShowEmoji()
{
    showView(QStringLiteral("emoji"));
}

void PickerService::ShowClipboard()
{
    showView(QStringLiteral("clipboard"));
}

void PickerService::NotifyClipboardChanged(const QString &kind)
{
    if (m_controller && (kind == QStringLiteral("text") || kind == QStringLiteral("image")))
        m_controller->captureNow();
}

void PickerService::Hide()
{
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

bool PickerService::IsVisible() const
{
    return m_window && m_window->isVisible();
}

void PickerService::showView(const QString &view)
{
    if (!m_window)
        return;
    m_window->setProperty("currentView", view);
    m_window->show();
    if (QGuiApplication::platformName() == QStringLiteral("xcb"))
        m_window->raise();
    m_window->requestActivate();
}
