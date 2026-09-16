#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QInputMethodEvent>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "document.h"
#include "theme.h"

class UiTest : public QObject {
    Q_OBJECT
private slots:
    void pluginLifecycle() {
        ThemeFiles theme(QDir::homePath());
        QQmlApplicationEngine engine;
        engine.addImportPath("qrc:/tests/support/vendor");
        engine.rootContext()->setContextProperty("themeFiles", &theme);
        engine.load(QUrl::fromLocalFile(QString(SOURCE_DIR) + "/ui/Editor.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *plugin = engine.rootObjects().first();
        auto window = plugin->findChild<QQuickWindow *>();
        QVERIFY(window);
        QVERIFY(window->property("document").value<QObject *>());
        QVERIFY(!plugin->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(plugin, "open", Q_ARG(QVariant, "{}")));
        QTRY_VERIFY(plugin->property("opened").toBool());
        auto edit = window->findChild<QQuickItem *>("editor");
        QVERIFY(edit); QTRY_VERIFY(edit->hasActiveFocus());
        QMetaObject::invokeMethod(plugin, "open", Q_ARG(QVariant, "{bad json"));
        QCOMPARE(plugin->findChildren<QQuickWindow *>().size(), 1);
        QObject *document = window->property("document").value<QObject *>();
        document->setProperty("text", "draft");
        QMetaObject::invokeMethod(document, "requestNew");
        auto dialog = window->findChild<QObject *>("unsavedDialog");
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(!edit->hasActiveFocus());
        QMetaObject::invokeMethod(plugin, "open", Q_ARG(QVariant, "{}"));
        QTest::qWait(30);
        QVERIFY(!edit->hasActiveFocus()); // Summoning must not move focus behind the modal.
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(document->property("text").toString(), "draft");
        document->setProperty("text", "");
        QMetaObject::invokeMethod(plugin, "close");
        QTRY_VERIFY(!plugin->property("opened").toBool());
    }
    void hoverActions() {
        Document doc; ThemeFiles theme(QDir::homePath());
        QQmlApplicationEngine engine;
        engine.addImportPath("qrc:/tests/support/vendor");
        engine.setInitialProperties({{"document", QVariant::fromValue(&doc)}});
        engine.rootContext()->setContextProperty("themeFiles", &theme);
        engine.load(QUrl("qrc:/ui/EditorView.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window); window->show(); QVERIFY(QTest::qWaitForWindowExposed(window));
        auto gear = window->findChild<QQuickItem *>("settingsButton");
        auto fresh = window->findChild<QQuickItem *>("newButton");
        auto open = window->findChild<QQuickItem *>("openButton");
        auto save = window->findChild<QQuickItem *>("saveButton");
        QVERIFY(gear); QVERIFY(fresh); QVERIFY(open); QVERIFY(save);
        auto center = [](QQuickItem *item) { return item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint(); };
        QTest::mouseMove(window, QPoint(400, 200));
        QVERIFY(gear->isVisible()); QVERIFY(!fresh->isVisible());
        QVERIFY(!open->isVisible()); QVERIFY(!save->isVisible());
        QTest::mouseMove(window, center(gear));
        QTRY_VERIFY(fresh->isVisible() && open->isVisible() && save->isVisible());
        QTRY_VERIFY(save->x() > open->x());
        QTest::mouseMove(window, center(save));
        QVERIFY(fresh->isVisible()); // Crossing the row must not collapse it.
        for (auto button : {gear, fresh, open, save}) {
            QVERIFY(button->property("text").toString().isEmpty());
            QVERIFY(!button->property("iconText").toString().isEmpty());
        }
        QSignalSpy picker(&doc, &Document::fileDialogRequested);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(save));
        QCOMPARE(picker.count(), 1); QCOMPARE(picker.at(0).at(0).toBool(), true);
        QTest::keyClick(window, Qt::Key_Escape);
    }
    void fontSettings() {
        ThemeFiles theme(QDir::homePath());
        auto loadPlugin = [&](QQmlApplicationEngine &engine) {
            engine.addImportPath("qrc:/tests/support/vendor");
            engine.rootContext()->setContextProperty("themeFiles", &theme);
            engine.load(QUrl::fromLocalFile(QString(SOURCE_DIR) + "/ui/Editor.qml"));
        };
        {
            QQmlApplicationEngine engine; loadPlugin(engine);
            QVERIFY(!engine.rootObjects().isEmpty());
            auto plugin = engine.rootObjects().first();
            auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(window);
            QMetaObject::invokeMethod(plugin, "open", Q_ARG(QVariant, "{}"));
            QVERIFY(QTest::qWaitForWindowExposed(window));
            auto edit = window->findChild<QQuickItem *>("editor");
            auto gear = window->findChild<QQuickItem *>("settingsButton");
            auto settings = window->findChild<QObject *>("settingsDialog");
            auto fonts = window->findChild<QObject *>("fontPicker");
            auto sizes = window->findChild<QObject *>("sizePicker");
            QVERIFY(edit && gear && settings && fonts && sizes);
            auto center = gear->mapToScene(QPointF(gear->width()/2, gear->height()/2)).toPoint();
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_VERIFY(settings->property("opened").toBool());
            QVERIFY(settings->property("modal").toBool());
            QVERIFY(!edit->hasActiveFocus());
            for (const auto &name : {"cancelSettings", "applySettings"}) {
                auto button = window->findChild<QQuickItem *>(name);
                QVERIFY(button);
                QVERIFY(button->property("text").toString().isEmpty());
                QVERIFY(!button->property("iconText").toString().isEmpty());
                QVERIFY(!button->property("tooltipText").toString().isEmpty());
            }
            const int originalSize = edit->property("font").value<QFont>().pixelSize();
            QMetaObject::invokeMethod(fonts, "changed", Q_ARG(QString, "DejaVu Sans"));
            QMetaObject::invokeMethod(sizes, "changed", Q_ARG(QString, "28"));
            QTRY_COMPARE(edit->property("font").value<QFont>().pixelSize(), 28);
            QCOMPARE(edit->property("font").value<QFont>().family(), "DejaVu Sans");
            QTest::keyClick(window, Qt::Key_Escape);
            QTRY_VERIFY(!settings->property("visible").toBool());
            QCOMPARE(edit->property("font").value<QFont>().pixelSize(), originalSize);
            QTest::keyClick(window, Qt::Key_Comma, Qt::ControlModifier);
            QTRY_VERIFY(settings->property("opened").toBool());
            auto document = window->property("document").value<QObject *>();
            QSignalSpy picker(document, SIGNAL(fileDialogRequested(bool)));
            for (auto key : {Qt::Key_N, Qt::Key_O, Qt::Key_S})
                QTest::keyClick(window, key, Qt::ControlModifier);
            QCOMPARE(picker.count(), 0);
            QVERIFY(settings->property("opened").toBool());
            QMetaObject::invokeMethod(fonts, "changed", Q_ARG(QString, "DejaVu Sans"));
            QMetaObject::invokeMethod(sizes, "changed", Q_ARG(QString, "28"));
            QMetaObject::invokeMethod(settings, "apply");
            QTRY_VERIFY(!settings->property("visible").toBool());
            QCOMPARE(window->property("preferredFontSize").toInt(), 28);
            QVERIFY(edit->hasActiveFocus());
        }
        // Recreate the actual plugin to verify its own settings file, not just UI state.
        QQmlApplicationEngine reopened; loadPlugin(reopened);
        QVERIFY(!reopened.rootObjects().isEmpty());
        auto window = reopened.rootObjects().first()->findChild<QQuickWindow *>();
        QVERIFY(window);
        QCOMPARE(window->property("preferredFontFamily").toString(), "DejaVu Sans");
        QCOMPARE(window->property("preferredFontSize").toInt(), 28);
    }
    void themeReload() {
        QTemporaryDir home;
        QString folder = home.filePath(".local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(folder));
        QFile colors(folder + "/colors.toml"); QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("background = \"#123456\"\nforeground = \"#eeeeee\"\n"); colors.close();
        ThemeFiles theme(home.path());
        QVERIFY(theme.colors().contains("#123456"));
        QSignalSpy changed(&theme, &ThemeFiles::changed);
        QVERIFY(colors.open(QIODevice::WriteOnly)); colors.write("background = \"#654321\"\n"); colors.close();
        theme.reload(); QVERIFY(theme.colors().contains("#654321")); QCOMPARE(changed.count(), 1);
    }
    void editorAndUnsavedDialog() {
        Document doc; ThemeFiles theme(QDir::homePath());
        QQmlApplicationEngine engine;
        engine.addImportPath("qrc:/tests/support/vendor");
        engine.setInitialProperties({{"document", QVariant::fromValue(&doc)}});
        engine.rootContext()->setContextProperty("themeFiles", &theme);
        engine.load(QUrl("qrc:/ui/EditorView.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window); window->show(); QVERIFY(QTest::qWaitForWindowExposed(window));
        auto editor = window->findChild<QQuickItem *>("editor");
        QVERIFY(editor);
        editor->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_H, Qt::ShiftModifier);
        for (auto key : {Qt::Key_E, Qt::Key_L, Qt::Key_L, Qt::Key_O}) QTest::keyClick(window, key);
        QCOMPARE(doc.text(), "hello");
        QInputMethodEvent composing(QString::fromUtf8("にほんご"), {});
        QCoreApplication::sendEvent(editor, &composing);
        QCOMPARE(doc.text(), "hello");
        QInputMethodEvent commit; commit.setCommitString(QString::fromUtf8("日本語"));
        QCoreApplication::sendEvent(editor, &commit);
        QCOMPARE(doc.text(), QString::fromUtf8("hello日本語"));
        QTest::keyClick(window, Qt::Key_N, Qt::ControlModifier);
        auto dialog = window->findChild<QObject *>("unsavedDialog");
        QVERIFY(dialog); QTRY_VERIFY(dialog->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(doc.text(), QString::fromUtf8("hello日本語"));
        QTemporaryDir dir;
        const QUrl url = QUrl::fromLocalFile(dir.filePath("日本語.txt"));
        doc.acceptFile(url, true);
        doc.requestNew(); QCOMPARE(editor->property("text").toString(), "");
        QVERIFY(doc.open(url)); QCOMPARE(editor->property("text").toString(), QString::fromUtf8("hello日本語"));
    }
};
int main(int argc, char **argv) {
    QTemporaryDir config;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    UiTest test; return QTest::qExec(&test, argc, argv);
}
#include "ui_test.moc"
