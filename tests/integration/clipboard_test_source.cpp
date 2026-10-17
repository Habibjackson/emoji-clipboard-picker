#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QPainter>
#include <QRasterWindow>
#include <QTimer>

class TestWindow : public QRasterWindow
{
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(QRect(QPoint(0, 0), size()), Qt::white);
        painter.setPen(Qt::black);
        painter.drawText(QRect(QPoint(0, 0), size()), Qt::AlignCenter,
                         QStringLiteral("Clipboard test source"));
    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    TestWindow window;
    window.setTitle(QStringLiteral("Emoji Keyboard clipboard test source"));
    window.resize(240, 100);
    window.show();
    QTimer::singleShot(150, &window, &QWindow::requestActivate);

    QTimer::singleShot(500, &app, [&window] {
        qInfo() << "Qt clipboard source:" << QGuiApplication::platformName()
                << "window active:" << window.isActive();
        QGuiApplication::clipboard()->setText(QStringLiteral("Emoji Keyboard clipboard test"));
        qInfo() << "Qt text formats:" << QGuiApplication::clipboard()->mimeData()->formats();
    });
    QTimer::singleShot(1800, &app, [&window] {
        qInfo() << "Qt image source window active:" << window.isActive();
        QImage image(2, 2, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QGuiApplication::clipboard()->setImage(image);
        qInfo() << "Qt image formats:" << QGuiApplication::clipboard()->mimeData()->formats();
    });
    QTimer::singleShot(2600, &app, &QGuiApplication::quit);
    return app.exec();
}
