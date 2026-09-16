#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QInputMethodEvent>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "file_io.h"
#include "theme.h"

static bool call(QObject *object, const char *method) { return QMetaObject::invokeMethod(object, method); }
static bool call(QObject *object, const char *method, QVariant a) { return QMetaObject::invokeMethod(object, method, Q_ARG(QVariant, a)); }
static bool call(QObject *object, const char *method, QVariant a, QVariant b) { return QMetaObject::invokeMethod(object, method, Q_ARG(QVariant, a), Q_ARG(QVariant, b)); }
static QPoint center(QQuickItem *item) { return item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint(); }

class UiTest : public QObject {
    Q_OBJECT
    void load(QQmlApplicationEngine &engine, ThemeFiles &theme, FileIo &io) {
        engine.addImportPath("qrc:/tests/support/vendor");
        engine.rootContext()->setContextProperty("themeFiles", &theme);
        engine.setInitialProperties({{"io", QVariant::fromValue(&io)}});
        engine.load(QUrl::fromLocalFile(QString(SOURCE_DIR) + "/ui/EditorBase.qml"));
    }
private slots:
    void pluginLifecycle() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first();
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(window);
        QVERIFY(!plugin->property("opened").toBool());
        QVERIFY(call(plugin, "showEditor", "{}")); QTRY_VERIFY(plugin->property("opened").toBool());
        auto edit = window->findChild<QQuickItem *>("editor"); QVERIFY(edit); QTRY_VERIFY(edit->hasActiveFocus());
        call(plugin, "showEditor", "{bad json"); QCOMPARE(plugin->findChildren<QQuickWindow *>().size(), 1);
        auto doc = window->property("document").value<QObject *>(); QVERIFY(doc);
        doc->setProperty("text", "draft"); call(doc, "requestNew");
        auto dialog = window->findChild<QObject *>("unsavedDialog");
        QTRY_VERIFY(dialog->property("opened").toBool()); QVERIFY(!edit->hasActiveFocus());
        call(plugin, "showEditor", "{}"); QTest::qWait(30); QVERIFY(!edit->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Escape); QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(doc->property("text").toString(), "draft");
        doc->setProperty("text", ""); call(plugin, "hideEditor"); QTRY_VERIFY(!plugin->property("opened").toBool());
    }
    void hoverActions() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first(); call(plugin, "showEditor", "{}");
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(QTest::qWaitForWindowExposed(window));
        auto gear = window->findChild<QQuickItem *>("settingsButton");
        auto fresh = window->findChild<QQuickItem *>("newButton");
        auto open = window->findChild<QQuickItem *>("openButton");
        auto save = window->findChild<QQuickItem *>("saveButton");
        QVERIFY(gear && fresh && open && save);
        QTest::mouseMove(window, QPoint(400, 200));
        QVERIFY(gear->isVisible()); QVERIFY(!fresh->isVisible() && !open->isVisible() && !save->isVisible());
        QTest::mouseMove(window, center(gear));
        QTRY_VERIFY(fresh->isVisible() && open->isVisible() && save->isVisible());
        QTRY_VERIFY(save->x() > open->x()); QTest::mouseMove(window, center(save)); QVERIFY(fresh->isVisible());
        for (auto button : {gear, fresh, open, save}) {
            QVERIFY(button->property("text").toString().isEmpty());
            QVERIFY(!button->property("iconText").toString().isEmpty());
        }
        QSignalSpy picker(window->property("document").value<QObject *>(), SIGNAL(fileDialogRequested(bool)));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(save));
        QCOMPARE(picker.count(), 1); QCOMPARE(picker.at(0).at(0).toBool(), true);
        QTest::keyClick(window, Qt::Key_Escape);
    }
    void fontSettings() {
        ThemeFiles theme(QDir::homePath()); FileIo io;
        {
            QQmlApplicationEngine engine; load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
            auto plugin = engine.rootObjects().first(); call(plugin, "showEditor", "{}");
            auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(QTest::qWaitForWindowExposed(window));
            auto edit = window->findChild<QQuickItem *>("editor");
            auto gear = window->findChild<QQuickItem *>("settingsButton");
            auto settings = window->findChild<QObject *>("settingsDialog");
            auto fonts = window->findChild<QObject *>("fontPicker");
            auto sizes = window->findChild<QObject *>("sizePicker");
            QVERIFY(edit && gear && settings && fonts && sizes);
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(gear));
            QTRY_VERIFY(settings->property("opened").toBool()); QVERIFY(settings->property("modal").toBool()); QVERIFY(!edit->hasActiveFocus());
            for (const auto &name : {"cancelSettings", "applySettings"}) {
                auto button = window->findChild<QQuickItem *>(name); QVERIFY(button);
                QVERIFY(button->property("text").toString().isEmpty());
                QVERIFY(!button->property("iconText").toString().isEmpty());
                QVERIFY(!button->property("tooltipText").toString().isEmpty());
            }
            const int original = edit->property("font").value<QFont>().pixelSize();
            QMetaObject::invokeMethod(fonts, "changed", Q_ARG(QString, "Liberation Mono"));
            QMetaObject::invokeMethod(sizes, "changed", Q_ARG(QString, "28"));
            QTRY_COMPARE(edit->property("font").value<QFont>().pixelSize(), 28);
            QCOMPARE(edit->property("font").value<QFont>().family(), "Liberation Mono");
            QTest::keyClick(window, Qt::Key_Escape); QTRY_VERIFY(!settings->property("visible").toBool());
            QCOMPARE(edit->property("font").value<QFont>().pixelSize(), original);
            QTest::keyClick(window, Qt::Key_Comma, Qt::ControlModifier); QTRY_VERIFY(settings->property("opened").toBool());
            QSignalSpy picker(window->property("document").value<QObject *>(), SIGNAL(fileDialogRequested(bool)));
            for (auto key : {Qt::Key_N, Qt::Key_O, Qt::Key_S}) QTest::keyClick(window, key, Qt::ControlModifier);
            QCOMPARE(picker.count(), 0); QVERIFY(settings->property("opened").toBool());
            QMetaObject::invokeMethod(fonts, "changed", Q_ARG(QString, "Liberation Mono"));
            QMetaObject::invokeMethod(sizes, "changed", Q_ARG(QString, "28")); call(settings, "apply");
            QTRY_VERIFY(!settings->property("visible").toBool()); QVERIFY(edit->hasActiveFocus());
        }
        QQmlApplicationEngine engine; load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto window = engine.rootObjects().first()->findChild<QQuickWindow *>(); QVERIFY(window);
        QCOMPARE(window->property("preferredFontFamily").toString(), "Liberation Mono");
        QCOMPARE(window->property("preferredFontSize").toInt(), 28);
    }
    void unsavedTransitions() {
        FileIo io; QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString(SOURCE_DIR) + "/ui/Document.qml"));
        std::unique_ptr<QObject> doc(component.createWithInitialProperties({{"io", QVariant::fromValue(&io)}}));
        QVERIFY2(doc, qPrintable(component.errorString()));
        QSignalSpy confirmation(doc.get(), SIGNAL(confirmationRequested()));
        QSignalSpy closed(doc.get(), SIGNAL(closeReady()));
        QSignalSpy picker(doc.get(), SIGNAL(fileDialogRequested(bool)));
        doc->setProperty("text", "draft"); call(doc.get(), "requestNew"); QCOMPARE(confirmation.count(), 1);
        call(doc.get(), "resolveUnsaved", "cancel"); QCOMPARE(doc->property("text").toString(), "draft");
        call(doc.get(), "requestNew"); call(doc.get(), "resolveUnsaved", "discard");
        QCOMPARE(doc->property("text").toString(), ""); QVERIFY(!doc->property("modified").toBool());
        doc->setProperty("text", "draft"); call(doc.get(), "requestClose"); call(doc.get(), "resolveUnsaved", "save");
        QCOMPARE(picker.count(), 1); QCOMPARE(closed.count(), 0); call(doc.get(), "cancelFile");
        QTemporaryDir dir; const auto first = QUrl::fromLocalFile(dir.filePath("first.txt"));
        call(doc.get(), "acceptFile", first, true); QTRY_VERIFY(!io.busy()); QCOMPARE(closed.count(), 0);
        doc->setProperty("text", "final"); call(doc.get(), "requestClose"); call(doc.get(), "resolveUnsaved", "save");
        QVERIFY(io.busy()); call(doc.get(), "requestNew"); // A second action cannot replace an in-flight save/close.
        QTRY_VERIFY(!io.busy()); QCOMPARE(closed.count(), 1); QCOMPARE(doc->property("text").toString(), "final");
        const auto other = QUrl::fromLocalFile(dir.filePath("other.txt"));
        call(doc.get(), "saveAs"); QCOMPARE(picker.count(), 2); call(doc.get(), "acceptFile", other, true);
        QTRY_VERIFY(!io.busy()); QCOMPARE(doc->property("url").toUrl(), other); QVERIFY(QFile::exists(first.toLocalFile()));
        call(doc.get(), "open", QUrl::fromLocalFile("/nonexistent-omatext.txt")); QTRY_VERIFY(!io.busy());
        QCOMPARE(doc->property("text").toString(), "final"); QCOMPARE(doc->property("url").toUrl(), other);
        QVERIFY(!doc->property("error").toString().isEmpty());
    }
    void documentStateAndInput() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first(); call(plugin, "showEditor", "{}");
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(QTest::qWaitForWindowExposed(window));
        auto edit = window->findChild<QQuickItem *>("editor"); edit->forceActiveFocus();
        auto doc = window->property("document").value<QObject *>();
        QInputMethodEvent composing(QString::fromUtf8("\u306b\u307b\u3093\u3054"), {});
        QCoreApplication::sendEvent(edit, &composing); QCOMPARE(doc->property("text").toString(), "");
        const QString text = QString::fromUtf8("\u65e5\u672c\u8a9e");
        QInputMethodEvent commit; commit.setCommitString(text); QCoreApplication::sendEvent(edit, &commit);
        QCOMPARE(doc->property("text").toString(), text);
        QTemporaryDir dir; QUrl url = QUrl::fromLocalFile(dir.filePath("\u65e5\u672c\u8a9e.txt"));
        call(doc, "acceptFile", url, true); QTRY_VERIFY(!io.busy()); QVERIFY(!doc->property("modified").toBool());
        call(doc, "requestNew"); QCOMPARE(edit->property("text").toString(), "");
        call(doc, "open", url); QTRY_VERIFY(!io.busy()); QCOMPARE(doc->property("text").toString(), text);
        doc->setProperty("text", "changed"); call(doc, "requestNew");
        auto dialog = window->findChild<QObject *>("unsavedDialog"); QTRY_VERIFY(dialog->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_Escape); QCOMPARE(doc->property("text").toString(), "changed");
        call(doc, "requestNew"); QTRY_VERIFY(dialog->property("opened").toBool()); call(dialog, "choose", "save");
        QTRY_VERIFY(!io.busy()); QCOMPARE(doc->property("text").toString(), "");
        QFile file(url.toLocalFile()); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("changed"));
        doc->setProperty("text", "keep"); call(doc, "requestClose"); QTRY_VERIFY(dialog->property("opened").toBool());
        call(dialog, "choose", "save"); call(doc, "acceptFile", QUrl::fromLocalFile("/nonexistent-omatext/child.txt"), true);
        QTRY_VERIFY(!io.busy()); QVERIFY(plugin->property("opened").toBool()); QCOMPARE(doc->property("text").toString(), "keep");
        QVERIFY(!doc->property("error").toString().isEmpty());
    }
};
int main(int argc, char **argv) {
    QTemporaryDir config; qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QGuiApplication app(argc, argv); QQuickStyle::setStyle("Basic");
    UiTest test; return QTest::qExec(&test, argc, argv);
}
#include "ui_test.moc"
