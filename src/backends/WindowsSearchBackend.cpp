// MIT License
//
// Copyright (c) 2026 Eric Gregory <mrericsir@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "WindowsSearchBackend.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <ShlObj.h>

WindowsSearchBackend::WindowsSearchBackend(QObject *parent)
    : QSearchableIndexBackend(parent)
{
    QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    appName = QFileInfo(QCoreApplication::applicationFilePath()).completeBaseName();
    if (appName.isEmpty()) {
        appName = QStringLiteral("QSearchable");
    }

    baseDir = localAppData + QStringLiteral("/QSearchable/") + appName;

    QByteArray nameHash = QCryptographicHash::hash(
        appName.toUtf8(), QCryptographicHash::Sha256).toHex();
    fileExtension = QStringLiteral("qs") + QString::fromLatin1(nameHash.left(6));

    clsid = generateClsid();
    windowClassName = QStringLiteral("QSearchable_") + appName + QStringLiteral("_IPC");

    setupIpc();
    checkPendingActivation();
}

WindowsSearchBackend::~WindowsSearchBackend()
{
    if (ipcWindow) {
        DestroyWindow(ipcWindow);
        UnregisterClassW(reinterpret_cast<const wchar_t *>(windowClassName.utf16()),
                         GetModuleHandle(nullptr));
    }
}

bool WindowsSearchBackend::isSupported() const
{
    return true;
}

void WindowsSearchBackend::indexItems(const QList<QSearchableItem> &items)
{
    if (!QDir().mkpath(baseDir)) {
        emit errorOccurred(QStringLiteral("Failed to create base directory: %1").arg(baseDir));
        return;
    }

    int indexed = 0;
    for (const QSearchableItem &item : items) {
        QString domain = item.domainIdentifier();
        if (domain.isEmpty()) {
            domain = QStringLiteral("default");
        }

        QString dir = domainDir(domain);
        if (!QDir().mkpath(dir)) {
            qWarning("WindowsSearchBackend: failed to create domain directory: %s",
                     qPrintable(dir));
            continue;
        }

        // Remove old file for this item (title may have changed)
        QString oldFile = findFileForId(domain, item.uniqueIdentifier());
        if (!oldFile.isEmpty()) {
            QFile::remove(oldFile);
        }

        QString filePath = itemFilePath(domain, item);
        if (!writeItemFile(filePath, item)) {
            qWarning("WindowsSearchBackend: failed to write item file: %s",
                     qPrintable(filePath));
            continue;
        }
        ++indexed;
    }

    if (indexed > 0) {
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH,
                       reinterpret_cast<const void *>(baseDir.toStdWString().c_str()),
                       nullptr);
    }

    const int count = indexed;
    QTimer::singleShot(0, this, [this, count]() {
        emit indexingSucceeded(count);
    });
}

void WindowsSearchBackend::removeItems(const QStringList &identifiers)
{
    QDir base(baseDir);
    const QStringList domains = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &domain : domains) {
        for (const QString &id : identifiers) {
            QString filePath = findFileForId(domain, id);
            if (!filePath.isEmpty()) {
                QFile::remove(filePath);
            }
        }
    }

    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH,
                   reinterpret_cast<const void *>(baseDir.toStdWString().c_str()),
                   nullptr);

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

void WindowsSearchBackend::removeItemsInDomains(const QStringList &domainIdentifiers)
{
    for (const QString &domain : domainIdentifiers) {
        QString dir = domainDir(domain);
        QDir domainDirectory(dir);
        if (domainDirectory.exists()) {
            domainDirectory.removeRecursively();
        }
    }

    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH,
                   reinterpret_cast<const void *>(baseDir.toStdWString().c_str()),
                   nullptr);

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

void WindowsSearchBackend::removeAllItems()
{
    QDir base(baseDir);
    if (base.exists()) {
        base.removeRecursively();
    }

    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH,
                   reinterpret_cast<const void *>(baseDir.toStdWString().c_str()),
                   nullptr);

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

bool WindowsSearchBackend::isRelayInstance() const
{
    return QCoreApplication::instance()->property("QSearchable_relay").toBool();
}

bool WindowsSearchBackend::isInstalled() const
{
    std::wstring key = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
                       L"PropertySystem\\PropertyHandlers\\."
                       + fileExtension.toStdWString();

    HKEY hkey = nullptr;
    LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_READ, &hkey);
    if (res != ERROR_SUCCESS) {
        return false;
    }

    // Check that the value matches our expected CLSID.
    wchar_t buf[128] = {};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    res = RegQueryValueExW(hkey, nullptr, nullptr, &type,
                           reinterpret_cast<LPBYTE>(buf), &size);
    RegCloseKey(hkey);

    if (res != ERROR_SUCCESS || type != REG_SZ) {
        return false;
    }

    return QString::fromWCharArray(buf) == clsid;
}

QString WindowsSearchBackend::domainDir(const QString &domain) const
{
    return baseDir + QLatin1Char('/') + domain;
}

QString WindowsSearchBackend::itemFilePath(const QString &domain, const QSearchableItem &item) const
{
    QString title = sanitizeTitle(item.title());
    QString dir = domainDir(domain);
    QString path = dir + QLatin1Char('/') + title + QLatin1Char('.') + fileExtension;

    if (!QFile::exists(path)) {
        return path;
    }

    // Avoid filename collisions by appending an incrementing suffix, e.g. "Title (2).ext".
    for (int i = 2; ; ++i) {
        path = dir + QLatin1Char('/') + title + QStringLiteral(" (%1).").arg(i) + fileExtension;
        if (!QFile::exists(path)) {
            return path;
        }
    }
}

