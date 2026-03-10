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
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_WIN
#include <ShlObj.h>
#include <ShObjIdl.h>
#include <comdef.h>
#include <objbase.h>
#endif

WindowsSearchBackend::WindowsSearchBackend(QObject *parent)
    : QSearchableIndexBackend(parent)
{
#ifdef Q_OS_WIN
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty()) {
        appName = QStringLiteral("QSearchable");
    }

    baseDir = localAppData + QStringLiteral("/QSearchable/") + appName;
    urlScheme = QStringLiteral("qsearchable-") + appName.toLower() + QStringLiteral("://");
}

WindowsSearchBackend::~WindowsSearchBackend()
{
#ifdef Q_OS_WIN
    CoUninitialize();
#endif
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

        QString filePath = linkFilePath(domain, item.uniqueIdentifier());
        QString url = urlScheme + item.uniqueIdentifier();

        if (createShellLink(filePath, url, item.title())) {
            notifyShell(filePath);
            ++indexed;
        }
    }

    const int count = indexed;
    QTimer::singleShot(0, this, [this, count]() {
        emit indexingSucceeded(count);
    });
}

void WindowsSearchBackend::removeItems(const QStringList &identifiers)
{
    // Search all domain subdirectories for matching files
    QDir base(baseDir);
    const QStringList domains = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &domain : domains) {
        for (const QString &id : identifiers) {
            QString filePath = linkFilePath(domain, id);
            if (QFile::exists(filePath)) {
                QFile::remove(filePath);
                notifyShell(filePath);
            }
        }
    }

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
            notifyShell(dir);
        }
    }

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

void WindowsSearchBackend::removeAllItems()
{
    QDir base(baseDir);
    if (base.exists()) {
        base.removeRecursively();
        notifyShell(baseDir);
    }

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

QString WindowsSearchBackend::domainDir(const QString &domainIdentifier) const
{
    return baseDir + QLatin1Char('/') + domainIdentifier;
}

QString WindowsSearchBackend::linkFilePath(const QString &domainIdentifier, const QString &uniqueId) const
{
    return domainDir(domainIdentifier) + QLatin1Char('/') + uniqueId + QStringLiteral(".lnk");
}

bool WindowsSearchBackend::createShellLink(const QString &filePath, const QString &url, const QString &description)
{
#ifdef Q_OS_WIN
    IShellLinkW *shellLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, reinterpret_cast<void **>(&shellLink));
    if (FAILED(hr)) {
        return false;
    }

    // Set the URL as the target path argument so the default browser opens it.
    shellLink->SetPath(L"cmd.exe");
    shellLink->SetArguments(reinterpret_cast<const wchar_t *>(
        (QStringLiteral("/c start \"\" \"") + url + QStringLiteral("\"")).utf16()));
    shellLink->SetShowCmd(SW_HIDE);

    if (!description.isEmpty()) {
        shellLink->SetDescription(reinterpret_cast<const wchar_t *>(description.utf16()));
    }

    IPersistFile *persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&persistFile));
    if (SUCCEEDED(hr)) {
        hr = persistFile->Save(reinterpret_cast<const wchar_t *>(filePath.utf16()), TRUE);
        persistFile->Release();
    }

    shellLink->Release();
    return SUCCEEDED(hr);
#else
    Q_UNUSED(filePath);
    Q_UNUSED(url);
    Q_UNUSED(description);
    return false;
#endif
}

void WindowsSearchBackend::notifyShell(const QString &path)
{
#ifdef Q_OS_WIN
    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH,
                   reinterpret_cast<const void *>(path.toStdWString().c_str()),
                   nullptr);
#else
    Q_UNUSED(path);
#endif
}
