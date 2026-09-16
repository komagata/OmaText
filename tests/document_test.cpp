#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "document.h"

class DocumentTest : public QObject {
    Q_OBJECT
private slots:
    void japaneseRoundTrip() {
        QTemporaryDir dir;
        const QUrl path = QUrl::fromLocalFile(dir.filePath("日本語.txt"));
        Document doc;
        doc.setText(QString::fromUtf8("こんにちは、日本語。\n第二行🙂\n"));
        QVERIFY(doc.modified());
        doc.acceptFile(path, true);
        QVERIFY2(!doc.modified(), qPrintable(doc.error()));
        QCOMPARE(doc.url(), path);
        Document reopened;
        QVERIFY(reopened.open(path));
        QCOMPARE(reopened.text(), doc.text());
    }
    void cancelAndDiscard() {
        Document doc;
        QSignalSpy confirm(&doc, &Document::confirmationRequested);
        doc.setText("draft"); doc.requestNew();
        QCOMPARE(confirm.count(), 1);
        doc.resolveUnsaved("cancel");
        QCOMPARE(doc.text(), "draft");
        doc.requestNew(); doc.resolveUnsaved("discard");
        QVERIFY(doc.text().isEmpty()); QVERIFY(!doc.modified());
    }
    void saveBeforeCloseAndCancelPicker() {
        QTemporaryDir dir;
        Document doc;
        QSignalSpy closed(&doc, &Document::closeReady);
        QSignalSpy picker(&doc, &Document::fileDialogRequested);
        doc.setText("draft"); doc.requestClose(); doc.resolveUnsaved("save");
        QCOMPARE(picker.count(), 1); QCOMPARE(closed.count(), 0);
        doc.cancelFile();
        doc.acceptFile(QUrl::fromLocalFile(dir.filePath("other.txt")), true);
        QCOMPARE(closed.count(), 0);
        doc.setText("final"); doc.requestClose(); doc.resolveUnsaved("save");
        QCOMPARE(closed.count(), 1);
    }
    void failedSaveNeverCloses() {
        Document doc;
        QSignalSpy closed(&doc, &Document::closeReady);
        doc.setText("precious"); doc.requestClose(); doc.resolveUnsaved("save");
        doc.acceptFile(QUrl::fromLocalFile("/nonexistent-omatext/child.txt"), true);
        QCOMPARE(closed.count(), 0); QVERIFY(doc.modified());
        QCOMPARE(doc.text(), "precious"); QVERIFY(!doc.error().isEmpty());
        QVERIFY(doc.url().isEmpty());
    }
    void failedOpenPreservesBuffer() {
        Document doc; doc.setText("keep");
        QVERIFY(!doc.open(QUrl::fromLocalFile("/nonexistent-omatext.txt")));
        QCOMPARE(doc.text(), "keep");
    }
    void invalidUtf8RejectedAndCrlfPreserved() {
        QTemporaryDir dir;
        QString path = dir.filePath("text.txt");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("\xff\xfe\x00\x01", 4); file.close();
        Document doc; QVERIFY(!doc.open(QUrl::fromLocalFile(path)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("\xef\xbb\xbfhello\r\n"); file.close();
        QVERIFY(doc.open(QUrl::fromLocalFile(path)));
        QCOMPARE(doc.text(), "hello\n"); doc.setText("hello\nworld\n"); doc.save();
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("\xef\xbb\xbfhello\r\nworld\r\n"));
    }
    void oversizedAndSpecialFilesRejected() {
        QTemporaryDir dir;
        const QString path = dir.filePath("large.txt");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArray(2 * 1024 * 1024 + 1, 'a')), qint64(2 * 1024 * 1024 + 1)); file.close();
        Document doc;
        QVERIFY(!doc.open(QUrl::fromLocalFile(path)));
        QVERIFY(!doc.error().isEmpty());
        QVERIFY(!doc.open(QUrl::fromLocalFile("/dev/zero")));
        QVERIFY(!doc.error().isEmpty());
    }
    void undoToSavedIsCleanAndSaveAsChangesTarget() {
        QTemporaryDir dir; Document doc;
        doc.setText("first"); doc.acceptFile(QUrl::fromLocalFile(dir.filePath("a.txt")), true);
        doc.setText("changed"); QVERIFY(doc.modified());
        doc.setText("first"); QVERIFY(!doc.modified());
        const QUrl next = QUrl::fromLocalFile(dir.filePath("b.txt"));
        doc.acceptFile(next, true); QCOMPARE(doc.url(), next);
        QVERIFY(QFile::exists(dir.filePath("a.txt")));
    }
};
QTEST_GUILESS_MAIN(DocumentTest)
#include "document_test.moc"
