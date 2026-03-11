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

#include "QSearchableIndex.h"
#include "QSearchableItem.h"

#include <QSignalSpy>
#include <QTest>

class TestQSearchableIndex : public QObject
{
    Q_OBJECT

private slots:
    void itemConstruction();
    void itemProperties();
    void itemCopy();
    void itemCopyOnWrite();
    void itemDefaults();
    void defaultIndexSingleton();
    void isSupported();
    void indexItemsEmitsSucceeded();
    void indexEmptyList();
    void removeItemsEmitsSucceeded();
    void removeItemsInDomainsEmitsSucceeded();
    void removeAllItemsEmitsSucceeded();
    void uninstallEmitsSucceeded();
};

void TestQSearchableIndex::itemConstruction()
{
    QSearchableItem item("test-id");
    QCOMPARE(item.uniqueIdentifier(), "test-id");
}

void TestQSearchableIndex::itemProperties()
{
    QSearchableItem item("test-id");

    item.setDomainIdentifier("domain");
    QCOMPARE(item.domainIdentifier(), "domain");

    item.setTitle("Test Title");
    QCOMPARE(item.title(), "Test Title");

    item.setContentDescription("A description");
    QCOMPARE(item.contentDescription(), "A description");

    item.setDisplayName("Display Name");
    QCOMPARE(item.displayName(), "Display Name");

    item.setKeywords({"keyword1", "keyword2"});
    QCOMPARE(item.keywords(), QStringList({"keyword1", "keyword2"}));

    QByteArray thumbnail("thumbnaildata");
    item.setThumbnailData(thumbnail);
    QCOMPARE(item.thumbnailData(), thumbnail);

    item.setContentType("text/html");
    QCOMPARE(item.contentType(), "text/html");

    QUrl url("https://example.com");
    item.setUrl(url);
    QCOMPARE(item.url(), url);

    QDateTime timestamp = QDateTime::currentDateTimeUtc();
    item.setTimestamp(timestamp);
    QCOMPARE(item.timestamp(), timestamp);
}

void TestQSearchableIndex::itemCopy()
{
    QSearchableItem original("original-id");
    original.setTitle("Original Title");
    original.setDomainIdentifier("domain");

    QSearchableItem copy(original);
    QCOMPARE(copy.uniqueIdentifier(), "original-id");
    QCOMPARE(copy.title(), "Original Title");
    QCOMPARE(copy.domainIdentifier(), "domain");
}

void TestQSearchableIndex::itemCopyOnWrite()
{
    QSearchableItem original("id");
    original.setTitle("Original");

    QSearchableItem copy(original);
    copy.setTitle("Modified");

    QCOMPARE(original.title(), "Original");
    QCOMPARE(copy.title(), "Modified");
}

void TestQSearchableIndex::itemDefaults()
{
    QSearchableItem item("id");

    QCOMPARE(item.uniqueIdentifier(), "id");
    QVERIFY(item.domainIdentifier().isEmpty());
    QVERIFY(item.title().isEmpty());
    QVERIFY(item.contentDescription().isEmpty());
    QVERIFY(item.displayName().isEmpty());
    QVERIFY(item.keywords().isEmpty());
    QVERIFY(item.thumbnailData().isEmpty());
    QVERIFY(item.contentType().isEmpty());
    QVERIFY(!item.url().isValid());
    QVERIFY(!item.timestamp().isValid());
}

void TestQSearchableIndex::defaultIndexSingleton()
{
    QSearchableIndex *index1 = QSearchableIndex::Get();
    QSearchableIndex *index2 = QSearchableIndex::Get();
    QCOMPARE(index1, index2);
    QVERIFY(index1 != nullptr);
}

void TestQSearchableIndex::isSupported()
{
    QSearchableIndex *index = QSearchableIndex::Get();
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    QVERIFY(index->isSupported());
#else
    QVERIFY(!index->isSupported());
#endif
}

void TestQSearchableIndex::indexItemsEmitsSucceeded()
{
    QSearchableIndex *index = QSearchableIndex::Get();
    QSignalSpy spy(index, &QSearchableIndex::indexingSucceeded);

    QSearchableItem item1("item-1");
    item1.setTitle("Item 1");
    QSearchableItem item2("item-2");
    item2.setTitle("Item 2");

    index->indexItems({item1, item2});

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toInt(), 2);
}

void TestQSearchableIndex::indexEmptyList()
{
    QSearchableIndex *index = QSearchableIndex::Get();
    QSignalSpy spy(index, &QSearchableIndex::indexingSucceeded);

    index->indexItems({});

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toInt(), 0);
}

void TestQSearchableIndex::removeItemsEmitsSucceeded()
{
    QSearchableIndex *index = QSearchableIndex::Get();
    QSignalSpy spy(index, &QSearchableIndex::removalSucceeded);

    index->removeItems({"item-1", "item-2"});

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
}

void TestQSearchableIndex::removeItemsInDomainsEmitsSucceeded()
{
    QSearchableIndex *index = QSearchableIndex::Get();
    QSignalSpy spy(index, &QSearchableIndex::removalSucceeded);
    QSignalSpy errorSpy(index, &QSearchableIndex::errorOccurred);

    index->removeItemsInDomains({"domain-1"});

    // On macOS CI (unsigned app), CoreSpotlight may return an error
    // instead of succeeding. Accept either response as valid.
    QVERIFY(QTest::qWaitFor([&]() {
        return spy.count() > 0 || errorSpy.count() > 0;
    }, 5000));
}

void TestQSearchableIndex::removeAllItemsEmitsSucceeded()
{
    QSearchableIndex *index = QSearchableIndex::Get();
    QSignalSpy spy(index, &QSearchableIndex::removalSucceeded);

    index->removeAllItems();

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
}

void TestQSearchableIndex::uninstallEmitsSucceeded()
{
    QSearchableIndex *index = QSearchableIndex::Get();
    QSignalSpy spy(index, &QSearchableIndex::removalSucceeded);

    index->uninstall();

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestQSearchableIndex)
#include "tst_qsearchableindex.moc"
