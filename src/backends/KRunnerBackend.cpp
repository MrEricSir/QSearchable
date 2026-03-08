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

#include "KRunnerBackend.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QTimer>

QString KRunnerBackend::s_busName;
QString KRunnerBackend::s_objectPath = QStringLiteral("/KRunner");

// D-Bus adaptor that exposes the org.kde.krunner1 interface
class KRunner1Adaptor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.krunner1")

public:
    explicit KRunner1Adaptor(KRunnerBackend *backend)
        : QObject(backend)
        , backend(backend)
    {
    }

public slots:
    QList<KRunnerAction> Actions()
    {
        return backend->actions();
    }

    QList<KRunnerMatch> Match(const QString &query)
    {
        return backend->match(query);
    }

    void Run(const QString &matchId, const QString &actionId)
    {
        backend->run(matchId, actionId);
    }

private:
    KRunnerBackend *backend;
};

KRunnerBackend::KRunnerBackend(QObject *parent)
    : QSearchableIndexBackend(parent)
    , m_registered(false)
{
    qDBusRegisterMetaType<KRunnerMatch>();
    qDBusRegisterMetaType<QList<KRunnerMatch>>();
    qDBusRegisterMetaType<KRunnerAction>();
    qDBusRegisterMetaType<QList<KRunnerAction>>();

    registerOnDBus();
}

KRunnerBackend::~KRunnerBackend()
{
    unregisterFromDBus();
}

void KRunnerBackend::setBusName(const QString &busName)
{
    s_busName = busName;
}

void KRunnerBackend::setObjectPath(const QString &objectPath)
{
    s_objectPath = objectPath;
}

bool KRunnerBackend::isSupported() const
{
    return m_registered;
}

void KRunnerBackend::indexItems(const QList<QSearchableItem> &items)
{
    m_store.addItems(items);

    const int count = items.size();
    QTimer::singleShot(0, this, [this, count]() {
        emit indexingSucceeded(count);
    });
}

void KRunnerBackend::removeItems(const QStringList &identifiers)
{
    m_store.removeItems(identifiers);

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

void KRunnerBackend::removeItemsInDomains(const QStringList &domainIdentifiers)
{
    m_store.removeItemsInDomains(domainIdentifiers);

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

void KRunnerBackend::removeAllItems()
{
    m_store.removeAllItems();

    QTimer::singleShot(0, this, [this]() {
        emit removalSucceeded();
    });
}

QList<KRunnerAction> KRunnerBackend::actions()
{
    return {};
}

QList<KRunnerMatch> KRunnerBackend::match(const QString &query)
{
    QStringList terms = query.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QList<QSearchableItem> items = m_store.search(terms);

    QList<KRunnerMatch> matches;
    for (const QSearchableItem &item : items) {
        KRunnerMatch m;
        m.id = item.uniqueIdentifier();
        m.text = item.title().isEmpty() ? item.displayName() : item.title();
        m.iconName = QCoreApplication::applicationName();
        m.type = 2; // PossibleMatch
        m.relevance = computeRelevance(item, query);

        if (!item.contentDescription().isEmpty())
            m.properties[QStringLiteral("subtext")] = item.contentDescription();

        matches.append(m);
    }
    return matches;
}

void KRunnerBackend::run(const QString &matchId, const QString &actionId)
{
    Q_UNUSED(actionId);
    emit activated(matchId);
}

void KRunnerBackend::registerOnDBus()
{
    QString busName = s_busName;
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
            busName = QStringLiteral("org.qsearchable.%1.KRunner").arg(appName);
        } else {
            if (desktopName.endsWith(QStringLiteral(".desktop")))
                desktopName.chop(8);
            busName = desktopName + QStringLiteral(".KRunner");
        }
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    new KRunner1Adaptor(this);

    if (!bus.registerObject(s_objectPath, this, QDBusConnection::ExportAdaptors))
        return;

    if (!bus.registerService(busName))
        return;

    m_registered = true;
}

void KRunnerBackend::unregisterFromDBus()
{
    if (!m_registered)
        return;

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(s_objectPath);
    m_registered = false;
}

double KRunnerBackend::computeRelevance(const QSearchableItem &item, const QString &query) const
{
    QString title = item.title();
    if (title.isEmpty())
        title = item.displayName();

    if (title.compare(query, Qt::CaseInsensitive) == 0)
        return 1.0;
    if (title.startsWith(query, Qt::CaseInsensitive))
        return 0.8;
    return 0.5;
}

#include "KRunnerBackend.moc"
