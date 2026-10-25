#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Notes are stored under QStandardPaths::AppDataLocation, e.g. ~/.local/share/QtNotes
    app.setApplicationName(QStringLiteral("QtNotes"));
    // Lets Linux desktops match the window to its .desktop file, for its name
    // and icon, e.g. in the task bar.
    app.setDesktopFileName(QStringLiteral("io.github.mknln.qtnotes"));

    // Works around a bug in Qt's GTK platform theme: looking up the fixed-width
    // font, which the Markdown importer does when notes are loaded, makes later
    // lookups of the title bar font return monospace, so the window title is
    // drawn in a monospace font. Looking the title bar font up first caches the
    // right one.
    QFontDatabase::systemFont(QFontDatabase::TitleFont);

    QQmlApplicationEngine engine;
    QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
            [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule(u"Notes", u"Main");

    return app.exec();
}
