#pragma once
#include <QObject>
#include <QProcess>
#include <QJsonDocument>
#include <QVariantMap>

// Test-only transport. Production uses Quickshell.Io.Process with the same helper.
class FileIo : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    explicit FileIo(QObject *parent = nullptr) : QObject(parent) {
        connect(&process, &QProcess::finished, this, [this](int code) {
            const auto result = QJsonDocument::fromJson(process.readAllStandardOutput()).toVariant().toMap();
            active = false; emit busyChanged();
            emit completed(code == 0 && !result.isEmpty() ? result : QVariantMap{{"ok", false}, {"error", "File helper failed."}});
        });
    }
    bool busy() const { return active; }
    Q_INVOKABLE void request(const QVariantMap &message) {
        if (active) return;
        active = true; emit busyChanged();
        process.start("python3", {QString(SOURCE_DIR) + "/scripts/file_io.py"});
        process.write(QJsonDocument::fromVariant(message).toJson(QJsonDocument::Compact) + '\n');
        process.closeWriteChannel();
    }
signals:
    void busyChanged();
    void completed(QVariant result);
private:
    QProcess process;
    bool active = false;
};
