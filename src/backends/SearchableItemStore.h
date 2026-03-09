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

#ifndef SEARCHABLEITEMSTORE_H
#define SEARCHABLEITEMSTORE_H

#include "../QSearchableItem.h"

#include <QList>
#include <QMap>
#include <QMultiMap>
#include <QString>
#include <QStringList>

class SearchableItemStore
{
public:
    void addItems(const QList<QSearchableItem> &items);
    void removeItems(const QStringList &identifiers);
    void removeItemsInDomains(const QStringList &domainIdentifiers);
    void removeAllItems();

    QList<QSearchableItem> search(const QStringList &terms) const;
    QSearchableItem item(const QString &identifier) const;
    bool contains(const QString &identifier) const;

private:
    bool matchesTerms(const QSearchableItem &item, const QStringList &terms) const;

    QMap<QString, QSearchableItem> items;
    QMultiMap<QString, QString> domainToIds;
};

#endif // SEARCHABLEITEMSTORE_H
