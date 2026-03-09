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

#include "SearchableItemStore.h"

void SearchableItemStore::addItems(const QList<QSearchableItem> &items)
{
    for (const QSearchableItem &item : items) {
        const QString &id = item.uniqueIdentifier();

        // Remove old domain mapping if replacing an existing item
        auto existingIt = this->items.find(id);
        if (existingIt != items.end()) {
            const QString &oldDomain = existingIt->domainIdentifier();
            if (!oldDomain.isEmpty()) {
                domainToIds.remove(oldDomain, id);
            }
        }

        this->items.insert(id, item);

        if (!item.domainIdentifier().isEmpty()) {
            domainToIds.insert(item.domainIdentifier(), id);
        }
    }
}

void SearchableItemStore::removeItems(const QStringList &identifiers)
{
    for (const QString &id : identifiers) {
        auto it = items.find(id);
        if (it != items.end()) {
            const QString &domain = it->domainIdentifier();
            if (!domain.isEmpty()) {
                domainToIds.remove(domain, id);
            }
            items.erase(it);
        }
    }
}

void SearchableItemStore::removeItemsInDomains(const QStringList &domainIdentifiers)
{
    for (const QString &domain : domainIdentifiers) {
        const QList<QString> ids = domainToIds.values(domain);
        for (const QString &id : ids) {
            items.remove(id);
        }
        domainToIds.remove(domain);
    }
}

void SearchableItemStore::removeAllItems()
{
    items.clear();
    domainToIds.clear();
}

QList<QSearchableItem> SearchableItemStore::search(const QStringList &terms) const
{
    if (terms.isEmpty())
        return {};

    QList<QSearchableItem> results;
    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        if (matchesTerms(it.value(), terms))
            results.append(it.value());
    }
    return results;
}

QSearchableItem SearchableItemStore::item(const QString &identifier) const
{
    auto it = items.find(identifier);
    Q_ASSERT(it != items.end());
    return it.value();
}

bool SearchableItemStore::contains(const QString &identifier) const
{
    return items.contains(identifier);
}

bool SearchableItemStore::matchesTerms(const QSearchableItem &item, const QStringList &terms) const
{
    // All terms must match at least one field (AND logic)
    for (const QString &term : terms) {
        bool found = false;

        if (item.title().contains(term, Qt::CaseInsensitive)) {
            found = true;
        } else if (item.contentDescription().contains(term, Qt::CaseInsensitive)) {
            found = true;
        } else if (item.displayName().contains(term, Qt::CaseInsensitive)) {
            found = true;
        } else {
            for (const QString &keyword : item.keywords()) {
                if (keyword.contains(term, Qt::CaseInsensitive)) {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
            return false;
    }
    return true;
}
