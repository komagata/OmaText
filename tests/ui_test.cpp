#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlProperty>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QInputMethodEvent>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QSettings>
#include "file_io.h"
#include "theme.h"

static bool call(QObject *object, const char *method) { return QMetaObject::invokeMethod(object, method); }
static bool call(QObject *object, const char *method, QVariant a) { return QMetaObject::invokeMethod(object, method, Q_ARG(QVariant, a)); }
static bool call(QObject *object, const char *method, QVariant a, QVariant b) { return QMetaObject::invokeMethod(object, method, Q_ARG(QVariant, a), Q_ARG(QVariant, b)); }
static QPoint center(QQuickItem *item) { return item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint(); }

static QQuickItem *visualChild(QQuickItem *parent, const QString &name) {
    for (auto child : parent->childItems()) {
        if (child->objectName() == name) return child;
        if (auto found = visualChild(child, name)) return found;
    }
    return nullptr;
}

class UiTest : public QObject {
    Q_OBJECT
    void load(QQmlApplicationEngine &engine, ThemeFiles &theme, FileIo &io) {
        engine.addImportPath("qrc:/tests/support/vendor");
        engine.rootContext()->setContextProperty("themeFiles", &theme);
        auto treeIo = new FileIo(&engine);
        engine.setInitialProperties({{"io", QVariant::fromValue(&io)}, {"treeIo", QVariant::fromValue(treeIo)}});
        engine.load(QUrl::fromLocalFile(QString(SOURCE_DIR) + "/ui/EditorBase.qml"));
    }
private slots:
    void sidebarVisibilityRestored() {
        // main() supplies a private XDG_CONFIG_HOME: remove only this test's preferences.
        const auto settingsPath = qEnvironmentVariable("XDG_CONFIG_HOME") + "/omatext/editor.ini";
        QFile::remove(settingsPath);
        ThemeFiles theme(QDir::homePath());
        for (int launch = 0; launch < 3; ++launch) {
            FileIo io; QQmlApplicationEngine engine; load(engine, theme, io);
            QVERIFY(!engine.rootObjects().isEmpty());
            auto plugin = engine.rootObjects().first();
            auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(window);
            QCOMPARE(window->property("sidebarVisible").toBool(), launch == 1);
            if (launch < 2) {
                call(plugin, "showEditor", "{}");
                QVERIFY(QTest::qWaitForWindowExposed(window));
                QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
                QTRY_COMPARE(window->property("sidebarVisible").toBool(), launch == 0);
            }
            delete plugin;
        }
    }
    void minimalEditorAndSettingsTreeActions() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first(); call(plugin, "showEditor", "{}");
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(QTest::qWaitForWindowExposed(window));
        auto edit = window->findChild<QQuickItem *>("editor");
        auto gear = window->findChild<QQuickItem *>("settingsButton");
        auto settings = window->findChild<QObject *>("settingsDialog");
        auto doc = window->property("document").value<QObject *>();
        QVERIFY(edit && gear && settings && doc);
        window->setProperty("sidebarVisible", false);
        QVERIFY(window->findChild<QQuickItem *>("sidebarToggle"));
        QVERIFY(!window->findChild<QObject *>("pathHeader"));
        QCOMPARE(edit->mapToScene(QPointF(0, 0)).y() + edit->property("topPadding").toReal(), 24.0);
        doc->setProperty("url", QUrl::fromLocalFile("/tmp/private-note.txt"));
        QCOMPARE(window->title(), QString("OmaText"));
        doc->setProperty("text", "keep this draft");
        QCOMPARE(window->title(), QString::fromUtf8("● OmaText"));
        const auto originalFamily = window->property("preferredFontFamily").toString();
        const auto originalSize = window->property("preferredFontSize").toInt();
        const auto originalFont = edit->property("font").value<QFont>();
        QSignalSpy prefs(window, SIGNAL(preferencesChanged(QString,int)));
        for (bool visible : {true, false}) {
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(gear));
            QTRY_VERIFY(settings->property("opened").toBool());
            auto toggle = window->findChild<QQuickItem *>("settingsTreeButton"); QVERIFY(toggle);
            QCOMPARE(toggle->property("text").toString(), visible ? QString("Show file tree") : QString("Hide file tree"));
            QVERIFY(toggle->property("tooltipText").toString().contains("Ctrl+B"));
            QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
            QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier | Qt::ShiftModifier);
            QCOMPARE(window->property("sidebarVisible").toBool(), !visible);
            QVERIFY(!window->findChild<QObject *>("folderDialog")->property("visible").toBool());
            QMetaObject::invokeMethod(window->findChild<QObject *>("sizePicker"), "changed", Q_ARG(QString, "28"));
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(toggle));
            QTRY_VERIFY(!settings->property("visible").toBool());
            QTRY_COMPARE(window->property("sidebarVisible").toBool(), visible);
            QCOMPARE(window->property("preferredFontFamily").toString(), originalFamily);
            QCOMPARE(window->property("preferredFontSize").toInt(), originalSize);
            QCOMPARE(edit->property("font").value<QFont>(), originalFont);
            QCOMPARE(doc->property("text").toString(), "keep this draft");
        }
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(gear));
        QTRY_VERIFY(settings->property("opened").toBool());
        auto folderButton = window->findChild<QQuickItem *>("settingsFolderButton"); QVERIFY(folderButton);
        QCOMPARE(folderButton->property("text").toString(), QString("Open folder"));
        QVERIFY(folderButton->property("tooltipText").toString().contains("Ctrl+Shift+O"));
        QMetaObject::invokeMethod(window->findChild<QObject *>("fontPicker"), "changed", Q_ARG(QString, "Liberation Mono"));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(folderButton));
        auto folderDialog = window->findChild<QObject *>("folderDialog");
        QTRY_VERIFY(folderDialog->property("visible").toBool());
        QVERIFY(!settings->property("visible").toBool());
        QVERIFY(!window->property("sidebarVisible").toBool());
        QCOMPARE(edit->property("font").value<QFont>(), originalFont);
        QTemporaryDir dir;
        folderDialog->setProperty("selectedFolder", QUrl::fromLocalFile(dir.path()));
        call(folderDialog, "accept");
        QTRY_VERIFY(window->property("sidebarVisible").toBool());
        auto tree = window->findChild<QObject *>("fileTree");
        QTRY_COMPARE(QUrl(tree->property("rootUrl").toString()), QUrl::fromLocalFile(dir.path()));
        QTRY_VERIFY(!tree->property("loading").toBool());
        QCOMPARE(doc->property("text").toString(), "keep this draft");
        QCOMPARE(prefs.count(), 0);
        window->resize(520, 360);
        window->requestActivate(); QTRY_VERIFY(window->isActive());
        QTRY_VERIFY(center(gear).x() < window->width() && center(gear).y() < window->height());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(gear));
        QTRY_VERIFY(settings->property("opened").toBool());
        auto toggle = window->findChild<QQuickItem *>("settingsTreeButton");
        QVERIFY(center(toggle).y() > 0 && center(toggle).y() < window->height());
        QVERIFY(center(folderButton).y() > 0 && center(folderButton).y() < window->height());
        call(settings, "close");
        window->setProperty("sidebarVisible", false);
    }
    void sidebarHoverToggle_data() {
        QTest::addColumn<QSize>("windowSize");
        QTest::newRow("default") << QSize(960, 720);
        QTest::newRow("minimum") << QSize(520, 360);
    }
    void sidebarHoverToggle() {
        QFETCH(QSize, windowSize);
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first();
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(window);
        window->resize(windowSize);
        call(plugin, "showEditor", "{}");
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto toggle = window->findChild<QQuickItem *>("sidebarToggle"); QVERIFY(toggle);
        auto gear = window->findChild<QQuickItem *>("settingsButton"); QVERIFY(gear);
        auto edit = window->findChild<QQuickItem *>("editor");
        auto doc = window->property("document").value<QObject *>();
        window->setProperty("sidebarVisible", false); call(window, "focusEditor");
        doc->setProperty("text", QString::fromUtf8("unsaved 日本語"));
        QTest::mouseMove(window, QPoint(window->width()/2, window->height()/2));
        QVERIFY(!toggle->isVisible());
        QCOMPARE(toggle->size(), QSizeF(36, 36));
        QTRY_COMPARE(center(gear), QPoint(window->width() - 30, window->height() - 30));
        const auto gearPosition = center(gear);
        QTest::mouseMove(window, QPoint(12, window->height() - 30));
        QTest::qWait(100);
        QVERIFY(!toggle->isVisible());
        QVERIFY(!window->findChild<QQuickItem *>("newButton")->isVisible());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(12, window->height() - 30));
        QVERIFY(!window->property("sidebarVisible").toBool());
        for (bool visible : {true, false}) {
            QTest::mouseMove(window, gearPosition); QTRY_VERIFY(toggle->isVisible());
            auto fresh = window->findChild<QQuickItem *>("newButton");
            auto open = window->findChild<QQuickItem *>("openButton");
            auto save = window->findChild<QQuickItem *>("saveButton");
            QVERIFY(fresh->isVisible() && open->isVisible() && save->isVisible());
            QCOMPARE(center(gear), gearPosition);
            QCOMPARE(toggle->property("tooltipText").toString(), visible ? QString("Show file tree (Ctrl+B)") : QString("Hide file tree (Ctrl+B)"));
            QTRY_VERIFY(center(fresh).x() < center(open).x() && center(open).x() < center(save).x()
                && center(save).x() < center(toggle).x() && center(toggle).x() < center(gear).x());
            QCOMPARE(fresh->mapToScene(QPointF(0, 0)).x(), qreal(window->width() - 208));
            // Traverse button edges and gaps, rather than teleporting between centers.
            for (int x = gearPosition.x(); x >= center(fresh).x(); x -= 4) {
                QTest::mouseMove(window, QPoint(x, gearPosition.y()));
                QVERIFY(toggle->isVisible() && fresh->isVisible() && open->isVisible() && save->isVisible());
            }
            QTest::mouseMove(window, center(toggle));
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(toggle));
            QTRY_COMPARE(window->property("sidebarVisible").toBool(), visible);
            QSettings saved(qEnvironmentVariable("XDG_CONFIG_HOME") + "/omatext/editor.ini", QSettings::IniFormat);
            QTRY_VERIFY((saved.sync(), saved.value("sidebarVisible").toBool() == visible));
            QTest::mouseMove(window, QPoint(window->width()/2, window->height()/2));
            QTRY_VERIFY(!toggle->isVisible() && !fresh->isVisible() && !open->isVisible() && !save->isVisible());
            QCOMPARE(center(gear), gearPosition);
        }
        QVERIFY(edit->hasActiveFocus());
        QTest::mouseMove(window, gearPosition); QTRY_VERIFY(toggle->isVisible());
        toggle->forceActiveFocus();
        QTest::mouseMove(window, QPoint(window->width()/2, window->height()/2));
        QVERIFY(toggle->isVisible());
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(window->property("sidebarVisible").toBool());
        window->setProperty("sidebarVisible", false); call(window, "focusEditor");
        auto settings = window->findChild<QObject *>("settingsDialog"); call(settings, "open");
        QTRY_VERIFY(settings->property("visible").toBool()); QVERIFY(!toggle->isEnabled());
        QTest::mouseMove(window, gearPosition);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(toggle));
        QVERIFY(!window->property("sidebarVisible").toBool());
        call(settings, "close");
        window->resize(520, 360);
        QTRY_COMPARE(window->size(), QSize(520, 360));
        // QQuickWindow resize delivery updates anchored items asynchronously.
        QTRY_COMPARE(center(gear), QPoint(490, 330));
        QTest::mouseMove(window, center(gear)); QTRY_VERIFY(toggle->isVisible());
        QTRY_COMPARE(center(toggle), QPoint(450, 330));
        QCOMPARE(doc->property("text").toString(), QString::fromUtf8("unsaved 日本語"));
    }
    void folderPayloadRevealsTree() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first(); call(plugin, "showEditor", "{}");
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(QTest::qWaitForWindowExposed(window));
        auto doc = window->property("document").value<QObject *>();
        auto tree = window->findChild<QObject *>("fileTree");
        window->setProperty("sidebarVisible", false);
        doc->setProperty("text", QString::fromUtf8("unsaved 日本語"));
        QTemporaryDir dir;
        const auto folderUrl = QUrl::fromLocalFile(dir.path());
        const auto payload = QString::fromUtf8(QJsonDocument(QJsonObject{{"folderUrl", folderUrl.toString()}}).toJson(QJsonDocument::Compact));
        call(plugin, "showEditor", payload);
        QTRY_VERIFY(window->property("sidebarVisible").toBool());
        QTRY_COMPARE(QUrl(tree->property("rootUrl").toString()), folderUrl);
        QTRY_VERIFY(!tree->property("loading").toBool());
        QCOMPARE(doc->property("text").toString(), QString::fromUtf8("unsaved 日本語"));
        QVERIFY(doc->property("modified").toBool());
        QCOMPARE(doc->property("url").toUrl(), QUrl());
        QVERIFY(!window->findChild<QObject *>("unsavedDialog")->property("visible").toBool());
        window->setProperty("sidebarVisible", false);
        call(plugin, "showEditor", payload); QTRY_VERIFY(window->property("sidebarVisible").toBool());
        QTRY_VERIFY(!tree->property("loading").toBool());
        window->setProperty("sidebarVisible", false);
        auto settings = window->findChild<QObject *>("settingsDialog"); call(settings, "open");
        QTRY_VERIFY(settings->property("visible").toBool());
        call(plugin, "showEditor", payload); QVERIFY(!window->property("sidebarVisible").toBool());
        call(settings, "close");
        call(window, "openFolder");
        auto folderDialog = window->findChild<QObject *>("folderDialog");
        QTRY_VERIFY(folderDialog->property("visible").toBool());
        call(folderDialog, "reject"); QTRY_VERIFY(!folderDialog->property("visible").toBool());
        QVERIFY(!window->property("sidebarVisible").toBool());
        QCOMPARE(doc->property("text").toString(), QString::fromUtf8("unsaved 日本語"));
        const auto savedUrl = QUrl::fromLocalFile(dir.filePath("saved.txt"));
        call(doc, "acceptFile", savedUrl, true); QVERIFY(io.busy());
        call(plugin, "showEditor", payload); QVERIFY(!window->property("sidebarVisible").toBool());
        QVERIFY(call(window, "openFolderUrl", folderUrl)); QVERIFY(!window->property("sidebarVisible").toBool());
        QTRY_VERIFY(!io.busy());
        QCOMPARE(doc->property("text").toString(), QString::fromUtf8("unsaved 日本語"));
        QCOMPARE(doc->property("url").toUrl(), savedUrl);
    }
    void sidebarKeyboardReachability() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first(); call(plugin, "showEditor", "{}");
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(QTest::qWaitForWindowExposed(window));
        auto edit = window->findChild<QQuickItem *>("editor");
        auto list = window->findChild<QQuickItem *>("treeList");
        auto doc = window->property("document").value<QObject *>();
        auto tree = window->findChild<QObject *>("fileTree");
        doc->setProperty("text", QString::fromUtf8("draft 日本語"));
        window->setProperty("sidebarVisible", false); call(window, "focusEditor");
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QTRY_VERIFY(list->hasActiveFocus());
        QTemporaryDir dir; QVERIFY(QDir(dir.path()).mkdir("nested"));
        QFile file(dir.filePath("nested/note.txt")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("text"); file.close();
        const auto rootUrl = QUrl::fromLocalFile(dir.path()).toString();
        const auto nestedUrl = QUrl::fromLocalFile(dir.filePath("nested")).toString();
        call(tree, "setRoot", rootUrl); QTRY_COMPARE(tree->property("rows").toList().size(), 1);
        QTest::keyClick(window, Qt::Key_Down); QTest::keyClick(window, Qt::Key_Right);
        QTRY_COMPARE(tree->property("rows").toList().size(), 2);
        QTest::keyClick(window, Qt::Key_Down); QCOMPARE(list->property("currentIndex").toInt(), 1);
        QQuickItem *row = nullptr; QTRY_VERIFY((row = visualChild(list, "fileTreeRow1")));
        QCOMPARE(row->property("color").value<QColor>().alpha(), 0);
        QCOMPARE(QQmlProperty(row, "border.width").read().toInt(), 1);
        QVERIFY(row->findChild<QObject *>("treeEntryIcon"));
        // Collapse while the cursor is on a child, then replace the root with an empty folder.
        call(tree, "toggle", nestedUrl); QTRY_COMPARE(list->property("currentIndex").toInt(), 0);
        QTemporaryDir empty; call(tree, "setRoot", QUrl::fromLocalFile(empty.path()).toString());
        QTRY_COMPARE(list->property("currentIndex").toInt(), -1);
        QTRY_VERIFY(!tree->property("loading").toBool());
        for (auto key : {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down}) QTest::keyClick(window, key);
        QCOMPARE(list->property("currentIndex").toInt(), -1);
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier); QTRY_VERIFY(edit->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier); QTRY_VERIFY(list->hasActiveFocus());
        // The intentionally visible sidebar folder button still leads into the tree.
        window->findChild<QQuickItem *>("openFolderButton")->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Tab); QTRY_VERIFY(list->hasActiveFocus());
        QCOMPARE(doc->property("text").toString(), QString::fromUtf8("draft 日本語"));
        QVERIFY(doc->property("modified").toBool());
    }
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
    void openFileFromLauncher() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first();
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(window);
        auto doc = window->property("document").value<QObject *>(); QVERIFY(doc);
        QTemporaryDir dir;
        const auto url = QUrl::fromLocalFile(dir.filePath("日本語 space #.md"));
        QFile file(url.toLocalFile()); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# Launcher document\n"); file.close();
        const QString payload = QString::fromUtf8(QJsonDocument(QJsonObject{{"fileUrl", url.toString(QUrl::FullyEncoded)}}).toJson(QJsonDocument::Compact));
        doc->setProperty("text", "keep this draft");
        call(plugin, "showEditor", payload);
        auto dialog = window->findChild<QObject *>("unsavedDialog");
        QTRY_VERIFY(dialog->property("opened").toBool());
        QCOMPARE(doc->property("text").toString(), "keep this draft");
        call(dialog, "choose", "cancel");
        QCOMPARE(doc->property("text").toString(), "keep this draft");
        call(plugin, "showEditor", payload);
        QTRY_VERIFY(dialog->property("opened").toBool());
        call(dialog, "choose", "discard");
        QTRY_COMPARE(doc->property("text").toString(), "# Launcher document\n");
        QCOMPARE(doc->property("url").toUrl(), url);
        doc->setProperty("text", "edited same file");
        call(plugin, "showEditor", payload);
        QCOMPARE(doc->property("text").toString(), "edited same file");
        QVERIFY(!dialog->property("visible").toBool());
        const auto nextUrl = QUrl::fromLocalFile(dir.filePath("next.txt"));
        QFile next(nextUrl.toLocalFile()); QVERIFY(next.open(QIODevice::WriteOnly));
        next.write("next file"); next.close();
        const QString nextPayload = QString::fromUtf8(QJsonDocument(QJsonObject{{"fileUrl", nextUrl.toString()}}).toJson(QJsonDocument::Compact));
        call(plugin, "showEditor", nextPayload);
        QTRY_VERIFY(dialog->property("opened").toBool());
        call(dialog, "choose", "save");
        QTRY_COMPARE(doc->property("text").toString(), "next file");
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("edited same file"));
    }
    void sidebarDragResize() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first(); call(plugin, "showEditor", "{}");
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(QTest::qWaitForWindowExposed(window));
        auto sidebar = window->findChild<QQuickItem *>("sidebar");
        auto handle = window->findChild<QQuickItem *>("sidebarResizeHandle");
        auto edit = window->findChild<QQuickItem *>("editor");
        auto doc = window->property("document").value<QObject *>();
        QVERIFY(sidebar && handle && edit && doc);
        window->setProperty("sidebarVisible", true);
        window->resize(960, 720);
        QTRY_COMPARE(sidebar->width(), 240.0);
        QTRY_COMPARE(edit->mapToScene(QPointF()).x(), sidebar->width());
        QTRY_COMPARE(handle->mapToScene(QPointF(handle->width()/2, 0)).x(), sidebar->width() - 0.5);
        QCOMPARE(handle->width(), 8.0);
        QCOMPARE(handle->property("cursorShape").toInt(), int(Qt::SizeHorCursor));
        doc->setProperty("text", "keep this draft and selection");
        edit->forceActiveFocus(); QVERIFY(QMetaObject::invokeMethod(edit, "select", Q_ARG(int, 5), Q_ARG(int, 15)));
        const int cursor = edit->property("cursorPosition").toInt();
        const QString selection = edit->property("selectedText").toString();
        auto drag = [window, handle](int targetX) {
            const auto start = center(handle);
            QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
            // Several moves catch drift caused by a handle that follows the pointer.
            QTest::mouseMove(window, QPoint((start.x() + targetX)/2, start.y()));
            QTest::mouseMove(window, QPoint(targetX, start.y()));
            QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(targetX, start.y()));
        };
        // Press within the generous invisible hit area, away from the one-pixel line.
        auto start = center(handle) + QPoint(3, 0);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, start + QPoint(100, 0));
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start + QPoint(100, 0));
        QTRY_COMPARE(sidebar->width(), 340.0);
        QTest::mouseMove(window, start + QPoint(200, 0));
        QCOMPARE(sidebar->width(), 340.0); // Release ends resizing.
        QTRY_COMPARE(edit->mapToScene(QPointF()).x(), 340.0);
        QCOMPARE(doc->property("text").toString(), "keep this draft and selection");
        QCOMPARE(edit->property("cursorPosition").toInt(), cursor);
        QCOMPARE(edit->property("selectedText").toString(), selection);
        QVERIFY(edit->hasActiveFocus());
        drag(-100); QTRY_COMPARE(sidebar->width(), 160.0);
        QTRY_COMPARE(edit->mapToScene(QPointF()).x(), 160.0);
        drag(1100); QTRY_COMPARE(sidebar->width(), 680.0);
        QTRY_COMPARE(edit->width(), 280.0);
        drag(400); QTRY_COMPARE(sidebar->width(), 400.0);
        window->resize(520, 360);
        QTRY_COMPARE(sidebar->height(), 360.0);
        QTRY_COMPARE(sidebar->width(), 240.0);
        QTRY_COMPARE(edit->width(), 280.0);
        QTRY_COMPARE(handle->height(), 360.0);
        window->resize(960, 720);
        QTRY_COMPARE(sidebar->width(), 400.0);
        QTRY_COMPARE(edit->width(), 560.0);
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QTRY_VERIFY(!handle->isVisible()); QVERIFY(!handle->property("enabled").toBool());
        QTRY_COMPARE(edit->mapToScene(QPointF()).x(), 0.0);
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QTRY_VERIFY(handle->isVisible());
        QTRY_COMPARE(sidebar->width(), 400.0);
        QTRY_COMPARE(edit->mapToScene(QPointF()).x(), 400.0);
        QTest::keyClick(window, Qt::Key_Comma, Qt::ControlModifier);
        auto settings = window->findChild<QObject *>("settingsDialog");
        QTRY_VERIFY(settings->property("opened").toBool()); QVERIFY(!handle->property("enabled").toBool());
        drag(160); QCOMPARE(sidebar->width(), 400.0);
        call(settings, "close"); QTRY_VERIFY(handle->property("enabled").toBool());
        QTemporaryDir saved;
        call(doc, "acceptFile", QUrl::fromLocalFile(saved.filePath("draft.txt")), true);
        QVERIFY(doc->property("busy").toBool()); QVERIFY(!handle->property("enabled").toBool());
        // Press while the real helper is in flight, without pumping its completion first.
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center(handle));
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center(handle));
        QCOMPARE(sidebar->width(), 400.0);
        QTRY_VERIFY(!doc->property("busy").toBool()); QTRY_VERIFY(handle->property("enabled").toBool());
        window->resize(520, 360);
        QTRY_COMPARE(sidebar->height(), 360.0);
        QTRY_COMPARE(sidebar->width(), 240.0);
        QTRY_COMPARE(edit->width(), 280.0);
        drag(-100); QTRY_COMPARE(sidebar->width(), 160.0);
        QTRY_COMPARE(edit->width(), 360.0);
        drag(800); QTRY_COMPARE(sidebar->width(), 240.0);
        QTRY_COMPARE(edit->width(), 280.0);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center(handle));
        QVERIFY(handle->property("pressed").toBool());
        window->setProperty("sidebarVisible", false);
        QTRY_VERIFY(!handle->property("pressed").toBool());
        QTRY_COMPARE(edit->mapToScene(QPointF()).x(), 0.0);
        QTest::mouseMove(window, QPoint(20, 180));
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(20, 180));
        window->setProperty("sidebarVisible", true);
        QTRY_COMPARE(edit->mapToScene(QPointF()).x(), 240.0);
        QTRY_COMPARE(sidebar->width(), 240.0); // Hiding cancels the active drag.
        drag(200); QTRY_COMPARE(sidebar->width(), 200.0);
        QCOMPARE(doc->property("text").toString(), "keep this draft and selection");
    }
    void sidebarSafety() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto plugin = engine.rootObjects().first(); call(plugin, "showEditor", "{}");
        auto window = plugin->findChild<QQuickWindow *>(); QVERIFY(QTest::qWaitForWindowExposed(window));
        auto tree = window->findChild<QObject *>("fileTree"); QVERIFY(tree);
        QVERIFY(window->findChild<QQuickItem *>("sidebarToggle"));
        auto doc = window->property("document").value<QObject *>();
        auto edit = window->findChild<QQuickItem *>("editor");
        window->setProperty("sidebarVisible", true);
        doc->setProperty("text", "draft");
        auto folderDialog = window->findChild<QObject *>("folderDialog"); QVERIFY(folderDialog);
        QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_VERIFY(folderDialog->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier); QVERIFY(window->property("sidebarVisible").toBool());
        auto dialogWindow = QGuiApplication::focusWindow(); QVERIFY(dialogWindow);
        QTest::keyClick(dialogWindow, Qt::Key_Escape); QTRY_VERIFY(!folderDialog->property("visible").toBool());
        // Offscreen leaves focusWindow on the hidden popup; emulate compositor return.
        window->requestActivate(); QTRY_VERIFY(window->isActive());
        QCOMPARE(doc->property("text").toString(), "draft");
        auto list = window->findChild<QQuickItem *>("treeList"); QVERIFY(list); list->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QTRY_VERIFY(!window->property("sidebarVisible").toBool()); QTRY_VERIFY(edit->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QTRY_VERIFY(window->property("sidebarVisible").toBool()); QCOMPARE(doc->property("text").toString(), "draft");
        QTemporaryDir dir; QDir(dir.path()).mkdir("nested");
        QFile file(dir.filePath("nested/日本語 # \".md")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("nested text"); file.close();
        const auto url = QUrl::fromLocalFile(file.fileName());
        call(tree, "setRoot", QUrl::fromLocalFile(dir.path()).toString());
        QTRY_VERIFY(!tree->property("loading").toBool());
        QVERIFY(!tree->property("rows").toList().isEmpty());
        list->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Down); QTest::keyClick(window, Qt::Key_Right);
        QTRY_VERIFY(!tree->property("loading").toBool());
        QCOMPARE(tree->property("rows").toList().size(), 2);
        QTest::keyClick(window, Qt::Key_Down); QTest::keyClick(window, Qt::Key_Return);
        auto dialog = window->findChild<QObject *>("unsavedDialog"); QTRY_VERIFY(dialog->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier); QVERIFY(window->property("sidebarVisible").toBool());
        QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier | Qt::ShiftModifier); QVERIFY(!folderDialog->property("visible").toBool());
        call(dialog, "choose", "cancel"); QCOMPARE(doc->property("text").toString(), "draft"); QCOMPARE(tree->property("selectedUrl").toString(), "");
        call(window, "openTreeFile", url.toString()); call(dialog, "choose", "discard");
        QTRY_COMPARE(doc->property("text").toString(), "nested text"); QCOMPARE(QUrl(tree->property("selectedUrl").toString()), url);
        QCOMPARE(tree->property("rows").toList().at(1).toMap().value("url").toString(), tree->property("selectedUrl").toString());
        list->forceActiveFocus(); QTest::keyClick(window, Qt::Key_Left);
        QCOMPARE(list->property("currentIndex").toInt(), 0);
        QTest::keyClick(window, Qt::Key_Left); QCOMPARE(tree->property("rows").toList().size(), 1);
        QTest::keyClick(window, Qt::Key_Right); QCOMPARE(tree->property("rows").toList().size(), 2);
        QCOMPARE(doc->property("text").toString(), "nested text");
        file.remove(); call(window, "openTreeFile", QUrl::fromLocalFile(dir.filePath("missing.txt")).toString());
        QTRY_VERIFY(!io.busy()); QCOMPARE(doc->property("text").toString(), "nested text"); QCOMPARE(QUrl(tree->property("selectedUrl").toString()), url);
        window->resize(520, 360); QTRY_VERIFY(edit->width() >= 280);
        call(window->findChild<QObject *>("errorDialog"), "close");
        window->setProperty("sidebarVisible", false);
        delete plugin; // Recreate the plugin, flushing Qt Settings as on shell shutdown.
        FileIo nextIo; QQmlApplicationEngine nextEngine; load(nextEngine, theme, nextIo);
        QVERIFY(!nextEngine.rootObjects().isEmpty());
        QVERIFY(!nextEngine.rootObjects().first()->findChild<QQuickWindow *>()->property("sidebarVisible").toBool());
    }
    void transportTimeoutKeepsRequestSerialized() {
        QQmlEngine engine;
        engine.addImportPath(QString(SOURCE_DIR) + "/tests/support/transport");
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString(SOURCE_DIR) + "/ui/FileIo.qml"));
        std::unique_ptr<QObject> io(component.create()); QVERIFY2(io, qPrintable(component.errorString()));
        auto process = io->property("process").value<QObject *>(); QVERIFY(process);
        auto timeout = io->property("timeout").value<QObject *>(); QVERIFY(timeout);
        QSignalSpy completed(io.get(), SIGNAL(completed(QVariant)));
        call(io.get(), "request", QVariantMap{{"action", "list"}, {"url", "file:///tmp"}});
        QVERIFY(io->property("busy").toBool());
        QVERIFY(call(timeout, "triggered"));
        QVERIFY(io->property("busy").toBool()); QCOMPARE(completed.count(), 0);
        QVERIFY(!process->property("running").toBool());
        QVERIFY(call(process, "exit", 0));
        QVERIFY(!io->property("busy").toBool()); QCOMPARE(completed.count(), 1);
        QVERIFY(!completed.at(0).at(0).toMap().value("ok").toBool());
        call(io.get(), "request", QVariantMap{{"action", "list"}, {"url", "file:///tmp/new"}});
        auto output = process->property("stdout").value<QObject *>(); QVERIFY(output);
        output->setProperty("text", "{\"ok\":true,\"entries\":[]}");
        call(process, "exit", 0); QCOMPARE(completed.count(), 2);
        QVERIFY(completed.at(1).at(0).toMap().value("ok").toBool());
    }
    void treeRootChangesAndRetry() {
        ThemeFiles theme(QDir::homePath()); FileIo io; QQmlApplicationEngine engine;
        load(engine, theme, io); QVERIFY(!engine.rootObjects().isEmpty());
        auto window = engine.rootObjects().first()->findChild<QQuickWindow *>();
        auto tree = window->findChild<QObject *>("fileTree"); QVERIFY(tree);
        QTemporaryDir oldRoot, newRoot;
        QFile oldFile(oldRoot.filePath("old.txt")); QVERIFY(oldFile.open(QIODevice::WriteOnly)); oldFile.close();
        QFile newFile(newRoot.filePath("new.txt")); QVERIFY(newFile.open(QIODevice::WriteOnly)); newFile.close();
        call(tree, "setRoot", QUrl::fromLocalFile(oldRoot.path()).toString());
        QVERIFY(tree->property("loading").toBool());
        call(tree, "setRoot", QUrl::fromLocalFile(newRoot.path()).toString());
        QTRY_VERIFY(!tree->property("loading").toBool());
        auto rows = tree->property("rows").toList(); QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value("name").toString(), "new.txt");
        auto doc = window->property("document").value<QObject *>(); doc->setProperty("text", "keep draft");
        const auto deleted = newRoot.filePath("deleted");
        call(tree, "setRoot", QUrl::fromLocalFile(deleted).toString());
        QTRY_VERIFY(!tree->property("loading").toBool()); QVERIFY(!tree->property("error").toString().isEmpty());
        QCOMPARE(doc->property("text").toString(), "keep draft");
        QVERIFY(QDir().mkdir(deleted)); call(tree, "retry");
        QTRY_VERIFY(!tree->property("loading").toBool()); QVERIFY(tree->property("error").toString().isEmpty());
        QCOMPARE(doc->property("text").toString(), "keep draft");
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
        auto toggle = window->findChild<QQuickItem *>("sidebarToggle");
        QVERIFY(gear && fresh && open && save && toggle);
        QTest::mouseMove(window, QPoint(400, 200));
        QVERIFY(gear->isVisible()); QVERIFY(!fresh->isVisible() && !open->isVisible() && !save->isVisible() && !toggle->isVisible());
        const auto gearPosition = center(gear); QVERIFY(gearPosition.x() > window->width() - 60);
        QTest::mouseMove(window, center(gear));
        QTRY_VERIFY(fresh->isVisible() && open->isVisible() && save->isVisible() && toggle->isVisible());
        QCOMPARE(center(gear), gearPosition); QVERIFY(center(save).x() < center(gear).x());
        QTRY_VERIFY(save->x() > open->x()); QTest::mouseMove(window, center(save)); QVERIFY(fresh->isVisible());
        for (auto button : {gear, fresh, open, save, toggle}) {
            QVERIFY(button->property("text").toString().isEmpty());
            QVERIFY(!button->property("iconText").toString().isEmpty());
        }
        QTest::mouseMove(window, QPoint(400, 200));
        QTRY_VERIFY(!fresh->isVisible() && !open->isVisible() && !save->isVisible() && !toggle->isVisible());
        QCOMPARE(center(gear), gearPosition);
        QTest::mouseMove(window, gearPosition);
        QTRY_VERIFY(save->isVisible());
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
    if (app.arguments().contains("--preview")) {
        QTemporaryDir fixture;
        const auto fixtureRoot = fixture.filePath("my-notes");
        QDir().mkpath(fixtureRoot + "/notes"); QDir().mkpath(fixtureRoot + "/journal");
        for (const auto &name : {"README.md", "todo.txt", "notes/ideas.md", "notes/meeting.txt", "journal/2026-10-01.md"}) {
            QFile file(fixtureRoot + "/" + name); if (!file.open(QIODevice::WriteOnly)) return 1;
            file.write(QByteArray("# Ideas\n\nWrite down what comes to mind.\n\n## Next steps\n\n- Choose a file in the sidebar\n- Read and edit plain text\n- Keep notes in folders\n"));
        }
        ThemeFiles theme(QDir::homePath()); FileIo io, treeIo; QQmlApplicationEngine engine;
        engine.addImportPath("qrc:/tests/support/vendor");
        engine.rootContext()->setContextProperty("themeFiles", &theme);
        engine.setInitialProperties({{"io", QVariant::fromValue(&io)}, {"treeIo", QVariant::fromValue(&treeIo)}});
        engine.load(QUrl::fromLocalFile(QString(SOURCE_DIR) + "/ui/EditorBase.qml"));
        if (engine.rootObjects().isEmpty()) return 1;
        auto plugin = engine.rootObjects().first(); auto window = plugin->findChild<QQuickWindow *>();
        auto tree = window->findChild<QObject *>("fileTree");
        window->setProperty("sidebarVisible", !qEnvironmentVariableIsSet("OMATEXT_PREVIEW_HIDDEN"));
        const auto sidebarWidth = qEnvironmentVariableIntValue("OMATEXT_PREVIEW_SIDEBAR_WIDTH");
        if (sidebarWidth > 0) window->setProperty("requestedSidebarWidth", sidebarWidth);
        const auto width = qEnvironmentVariableIntValue("OMATEXT_PREVIEW_WIDTH");
        const auto height = qEnvironmentVariableIntValue("OMATEXT_PREVIEW_HEIGHT");
        if (width >= 520 || height >= 360)
            window->resize(width >= 520 ? width : 960, height >= 360 ? height : 720);
        call(plugin, "showEditor", "{}");
        call(tree, "setRoot", QUrl::fromLocalFile(fixtureRoot).toString());
        QObject::connect(&treeIo, &FileIo::completed, &app, [&, tree](QVariant) {
            if (tree->property("rows").toList().size() == 4)
                QTimer::singleShot(0, tree, [&, tree] { call(tree, "toggle", QUrl::fromLocalFile(fixtureRoot + "/notes").toString()); });
        });
        call(window, "openTreeFile", QUrl::fromLocalFile(fixtureRoot + "/notes/ideas.md").toString());
        QTimer::singleShot(200, window, [window] {
            call(window, "focusEditor");
            const auto gear = window->findChild<QQuickItem *>("settingsButton");
            if (qEnvironmentVariableIsSet("OMATEXT_PREVIEW_TREE_TOGGLE")) {
                QTest::mouseMove(window, center(gear));
                const auto toggle = window->findChild<QQuickItem *>("sidebarToggle");
                // The Row positioner runs after hover changes visibility.
                if (!QTest::qWaitFor([gear, toggle] {
                    return toggle->isVisible() && center(toggle) == center(gear) - QPoint(40, 0);
                })) { QCoreApplication::exit(1); return; }
                QTest::mouseMove(window, center(toggle));
            } else QTest::mouseMove(window, qEnvironmentVariableIsSet("OMATEXT_PREVIEW_ACTIONS")
                ? center(gear) : QPoint(window->width()/2, 300));
            if (qEnvironmentVariableIsSet("OMATEXT_PREVIEW_SETTINGS"))
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(gear));
        });
        const auto snapshot = qEnvironmentVariable("OMATEXT_PREVIEW_SNAPSHOT");
        if (!snapshot.isEmpty()) QTimer::singleShot(1500, &app, [&] { app.exit(window->grabWindow().save(snapshot) ? 0 : 1); });
        return app.exec();
    }
    UiTest test; return QTest::qExec(&test, argc, argv);
}
#include "ui_test.moc"
