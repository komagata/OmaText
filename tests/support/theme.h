#pragma once
#include <QObject>
#include <QTimer>
class ThemeFiles : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString colors READ colors NOTIFY changed)
    Q_PROPERTY(QString shell READ shell NOTIFY changed)
    Q_PROPERTY(QString userShell READ userShell NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY changed)
public:
    explicit ThemeFiles(const QString &home, QObject *parent = nullptr);
    QString colors() const { return m_colors; }
    QString shell() const { return m_shell; }
    QString userShell() const { return m_user; }
    QString fontFamily() const { return m_font; }
    void reload();
signals:
    void changed();
private:
    QString m_home, m_colors, m_shell, m_user, m_font = "monospace", m_fontConfig;
    QTimer m_timer;
};
