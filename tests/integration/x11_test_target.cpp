#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QRasterWindow>
#include <QTimer>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstdio>

class TargetWindow final : public QRasterWindow
{
public:
    TargetWindow(QString resultPath, bool terminal)
        : m_resultPath(std::move(resultPath)), m_terminal(terminal) {}

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() != Qt::Key_V
            || !(event->modifiers() & Qt::ControlModifier)
            || bool(event->modifiers() & Qt::ShiftModifier) != m_terminal) {
            return;
        }
        QFile result(m_resultPath);
        if (!result.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return;
        auto *clipboard = QGuiApplication::clipboard();
        if (clipboard->mimeData()->hasImage()) {
            const QSize size = clipboard->image().size();
            result.write(QStringLiteral("IMAGE %1x%2").arg(size.width()).arg(size.height()).toUtf8());
        } else {
            result.write(clipboard->text().toUtf8());
        }
        result.close();
        std::puts("PASTED");
        std::fflush(stdout);
    }

private:
    QString m_resultPath;
    bool m_terminal;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (app.arguments().size() < 2)
        return 2;
    const bool terminal = app.arguments().contains(QStringLiteral("--terminal"));
    TargetWindow window(app.arguments().at(1), terminal);
    window.setTitle(QStringLiteral("X11 integration test target"));
    window.resize(240, 100);
    window.show();

    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return 3;
    if (terminal) {
        XClassHint hint;
        hint.res_name = const_cast<char *>("org.gnome.Ptyxis");
        hint.res_class = const_cast<char *>("org.gnome.Ptyxis");
        XSetClassHint(display, Window(window.winId()), &hint);
    }
    XSetInputFocus(display, Window(window.winId()), RevertToPointerRoot, CurrentTime);
    XFlush(display);
    std::printf("READY %lu\n", static_cast<unsigned long>(window.winId()));
    std::fflush(stdout);

    QTimer::singleShot(200, &app, [] {
        QGuiApplication::clipboard()->setText(QStringLiteral("X11 source text"));
    });
    QTimer::singleShot(650, &app, [] {
        QImage image(2, 2, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QGuiApplication::clipboard()->setImage(image);
    });

    const int result = app.exec();
    XCloseDisplay(display);
    return result;
}
