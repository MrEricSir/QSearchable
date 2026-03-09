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

#include "GnomeSearchBackend.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QTimer>
#include <QVariantMap>

QString GnomeSearchBackend::busName;
QString GnomeSearchBackend::objectPath = QStringLiteral("/SearchProvider");

// D-Bus adaptor that exposes the org.gnome.Shell.SearchProvider2 interface
class SearchProvider2Adaptor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.gnome.Shell.SearchProvider2")

public:
    explicit SearchProvider2Adaptor(GnomeSearchBackend *backend)
        : QObject(backend)
        , backend(backend)
    {
    }

public slots:
    QStringList GetInitialResultSet(const QStringList &terms)
    {
        return backend->getInitialResultSet(terms);
    }

    QStringList GetSubsearchResultSet(const QStringList &previousResults, const QStringList &terms)
    {
        return backend->getSubsearchResultSet(previousResults, terms);
    }

    QList<QVariantMap> GetResultMetas(const QStringList &identifiers)
    {
        return backend->getResultMetas(identifiers);
    }

    void ActivateResult(const QString &identifier, const QStringList &terms, uint timestamp)
    {
        backend->activateResult(identifier, terms, timestamp);
    }

    void LaunchSearch(const QStringList &terms, uint timestamp)
    {
        backend->launchSearch(terms, timestamp);
    }

private:
    GnomeSearchBackend *backend;
};

GnomeSearchBackend::GnomeSearchBackend(QObject *parent)
    : QSearchableIndexBackend(parent)
    , registered(false)
{
    registerOnDBus();
}

GnomeSearchBackend::~GnomeSearchBackend()
{
    unregisterFromDBus();
}

void GnomeSearchBackend::setBusName(const QString &busName)
{
    busName = busName;
}

void GnomeSearchBackend::setObjectPath(const QString &objectPath)
{
    objectPath = objectPath;
}

bool GnomeSearchBackend::isSupported() const
{
    return registered;
}

void GnomeSearchBackend::indexItems(const QList<QSearchableItem> &items)
{
    store.addItems(items);

    const int count = items.size();
    QTimer::singleShot(0, this, [this, count]() {
        emit indexingSucceeded(count);
    });
}

void GnomeSearchBackend::removeItems(const QStringList &identifiers)
{
    store.removeItems(identifiers);

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

void GnomeSearchBackend::removeItemsInDomains(const QStringList &domainIdentifiers)
{
    store.removeItemsInDomains(domainIdentifiers);

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

void GnomeSearchBackend::removeAllItems()
{
    store.removeAllItems();

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

QStringList GnomeSearchBackend::getInitialResultSet(const QStringList &terms)
{
    QStringList results;
    const QList<QSearchableItem> items = store.search(terms);
    for (const QSearchableItem &item : items)
        results.append(item.uniqueIdentifier());
    return results;
}

QStringList GnomeSearchBackend::getSubsearchResultSet(const QStringList &previousResults, const QStringList &terms)
{
    Q_UNUSED(previousResults);
    // Re-search the full store — the subset optimization isn't needed for typical item counts
    return getInitialResultSet(terms);
}

QList<QVariantMap> GnomeSearchBackend::getResultMetas(const QStringList &identifiers)
{
    QList<QVariantMap> metas;
    for (const QString &id : identifiers) {
        if (!store.contains(id))
            continue;

        const QSearchableItem item = store.item(id);
        QVariantMap meta;
        meta[QStringLiteral("id")] = id;
        meta[QStringLiteral("name")] = item.title().isEmpty() ? item.displayName() : item.title();
        if (!item.contentDescription().isEmpty())
            meta[QStringLiteral("description")] = item.contentDescription();
        metas.append(meta);
    }
    return metas;
}

void GnomeSearchBackend::activateResult(const QString &identifier, const QStringList &terms, uint timestamp)
{
    Q_UNUSED(terms);
    Q_UNUSED(timestamp);
    emit activated(identifier);
}

void GnomeSearchBackend::launchSearch(const QStringList &terms, uint timestamp)
{
    Q_UNUSED(terms);
    Q_UNUSED(timestamp);
}

void GnomeSearchBackend::registerOnDBus()
{
    QString busName = busName;
    if (busName.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QString desktopName = QCoreApplication::desktopFileName();
#else
        QString desktopName;
#endif
        if (desktopName.isEmpty()) {
            QString appName = QCoreApplication::applicationName();
            if (appName.isEmpty())
                appName = QStringLiteral("QSearchable");
            busName = QStringLiteral("org.qsearchable.%1.SearchProvider").arg(appName);
        } else {
            // Strip .desktop suffix if present
            if (desktopName.endsWith(QStringLiteral(".desktop")))
                desktopName.chop(8);
            busName = desktopName + QStringLiteral(".SearchProvider");
        }
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    new SearchProvider2Adaptor(this);

    if (!bus.registerObject(objectPath, this, QDBusConnection::ExportAdaptors))
        return;

    if (!bus.registerService(busName))
        return;

    registered = true;
}

void GnomeSearchBackend::unregisterFromDBus()
{
    if (!registered)
        return;

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(objectPath);
    registered = false;
}

#include "GnomeSearchBackend.moc"
