#pragma once
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class Document : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QUrl url READ url NOTIFY stateChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    using QObject::QObject;
    QString text() const { return m_text; }
    QUrl url() const { return m_url; }
    bool modified() const { return m_text != m_saved; }
    QString error() const { return m_error; }
    void setText(const QString &text);
    Q_INVOKABLE void requestNew();
    Q_INVOKABLE void requestOpen();
    Q_INVOKABLE void requestClose();
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs();
    Q_INVOKABLE void acceptFile(const QUrl &url, bool saving);
    Q_INVOKABLE void cancelFile();
    Q_INVOKABLE void resolveUnsaved(const QString &choice);
    Q_INVOKABLE void clearError();
    bool open(const QUrl &url);
signals:
    void textChanged();
    void stateChanged();
    void errorChanged();
    void confirmationRequested();
    void fileDialogRequested(bool saving);
    void closeReady();
private:
    enum Action { None, New, Open, Close };
    void request(Action action);
    void proceed();
    bool write(const QUrl &url);
    bool fail(const QString &message);
    QString m_text, m_saved, m_error;
    QUrl m_url;
    Action m_pending = None;
    bool m_crlf = false;
    bool m_bom = false;
};
