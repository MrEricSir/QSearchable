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
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <ShlObj.h>
#include <objbase.h>
#include <searchapi.h>

WindowsSearchBackend::WindowsSearchBackend(QObject *parent)
    : QSearchableIndexBackend(parent)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty()) {
        appName = QStringLiteral("QSearchable");
    }

    baseDir = localAppData + QStringLiteral("/QSearchable/") + appName;

    QString cleanName = appName;
    cleanName.remove(QRegularExpression(QStringLiteral("[^a-zA-Z0-9]")));
    if (cleanName.isEmpty()) {
        cleanName = QStringLiteral("app");
    }
    fileExtension = QStringLiteral("qs") + cleanName.toLower();

    progId = QStringLiteral("QSearchable.") + appName;
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
    CoUninitialize();
}

bool WindowsSearchBackend::isSupported() const
{
    return true;
}

void WindowsSearchBackend::indexItems(const QList<QSearchableItem> &items)
{
    QDir().mkpath(baseDir);

    int indexed = 0;
    for (const QSearchableItem &item : items) {
        QString domain = item.domainIdentifier();
        if (domain.isEmpty()) {
            domain = QStringLiteral("default");
        }

        QString dir = domainDir(domain);
        QDir().mkpath(dir);

        // Remove old file for this item (title may have changed)
        QString oldFile = findFileForId(domain, item.uniqueIdentifier());
        if (!oldFile.isEmpty()) {
            QFile::remove(oldFile);
        }

        QString filePath = itemFilePath(domain, item);
        writeItemFile(filePath, item);
        ++indexed;
    }

    if (!scopeRegistered && !items.isEmpty()) {
        registerCrawlScope();
        registerFileType();
        scopeRegistered = true;
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

    unregisterCrawlScope();
    unregisterFileType();
    scopeRegistered = false;

    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH,
                   reinterpret_cast<const void *>(baseDir.toStdWString().c_str()),
                   nullptr);

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

QString WindowsSearchBackend::domainDir(const QString &domain) const
{
    return baseDir + QLatin1Char('/') + domain;
}

QString WindowsSearchBackend::itemFilePath(const QString &domain, const QSearchableItem &item) const
{
    QString title = sanitizeTitle(item.title());
    QString hash = hashId(item.uniqueIdentifier());
    return domainDir(domain) + QLatin1Char('/') + title
           + QStringLiteral(" [") + hash + QStringLiteral("].") + fileExtension;
}

QString WindowsSearchBackend::findFileForId(const QString &domain, const QString &id) const
{
    QString hash = hashId(id);
    QString suffix = QStringLiteral("[") + hash + QStringLiteral("].");
    QDir dir(domainDir(domain));
    const QStringList entries = dir.entryList(QDir::Files);
    for (const QString &entry : entries) {
        if (entry.contains(suffix)) {
            return dir.absoluteFilePath(entry);
        }
    }
    return QString();
}

void WindowsSearchBackend::writeItemFile(const QString &filePath, const QSearchableItem &item)
{
    QSettings settings(filePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("UniqueIdentifier"), item.uniqueIdentifier());
    settings.setValue(QStringLiteral("DomainIdentifier"), item.domainIdentifier());
    settings.setValue(QStringLiteral("Title"), item.title());
    settings.setValue(QStringLiteral("ContentDescription"), item.contentDescription());
    settings.setValue(QStringLiteral("DisplayName"), item.displayName());
    settings.setValue(QStringLiteral("Keywords"), item.keywords().join(QStringLiteral(", ")));
    
    if (item.url().isValid()) {
        settings.setValue(QStringLiteral("URL"), item.url().toString());
    }
    
    if (item.timestamp().isValid()) {
        settings.setValue(QStringLiteral("Timestamp"), 
        item.timestamp().toString(Qt::ISODate));
    }

    settings.sync();
}

QString WindowsSearchBackend::parseIdFromFile(const QString &filePath) const
{
    QSettings settings(filePath, QSettings::IniFormat);
    return settings.value(QStringLiteral("UniqueIdentifier")).toString();
}

QString WindowsSearchBackend::hashId(const QString &id) const
{
    return QString::number(qHash(id, 0), 16);
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

void WindowsSearchBackend::registerCrawlScope()
{
    ISearchManager *searchManager = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(CSearchManager), nullptr, CLSCTX_ALL,
                                 IID_PPV_ARGS(&searchManager));
    if (FAILED(hr)) {
        return;
    }

    ISearchCatalogManager *catalogManager = nullptr;
    hr = searchManager->GetCatalog(L"SystemIndex", &catalogManager);
    searchManager->Release();
    if (FAILED(hr)) {
        return;
    }

    ISearchCrawlScopeManager *scopeManager = nullptr;
    hr = catalogManager->GetCrawlScopeManager(&scopeManager);
    catalogManager->Release();
    if (FAILED(hr)) {
        return;
    }

    QString url = QStringLiteral("file:///") + QDir::toNativeSeparators(baseDir);
    scopeManager->AddUserScopeRule(reinterpret_cast<const wchar_t *>(url.utf16()),
                                   TRUE, TRUE, FALSE);
    scopeManager->SaveAll();
    scopeManager->Release();
}

void WindowsSearchBackend::unregisterCrawlScope()
{
    ISearchManager *searchManager = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(CSearchManager), nullptr, CLSCTX_ALL,
                                 IID_PPV_ARGS(&searchManager));
    if (FAILED(hr)) {
        return;
    }

    ISearchCatalogManager *catalogManager = nullptr;
    hr = searchManager->GetCatalog(L"SystemIndex", &catalogManager);
    searchManager->Release();
    if (FAILED(hr))
        return;

    ISearchCrawlScopeManager *scopeManager = nullptr;
    hr = catalogManager->GetCrawlScopeManager(&scopeManager);
    catalogManager->Release();
    if (FAILED(hr)) {
        return;
    }

    QString url = QStringLiteral("file:///") + QDir::toNativeSeparators(baseDir);
    scopeManager->RemoveScopeRule(reinterpret_cast<const wchar_t *>(url.utf16()));
    scopeManager->SaveAll();
    scopeManager->Release();
}

