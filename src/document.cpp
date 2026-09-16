#include "document.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringDecoder>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

void Document::setText(const QString &text) {
    if (m_text == text) return;
    m_text = text;
    emit textChanged(); emit stateChanged();
}
void Document::clearError() { m_error.clear(); emit errorChanged(); }
bool Document::fail(const QString &message) {
    m_pending = None;
    m_error = message; emit errorChanged(); return false;
}
void Document::request(Action action) {
    m_pending = action;
    if (modified()) emit confirmationRequested(); else proceed();
}
void Document::requestNew() { request(New); }
void Document::requestOpen() { request(Open); }
void Document::requestClose() { request(Close); }
void Document::proceed() {
    const Action action = m_pending;
    m_pending = None;
    if (action == New) {
        m_url = QUrl(); m_saved.clear(); m_crlf = false; m_bom = false;
        setText({}); emit stateChanged();
    } else if (action == Open) emit fileDialogRequested(false);
    else if (action == Close) emit closeReady();
}
void Document::resolveUnsaved(const QString &choice) {
    if (choice == "save") save();
    else if (choice == "discard") proceed();
    else m_pending = None;
}
void Document::save() {
    if (m_url.isEmpty()) emit fileDialogRequested(true);
    else if (write(m_url)) proceed();
}
void Document::saveAs() { emit fileDialogRequested(true); }
void Document::cancelFile() { m_pending = None; }
void Document::acceptFile(const QUrl &url, bool saving) {
    if (saving) { if (write(url)) proceed(); }
    else open(url);
}
bool Document::open(const QUrl &url) {
    if (!url.isLocalFile()) return fail(tr("ローカルのUTF-8テキストファイルを選んでください。"));
    constexpr qint64 maxBytes = 2 * 1024 * 1024;
    const int fd = ::open(QFile::encodeName(url.toLocalFile()).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return fail(tr("ファイルを開けませんでした。場所とアクセス権を確認してください。"));
    struct stat info {};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size > maxBytes) {
        ::close(fd);
        return fail(tr("2 MiB以下の通常のテキストファイルを選んでください。"));
    }
    QFile file;
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd); return fail(file.errorString());
    }
    const QByteArray bytes = file.read(maxBytes + 1);
    if (file.error() != QFileDevice::NoError) return fail(file.errorString());
    if (bytes.size() > maxBytes) return fail(tr("ファイルが2 MiBを超えています。"));
    QStringDecoder decode(QStringDecoder::Utf8);
    QString text = decode(bytes);
    if (decode.hasError() || text.contains(QChar::Null))
        return fail(tr("UTF-8のテキストではありません。元のファイルは変更していません。"));
    m_crlf = text.contains("\r\n");
    m_bom = bytes.startsWith("\xef\xbb\xbf");
    text.replace("\r\n", "\n");
    m_url = url; m_saved = text;
    setText(text); clearError(); emit stateChanged(); return true;
}
bool Document::write(const QUrl &url) {
    if (!url.isLocalFile()) return fail(tr("保存先にはローカルファイルを選んでください。"));
    QSaveFile file(url.toLocalFile());
    // Never fall back to truncating the original file when atomic saving fails.
    if (!file.open(QIODevice::WriteOnly)) return fail(tr("保存できませんでした：%1").arg(file.errorString()));
    QString output = m_text;
    if (m_crlf) output.replace("\n", "\r\n");
    QByteArray bytes = output.toUtf8();
    if (m_bom) bytes.prepend("\xef\xbb\xbf");
    if (file.write(bytes) != bytes.size() || !file.commit())
        return fail(tr("保存できませんでした：%1").arg(file.errorString()));
    m_url = url; m_saved = m_text; clearError(); emit stateChanged(); return true;
}
