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

#ifndef KRUNNERBACKEND_H
#define KRUNNERBACKEND_H

#include "QSearchableIndexBackend.h"
#include "SearchableItemStore.h"

#include <QDBusArgument>
#include <QVariantMap>

// KRunner match result: (id, text, iconName, type, relevance, properties)
struct KRunnerMatch
{
    QString id;
    QString text;
    QString iconName;
    int type;
    double relevance;
    QVariantMap properties;
};
Q_DECLARE_METATYPE(KRunnerMatch)

inline QDBusArgument &operator<<(QDBusArgument &argument, const KRunnerMatch &match)
{
    argument.beginStructure();
    argument << match.id << match.text << match.iconName << match.type << match.relevance << match.properties;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, KRunnerMatch &match)
{
    argument.beginStructure();
    argument >> match.id >> match.text >> match.iconName >> match.type >> match.relevance >> match.properties;
    argument.endStructure();
    return argument;
}

// KRunner action: (id, text, iconName)
struct KRunnerAction
{
    QString id;
    QString text;
    QString iconName;
};
Q_DECLARE_METATYPE(KRunnerAction)

inline QDBusArgument &operator<<(QDBusArgument &argument, const KRunnerAction &action)
{
    argument.beginStructure();
    argument << action.id << action.text << action.iconName;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, KRunnerAction &action)
{
    argument.beginStructure();
    argument >> action.id >> action.text >> action.iconName;
    argument.endStructure();
    return argument;
}

class KRunnerBackend : public QSearchableIndexBackend
{
    Q_OBJECT

public:
    explicit KRunnerBackend(QObject *parent = nullptr);
    ~KRunnerBackend() override;

    static void setBusName(const QString &busName);
    static void setObjectPath(const QString &objectPath);

    bool isSupported() const override;

    void indexItems(const QList<QSearchableItem> &items) override;
    void removeItems(const QStringList &identifiers) override;
    void removeItemsInDomains(const QStringList &domainIdentifiers) override;
    void removeAllItems() override;
    void uninstall() override;

    // D-Bus krunner1 methods (called by the adaptor)
    QList<KRunnerAction> actions();
    QList<KRunnerMatch> match(const QString &query);
    void run(const QString &matchId, const QString &actionId);

private:
    void registerOnDBus();
    void unregisterFromDBus();
    double computeRelevance(const QSearchableItem &item, const QString &query) const;

    SearchableItemStore store;
    bool registered;

    static QString busName;
    static QString objectPath;
};

#endif // KRUNNERBACKEND_H