// --- File type association ---

void WindowsSearchBackend::registerFileType()
{
    QString extKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\.") + fileExtension;
    QSettings extSettings(extKey, QSettings::NativeFormat);
    extSettings.setValue(QStringLiteral("."), progId);

    QString progKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\") + progId;
    QSettings progSettings(progKey, QSettings::NativeFormat);
    QString appPath = QCoreApplication::applicationFilePath();
    progSettings.setValue(QStringLiteral("shell/open/command/."),
                          QStringLiteral("\"") + QDir::toNativeSeparators(appPath)
                              + QStringLiteral("\" \"%1\""));
}

void WindowsSearchBackend::unregisterFileType()
{
    QString extKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\.") + fileExtension;
    QSettings extSettings(extKey, QSettings::NativeFormat);
    extSettings.clear();

    QString progKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\") + progId;
    QSettings progSettings(progKey, QSettings::NativeFormat);
    progSettings.clear();
}

void WindowsSearchBackend::setupIpc()
{
    // Check if an existing instance already has the IPC window
    HWND existing = FindWindowExW(HWND_MESSAGE, nullptr,
                                  reinterpret_cast<const wchar_t *>(windowClassName.utf16()),
                                  nullptr);

    // Check if we were launched with a file argument
    QStringList args = QCoreApplication::arguments();
    QString targetFile;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i].endsWith(QLatin1Char('.') + fileExtension)) {
            targetFile = args[i];
            break;
        }
    }

    if (existing && !targetFile.isEmpty()) {
        // Send file path to existing instance via WM_COPYDATA
        QByteArray data = targetFile.toUtf8();
        COPYDATASTRUCT cds = {};
        cds.cbData = static_cast<DWORD>(data.size());
        cds.lpData = data.data();
        SendMessageW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));

        // Schedule quit of this second instance
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
