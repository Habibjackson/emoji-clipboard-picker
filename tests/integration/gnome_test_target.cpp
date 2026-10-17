#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QRasterWindow>

#include <cstdio>

class TargetWindow final : public QRasterWindow
{
public:
    TargetWindow(QString resultPath, QString title)
        : m_resultPath(std::move(resultPath))
        , m_xwayland(title.contains(QStringLiteral("XWayland")))
    {
        setTitle(std::move(title));
        resize(320, 180);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(QRect(QPoint(0, 0), size()), Qt::white);
        painter.setPen(Qt::black);
        painter.drawText(QRect(QPoint(0, 0), size()), Qt::AlignCenter,
                         QStringLiteral("GNOME paste test target"));
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        auto *clipboard = QGuiApplication::clipboard();
        if (event->key() == Qt::Key_C && event->modifiers() == Qt::NoModifier) {
            clipboard->setText(m_xwayland ? QStringLiteral("GNOME test XWayland text")
                                         : QStringLiteral("GNOME test prior text"));
            return;
        }
        if (event->key() == Qt::Key_I && event->modifiers() == Qt::NoModifier) {
            QImage image(2, 2, QImage::Format_ARGB32);
            image.fill(m_xwayland ? Qt::blue : Qt::red);
            clipboard->setImage(image);
            return;
        }
        const bool paste = event->key() == Qt::Key_V
                           && event->modifiers() == Qt::ControlModifier;
        const bool read = event->key() == Qt::Key_R
                          && event->modifiers() == Qt::NoModifier;
        if (!paste && !read)
            return;

        QFile result(read ? m_resultPath + QStringLiteral(".clipboard") : m_resultPath);
        if (!result.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return;
        if (clipboard->mimeData()->hasImage()) {
            const QSize size = clipboard->image().size();
            result.write(QStringLiteral("IMAGE %1x%2")
                             .arg(size.width()).arg(size.height()).toUtf8());
        } else {
            result.write(clipboard->text().toUtf8());
        }
        result.close();
        std::puts(paste ? "PASTED" : "READ");
        std::fflush(stdout);
    }

private:
    QString m_resultPath;
    bool m_xwayland;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (app.arguments().size() != 3)
        return 2;
    TargetWindow window(app.arguments().at(1), app.arguments().at(2));
    window.show();
    std::puts("READY");
    std::fflush(stdout);
    return app.exec();
}
