#include "theme.h"
#include <QFile>
#include <QProcess>

namespace {
QString read(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}
}
ThemeFiles::ThemeFiles(const QString &home, QObject *parent) : QObject(parent), m_home(home) {
    reload();
    connect(&m_timer, &QTimer::timeout, this, &ThemeFiles::reload);
    m_timer.start(1000);
}
void ThemeFiles::reload() {
    const QString dir = m_home + "/.local/state/omarchy/current/theme/";
    const QString colors = read(dir + "colors.toml");
    const QString shell = read(dir + "shell.toml");
    const QString user = read(m_home + "/.config/omarchy/shell.toml");
    const QString fontConfig = read(m_home + "/.config/fontconfig/fonts.conf");
    bool fontChanged = false;
    if (m_font == "monospace" || fontConfig != m_fontConfig) {
        QProcess match;
        match.start("fc-match", {"-f", "%{family[0]}", "monospace"});
        if (match.waitForFinished(1000) && match.exitCode() == 0) {
            const QString family = QString::fromUtf8(match.readAllStandardOutput()).trimmed();
            if (!family.isEmpty()) { fontChanged = m_font != family; m_font = family; }
        }
        m_fontConfig = fontConfig;
    }
    if (m_colors == colors && m_shell == shell && m_user == user && !fontChanged) return;
    m_colors = colors; m_shell = shell; m_user = user;
    emit changed();
}
