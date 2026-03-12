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

#include <QBuffer>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QLibraryInfo>
#include <QPixmap>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <ShlObj.h>
#include <shellapi.h>
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

    fileExtension = QStringLiteral("qs") + QString::number(qHash(appName), 16).left(4);

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

    if (!scopeRegistered && !items.isEmpty()) {
        registerCrawlScope();
        registerFileType();
        scopeRegistered = true;
    }

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

void WindowsSearchBackend::uninstall()
{
    QDir base(baseDir);
    if (base.exists()) {
        base.removeRecursively();
    }

    unregisterCrawlScope();
    unregisterFileType();
    scopeRegistered = false;

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
    QString dir = domainDir(domain);
    QString path = dir + QLatin1Char('/') + title + QLatin1Char('.') + fileExtension;

    if (!QFile::exists(path)) {
        return path;
    }

    // Avoid filename collisions.
    // TODO: Find a more elegant way to handle this.
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

    QString url = QStringLiteral("file:///") + baseDir;
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
    if (FAILED(hr)) {
        return;
    }

    ISearchCrawlScopeManager *scopeManager = nullptr;
    hr = catalogManager->GetCrawlScopeManager(&scopeManager);
    catalogManager->Release();
    if (FAILED(hr)) {
        return;
    }

    QString url = QStringLiteral("file:///") + baseDir;
    scopeManager->RemoveScopeRule(reinterpret_cast<const wchar_t *>(url.utf16()));
    scopeManager->SaveAll();
    scopeManager->Release();
}

