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
#include "QSearchableItem.h"

#include <QTest>

class TestSearchableItemStore : public QObject
{
    Q_OBJECT

private slots:
    void addAndContains();
    void addReplacesExisting();
    void removeItems();
    void removeItemsNonexistent();
    void removeItemsInDomains();
    void removeItemsInDomainsMultiple();
    void removeAllItems();
    void searchByTitle();
    void searchByContentDescription();
    void searchByDisplayName();
    void searchByKeyword();
    void searchCaseInsensitive();
    void searchMultipleTermsAndLogic();
    void searchEmptyTerms();
    void searchNoMatch();
    void itemLookup();
    void domainTrackingOnReplace();
};

static QSearchableItem makeItem(const QString &id,
                                const QString &title = {},
                                const QString &domain = {})
{
    QSearchableItem item(id);
    if (!title.isEmpty()) item.setTitle(title);
    if (!domain.isEmpty()) item.setDomainIdentifier(domain);
    return item;
}

void TestSearchableItemStore::addAndContains()
{
    SearchableItemStore store;
    QVERIFY(!store.contains("id-1"));

    store.addItems({makeItem("id-1", "Alpha")});
    QVERIFY(store.contains("id-1"));
    QVERIFY(!store.contains("id-2"));
}

void TestSearchableItemStore::addReplacesExisting()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "Original")});
    QCOMPARE(store.item("id-1").title(), "Original");

    store.addItems({makeItem("id-1", "Updated")});
    QCOMPARE(store.item("id-1").title(), "Updated");
}

void TestSearchableItemStore::removeItems()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "A"), makeItem("id-2", "B"), makeItem("id-3", "C")});
    QVERIFY(store.contains("id-1"));
    QVERIFY(store.contains("id-2"));

    store.removeItems({"id-1", "id-2"});
    QVERIFY(!store.contains("id-1"));
    QVERIFY(!store.contains("id-2"));
    QVERIFY(store.contains("id-3"));
}

void TestSearchableItemStore::removeItemsNonexistent()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "A")});

    // Should not crash or affect existing items.
    store.removeItems({"nonexistent"});
    QVERIFY(store.contains("id-1"));
}

void TestSearchableItemStore::removeItemsInDomains()
{
    SearchableItemStore store;
    store.addItems({
        makeItem("id-1", "A", "animals"),
        makeItem("id-2", "B", "animals"),
        makeItem("id-3", "C", "plants"),
    });

    store.removeItemsInDomains({"animals"});
    QVERIFY(!store.contains("id-1"));
    QVERIFY(!store.contains("id-2"));
    QVERIFY(store.contains("id-3"));
}

void TestSearchableItemStore::removeItemsInDomainsMultiple()
{
    SearchableItemStore store;
    store.addItems({
        makeItem("id-1", "A", "dom-a"),
        makeItem("id-2", "B", "dom-b"),
        makeItem("id-3", "C", "dom-c"),
    });

    store.removeItemsInDomains({"dom-a", "dom-c"});
    QVERIFY(!store.contains("id-1"));
    QVERIFY(store.contains("id-2"));
    QVERIFY(!store.contains("id-3"));
}

void TestSearchableItemStore::removeAllItems()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "A"), makeItem("id-2", "B")});

    store.removeAllItems();
    QVERIFY(!store.contains("id-1"));
    QVERIFY(!store.contains("id-2"));
}

void TestSearchableItemStore::searchByTitle()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "Alpaca"), makeItem("id-2", "Pangolin")});

    QList<QSearchableItem> results = store.search({"Alpaca"});
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().uniqueIdentifier(), "id-1");
}

void TestSearchableItemStore::searchByContentDescription()
{
    SearchableItemStore store;
    QSearchableItem item("id-1");
    item.setTitle("Note");
    item.setContentDescription("Buy groceries tomorrow");
    store.addItems({item});

    QList<QSearchableItem> results = store.search({"groceries"});
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().uniqueIdentifier(), "id-1");
}

void TestSearchableItemStore::searchByDisplayName()
{
    SearchableItemStore store;
    QSearchableItem item("id-1");
    item.setTitle("Doc");
    item.setDisplayName("Annual Report 2026");
    store.addItems({item});

    QList<QSearchableItem> results = store.search({"Annual"});
    QCOMPARE(results.size(), 1);
}

void TestSearchableItemStore::searchByKeyword()
{
    SearchableItemStore store;
    QSearchableItem item("id-1");
    item.setTitle("Photo");
    item.setKeywords({"vacation", "beach"});
    store.addItems({item});

    QList<QSearchableItem> results = store.search({"beach"});
    QCOMPARE(results.size(), 1);

    // Title doesn't match, other keywords don't match.
    results = store.search({"mountain"});
    QCOMPARE(results.size(), 0);
}

void TestSearchableItemStore::searchCaseInsensitive()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "Alpaca")});

    QCOMPARE(store.search({"alpaca"}).size(), 1);
    QCOMPARE(store.search({"ALPACA"}).size(), 1);
    QCOMPARE(store.search({"aLpAcA"}).size(), 1);
}

void TestSearchableItemStore::searchMultipleTermsAndLogic()
{
    SearchableItemStore store;
    QSearchableItem item("id-1");
    item.setTitle("Fluffy Alpaca");
    item.setContentDescription("A very cute animal");
    store.addItems({item, makeItem("id-2", "Fluffy Cat")});

    // Both terms must match (AND logic). "Alpaca" is only in id-1.
    QList<QSearchableItem> results = store.search({"Fluffy", "Alpaca"});
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().uniqueIdentifier(), "id-1");

    // "Fluffy" matches both items.
    results = store.search({"Fluffy"});
    QCOMPARE(results.size(), 2);
}

void TestSearchableItemStore::searchEmptyTerms()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "Alpaca")});

    QList<QSearchableItem> results = store.search({});
    QCOMPARE(results.size(), 0);
}

void TestSearchableItemStore::searchNoMatch()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "Alpaca")});

    QList<QSearchableItem> results = store.search({"Zebra"});
    QCOMPARE(results.size(), 0);
}

void TestSearchableItemStore::itemLookup()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "Alpha", "dom-a")});

    QSearchableItem result = store.item("id-1");
    QCOMPARE(result.uniqueIdentifier(), "id-1");
    QCOMPARE(result.title(), "Alpha");
    QCOMPARE(result.domainIdentifier(), "dom-a");
}

void TestSearchableItemStore::domainTrackingOnReplace()
{
    SearchableItemStore store;
    store.addItems({makeItem("id-1", "A", "old-domain")});

    // Replace item with a new domain.
    store.addItems({makeItem("id-1", "A", "new-domain")});

    // Removing old domain should not affect the item.
    store.removeItemsInDomains({"old-domain"});
    QVERIFY(store.contains("id-1"));

    // Removing new domain should remove it.
    store.removeItemsInDomains({"new-domain"});
    QVERIFY(!store.contains("id-1"));
}

QTEST_MAIN(TestSearchableItemStore)
#include "tst_searchableitemstore.moc"