QString WindowsSearchBackend::findFileForId(const QString &domain, const QString &id) const
{
    QDir dir(domainDir(domain));
    const QStringList entries = dir.entryList(QDir::Files);
    for (const QString &entry : entries) {
        QString path = dir.absoluteFilePath(entry);
        if (parseIdFromFile(path) == id) {
            return path;
        }
    }
    return QString();
}

bool WindowsSearchBackend::writeItemFile(const QString &filePath, const QSearchableItem &item)
{
    QSettings settings(filePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("UniqueIdentifier"), item.uniqueIdentifier());
    settings.setValue(QStringLiteral("DomainIdentifier"), item.domainIdentifier());
    settings.setValue(QStringLiteral("Title"), item.title());
    settings.setValue(QStringLiteral("ContentDescription"), item.contentDescription());
    settings.setValue(QStringLiteral("DisplayName"), item.displayName());
    settings.setValue(QStringLiteral("Keywords"), item.keywords().join(QStringLiteral(", ")));

    settings.setValue(QStringLiteral("AppName"), appName);

    if (item.url().isValid()) {
        settings.setValue(QStringLiteral("URL"), item.url().toString());
    }

    if (item.timestamp().isValid()) {
        settings.setValue(QStringLiteral("Timestamp"),
        item.timestamp().toString(Qt::ISODate));
    }

    settings.sync();
    return settings.status() == QSettings::NoError;
}

QString WindowsSearchBackend::parseIdFromFile(const QString &filePath) const
{
    QSettings settings(filePath, QSettings::IniFormat);
    return settings.value(QStringLiteral("UniqueIdentifier")).toString();
}

QString WindowsSearchBackend::sanitizeTitle(const QString &title) const
{
    QString sanitized = title;
    sanitized.remove(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")));
    sanitized = sanitized.simplified();

    if (sanitized.length() > 100) {
        sanitized = sanitized.left(100);
    }

    if (sanitized.isEmpty()) {
        sanitized = QStringLiteral("item");
    }

    return sanitized;
}

QString WindowsSearchBackend::generateClsid() const
{
    // Generate a deterministic CLSID from the exe filename so that each
    // app using QSearchable gets its own COM registration.
    // Uses SHA-256 (not qHash, which is randomized per-process in Qt 6).
    QByteArray hash = QCryptographicHash::hash(
        appName.toUtf8(), QCryptographicHash::Sha256).toHex();

    // Format 16 bytes of the hash as a GUID: {8-4-4-4-12}
    return QStringLiteral("{%1-%2-%3-%4-%5}")
        .arg(QString::fromLatin1(hash.mid(0, 8)))
        .arg(QString::fromLatin1(hash.mid(8, 4)))
        .arg(QString::fromLatin1(hash.mid(12, 4)))
        .arg(QString::fromLatin1(hash.mid(16, 4)))
        .arg(QString::fromLatin1(hash.mid(20, 12)));
}

void WindowsSearchBackend::setupIpc()
{
    // Check if an existing instance already has the IPC window.
    HWND existing = FindWindowExW(HWND_MESSAGE, nullptr,
                                  reinterpret_cast<const wchar_t *>(windowClassName.utf16()),
                                  nullptr);

    // Check if we were launched with a file argument.
    QStringList args = QCoreApplication::arguments();
    QString targetFile;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i].endsWith(QLatin1Char('.') + fileExtension)) {
            targetFile = args[i];
            break;
        }
    }

    if (existing && !targetFile.isEmpty()) {
        // Transfer foreground activation rights to the existing instance
        // so it can bring itself to the front after we quit.
        DWORD targetPid = 0;
        GetWindowThreadProcessId(existing, &targetPid);
        if (targetPid != 0) {
            AllowSetForegroundWindow(targetPid);
        }

        // Send file path to existing instance via WM_COPYDATA.
        QByteArray data = targetFile.toUtf8();
        COPYDATASTRUCT cds = {};
        cds.cbData = static_cast<DWORD>(data.size());
        cds.lpData = data.data();
        SendMessageW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));

        // Mark as relay so the app can skip showing UI.
        QCoreApplication::instance()->setProperty("QSearchable_relay", true);

        // Quit temporary instance when complete.
        QTimer::singleShot(0, []() {
            QCoreApplication::quit();
        });
        return;
    }

    // Register window class and create hidden message-only window
    WNDCLASSW wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = reinterpret_cast<const wchar_t *>(windowClassName.utf16());
    RegisterClassW(&wc);

    ipcWindow = CreateWindowW(
        reinterpret_cast<const wchar_t *>(windowClassName.utf16()),
        L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
        GetModuleHandle(nullptr), nullptr);

    SetWindowLongPtrW(ipcWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

void WindowsSearchBackend::checkPendingActivation()
{
    QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i].endsWith(QLatin1Char('.') + fileExtension)) {
            QString filePath = args[i];
            QTimer::singleShot(0, this, [this, filePath]() {
                handleActivation(filePath);
            });
            return;
        }
    }
}

void WindowsSearchBackend::handleActivation(const QString &filePath)
{
    QString id = parseIdFromFile(filePath);
    if (!id.isEmpty()) {
        emit activated(id);
    }
}

LRESULT CALLBACK WindowsSearchBackend::windowProc(HWND hwnd, UINT msg,
                                                   WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_COPYDATA) {
        auto *cds = reinterpret_cast<COPYDATASTRUCT *>(lParam);
        QString filePath = QString::fromUtf8(
            reinterpret_cast<const char *>(cds->lpData),
            static_cast<int>(cds->cbData));

        auto *self = reinterpret_cast<WindowsSearchBackend *>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self) {
            self->handleActivation(filePath);
        }

        return TRUE;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