void WindowsSearchBackend::registerFileType()
{
    QString nativeAppPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QString appFileName = QFileInfo(nativeAppPath).fileName();
    QString classesBase = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\");

    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty()) {
        appName = QStringLiteral("QSearchable");
    }

    // Set our App Paths key.
    {
        QString key = QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\")
                      + appFileName;
        QSettings s(key, QSettings::NativeFormat);
        s.setValue(QStringLiteral("."), nativeAppPath);
        s.setValue(QStringLiteral("Path"),
                   QDir::toNativeSeparators(QLibraryInfo::path(QLibraryInfo::BinariesPath)));
        s.sync();
    }

    // Register file extension -> progId.
    {
        QSettings s(classesBase + QStringLiteral(".") + fileExtension, QSettings::NativeFormat);
        s.setValue(QStringLiteral("."), progId);
        s.setValue(QStringLiteral("Content Type"), QStringLiteral("application/x-qsearchable"));
        s.setValue(QStringLiteral("PerceivedType"), QStringLiteral("document"));
        s.sync();
    }

    // Register "friendly" name and hide our search file extension (when possible.)
    {
        QSettings s(classesBase + progId, QSettings::NativeFormat);
        s.setValue(QStringLiteral("."), appName);
        s.setValue(QStringLiteral("FriendlyTypeName"), appName);
        s.setValue(QStringLiteral("NeverShowExt"), QStringLiteral(""));
        s.sync();
    }

    // Create shell open command.
    {
        QSettings s(classesBase + progId + QStringLiteral("\\shell\\open\\command"),
                    QSettings::NativeFormat);
        s.setValue(QStringLiteral("."),
                   QStringLiteral("\"") + nativeAppPath + QStringLiteral("\" \"%1\""));
        s.sync();
    }

    // Register DefaultIcon for our search file type.
    {
        QSettings s(classesBase + progId + QStringLiteral("\\DefaultIcon"),
                    QSettings::NativeFormat);
        QString iconPath = saveAppIcon();
        if (!iconPath.isEmpty()) {
            s.setValue(QStringLiteral("."), QDir::toNativeSeparators(iconPath));
        } else {
            s.setValue(QStringLiteral("."), nativeAppPath + QStringLiteral(",0"));
        }
        s.sync();
    }

    // Tell Explorer to re-check file associations.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void WindowsSearchBackend::unregisterFileType()
{
    // Undo everything we did in registerFileType()
    QString appFileName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    QString classesBase = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\");

    {
        QString key = QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\")
                      + appFileName;
        QSettings s(key, QSettings::NativeFormat);
        s.clear();
        s.sync();
    }
    {
        QSettings s(classesBase + QStringLiteral(".") + fileExtension, QSettings::NativeFormat);
        s.clear();
        s.sync();
    }
    {
        QSettings s(classesBase + progId, QSettings::NativeFormat);
        s.clear();
        s.sync();
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

QString WindowsSearchBackend::saveAppIcon()
{
    QDir().mkpath(baseDir);
    QString icoPath = baseDir + QStringLiteral("/app.ico");

    // Attempt to copy the icon from the exe binary.
    QString appPath = QCoreApplication::applicationFilePath();
    HICON hIcons[1] = {};
    UINT found = ExtractIconExW(
        reinterpret_cast<const wchar_t *>(appPath.utf16()),
        0, hIcons, nullptr, 1);

    if (found >= 1 && hIcons[0]) {
        // Use the exe's embedded icon.
        QIcon appIcon;

        // Convert HICON to QImage.
        QPixmap pm = QPixmap::fromImage(QImage::fromHICON(hIcons[0]));
        if (!pm.isNull()) {
            appIcon.addPixmap(pm);
        }
        DestroyIcon(hIcons[0]);

        // Also try QGuiApplication::windowIcon() which may have more sizes.
        QIcon windowIcon = QGuiApplication::windowIcon();
        if (!windowIcon.isNull()) {
            appIcon = windowIcon;
        }

        if (!appIcon.isNull()) {
            if (writeIcoFile(icoPath, appIcon)) {
                return icoPath;
            }
        }
    }

    // Final fallback: try QGuiApplication::windowIcon() alone.
    QIcon appIcon = QGuiApplication::windowIcon();
    if (!appIcon.isNull()) {
        if (writeIcoFile(icoPath, appIcon)) {
            return icoPath;
        }
    }

    qWarning("QSearchable: could not extract app icon from executable or windowIcon()");
    return QString();
}

bool WindowsSearchBackend::writeIcoFile(const QString &path, const QIcon &icon)
{
    static const int sizes[] = {16, 32, 48, 256};
    QList<QByteArray> pngEntries;
    QList<int> entrySizes;

    for (int size : sizes) {
        QImage img = icon.pixmap(size, size).toImage();
        if (img.isNull()) {
            continue;
        }

        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        if (!png.isEmpty()) {
            pngEntries.append(png);
            entrySizes.append(size);
        }
    }

    if (pngEntries.isEmpty()) {
        return false;
    }

    QByteArray ico;
    QDataStream ds(&ico, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    // ICO header: reserved(2) + type(2) + count(2)
    ds << quint16(0) << quint16(1) << quint16(pngEntries.size());

    // Directory entries: 16 bytes each
    int dataOffset = 6 + 16 * pngEntries.size();
    for (int i = 0; i < pngEntries.size(); ++i) {
        int s = entrySizes[i];
        ds << quint8(s < 256 ? s : 0);         // width
        ds << quint8(s < 256 ? s : 0);         // height
        ds << quint8(0);                        // color palette
        ds << quint8(0);                        // reserved
        ds << quint16(1);                       // color planes
        ds << quint16(32);                      // bits per pixel
        ds << quint32(pngEntries[i].size());    // image data size
        ds << quint32(dataOffset);              // image data offset
        dataOffset += pngEntries[i].size();
    }

    // Image data (PNG-compressed)
    for (const QByteArray &png : pngEntries) {
        ds.writeRawData(png.constData(), png.size());
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("QSearchable: failed to write %s: %s",
                 qPrintable(path), qPrintable(file.errorString()));
        return false;
    }
    file.write(ico);
    file.close();
    return true;
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
        // Send file path to existing instance via WM_COPYDATA.
        QByteArray data = targetFile.toUtf8();
        COPYDATASTRUCT cds = {};
        cds.cbData = static_cast<DWORD>(data.size());
        cds.lpData = data.data();
        SendMessageW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));

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
