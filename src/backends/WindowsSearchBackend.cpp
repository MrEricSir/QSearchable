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
#include <QTextStream>
#include <QTimer>

#ifdef Q_OS_WIN
#include <ShlObj.h>
#endif

WindowsSearchBackend::WindowsSearchBackend(QObject *parent)
    : QSearchableIndexBackend(parent)
{
    QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty())
        appName = QStringLiteral("QSearchable");
    m_baseDir = localAppData + QStringLiteral("/QSearchable/") + appName;
}

bool WindowsSearchBackend::isSupported() const
{
    return true;
}

void WindowsSearchBackend::indexItems(const QList<QSearchableItem> &items)
{
    QDir().mkpath(m_baseDir);

    int indexed = 0;
    for (const QSearchableItem &item : items) {
        QString domain = item.domainIdentifier();
        if (domain.isEmpty())
            domain = QStringLiteral("default");

        QString dir = domainDir(domain);
        QDir().mkpath(dir);

        QString filePath = urlFilePath(domain, item.uniqueIdentifier());
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << QStringLiteral("[InternetShortcut]\n");
            stream << QStringLiteral("URL=qsearchable://") << item.uniqueIdentifier() << QStringLiteral("\n");
            if (!item.title().isEmpty())
                stream << QStringLiteral("; Title: ") << item.title() << QStringLiteral("\n");
            if (!item.contentDescription().isEmpty())
                stream << QStringLiteral("; Description: ") << item.contentDescription() << QStringLiteral("\n");
            file.close();
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
    QDir base(m_baseDir);
    const QStringList domains = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &domain : domains) {
        for (const QString &id : identifiers) {
            QString filePath = urlFilePath(domain, id);
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
    QDir base(m_baseDir);
    if (base.exists()) {
        base.removeRecursively();
        notifyShell(m_baseDir);
    }

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

QString WindowsSearchBackend::baseDir() const
{
    return m_baseDir;
}

QString WindowsSearchBackend::domainDir(const QString &domainIdentifier) const
{
    return m_baseDir + QLatin1Char('/') + domainIdentifier;
}

QString WindowsSearchBackend::urlFilePath(const QString &domainIdentifier, const QString &uniqueId) const
{
    return domainDir(domainIdentifier) + QLatin1Char('/') + uniqueId + QStringLiteral(".url");
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
