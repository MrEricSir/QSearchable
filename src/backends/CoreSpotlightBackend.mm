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

#include "CoreSpotlightBackend.h"

#include <QMetaObject>

#import <CoreSpotlight/CoreSpotlight.h>

static NSString *toNSString(const QString &string)
{
    return string.toNSString();
}

static NSArray<NSString *> *toNSStringArray(const QStringList &list)
{
    NSMutableArray<NSString *> *array = [NSMutableArray arrayWithCapacity:list.size()];
    for (const QString &string : list) {
        [array addObject:toNSString(string)];
    }
    return array;
}

CoreSpotlightBackend::CoreSpotlightBackend(QObject *parent)
    : QSearchableIndexBackend(parent)
    , index(nullptr)
    , observer(nullptr)
{
    index = (void *)[[CSSearchableIndex defaultSearchableIndex] retain];

    // Observe search result activation via NSUserActivity
    observer = (void *)[[NSNotificationCenter defaultCenter]
        addObserverForName:CSSearchableItemActionType
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification *notification) {
                    NSUserActivity *activity = notification.object;
                    if ([activity isKindOfClass:[NSUserActivity class]]) {
                        NSString *identifier = activity.userInfo[CSSearchableItemActivityIdentifier];
                        if (identifier) {
                            QString qtIdentifier = QString::fromNSString(identifier);
                            QMetaObject::invokeMethod(this, [this, qtIdentifier]() {
                                emit activated(qtIdentifier);
                            }, Qt::QueuedConnection);
                        }
                    }
                }];
}

CoreSpotlightBackend::~CoreSpotlightBackend()
{
    if (observer) {
        [[NSNotificationCenter defaultCenter] removeObserver:(id)observer];
        observer = nullptr;
    }
    if (index) {
        [(id)index release];
        index = nullptr;
    }
}

bool CoreSpotlightBackend::isSupported() const
{
    return true;
}

void CoreSpotlightBackend::indexItems(const QList<QSearchableItem> &items)
{
    NSMutableArray<CSSearchableItem *> *csItems =
        [NSMutableArray arrayWithCapacity:items.size()];

    for (const QSearchableItem &item : items) {
        CSSearchableItemAttributeSet *attributes =
            [[CSSearchableItemAttributeSet alloc]
                initWithContentType:UTTypeData];

        if (!item.title().isEmpty()) {
            attributes.title = toNSString(item.title());
        }
        if (!item.contentDescription().isEmpty()) {
            attributes.contentDescription = toNSString(item.contentDescription());
        }
        if (!item.displayName().isEmpty()) {
            attributes.displayName = toNSString(item.displayName());
        }
        if (!item.keywords().isEmpty()) {
            attributes.keywords = toNSStringArray(item.keywords());
        }
        if (!item.thumbnailData().isEmpty()) {
            attributes.thumbnailData =
                [NSData dataWithBytes:item.thumbnailData().constData()
                               length:item.thumbnailData().size()];
        }
        if (!item.contentType().isEmpty()) {
            attributes.contentType = toNSString(item.contentType());
        }
        if (item.url().isValid()) {
            attributes.contentURL = item.url().toNSURL();
        }
        if (item.timestamp().isValid()) {
            attributes.contentModificationDate =
                [NSDate dateWithTimeIntervalSince1970:
                    item.timestamp().toSecsSinceEpoch()];
        }

        CSSearchableItem *csItem =
            [[CSSearchableItem alloc]
                initWithUniqueIdentifier:toNSString(item.uniqueIdentifier())
                        domainIdentifier:item.domainIdentifier().isEmpty()
                                             ? nil
                                             : toNSString(item.domainIdentifier())
                            attributeSet:attributes];

        [csItems addObject:csItem];
    }

    CSSearchableIndex *csIndex = (CSSearchableIndex *)index;
    const int count = items.size();

    [csIndex indexSearchableItems:csItems
              completionHandler:^(NSError *error) {
                  if (error) {
                      QString errorMsg = QString::fromNSString(error.localizedDescription);
                      QMetaObject::invokeMethod(this, [this, errorMsg]() {
                          emit errorOccurred(errorMsg);
                      }, Qt::QueuedConnection);
                  } else {
                      QMetaObject::invokeMethod(this, [this, count]() {
                          emit indexingSucceeded(count);
                      }, Qt::QueuedConnection);
                  }
              }];
}

void CoreSpotlightBackend::removeItems(const QStringList &identifiers)
{
    CSSearchableIndex *csIndex = (CSSearchableIndex *)index;
    NSArray<NSString *> *nsIdentifiers = toNSStringArray(identifiers);

    [csIndex deleteSearchableItemsWithIdentifiers:nsIdentifiers
                              completionHandler:^(NSError *error) {
                                  if (error) {
                                      QString errorMsg = QString::fromNSString(error.localizedDescription);
                                      QMetaObject::invokeMethod(this, [this, errorMsg]() {
                                          emit errorOccurred(errorMsg);
                                      }, Qt::QueuedConnection);
                                  } else {
                                      QMetaObject::invokeMethod(this, [this]() {
                                          emit removalSucceeded();
                                      }, Qt::QueuedConnection);
                                  }
                              }];
}

void CoreSpotlightBackend::removeItemsInDomains(const QStringList &domainIdentifiers)
{
    CSSearchableIndex *csIndex = (CSSearchableIndex *)index;
    NSArray<NSString *> *nsDomains = toNSStringArray(domainIdentifiers);

    [csIndex deleteSearchableItemsWithDomainIdentifiers:nsDomains
                                    completionHandler:^(NSError *error) {
                                        if (error) {
                                            QString errorMsg = QString::fromNSString(error.localizedDescription);
                                            QMetaObject::invokeMethod(this, [this, errorMsg]() {
                                                emit errorOccurred(errorMsg);
                                            }, Qt::QueuedConnection);
                                        } else {
                                            QMetaObject::invokeMethod(this, [this]() {
                                                emit removalSucceeded();
                                            }, Qt::QueuedConnection);
                                        }
                                    }];
}

void CoreSpotlightBackend::removeAllItems()
{
    CSSearchableIndex *csIndex = (CSSearchableIndex *)index;

    [csIndex deleteAllSearchableItemsWithCompletionHandler:^(NSError *error) {
        if (error) {
            QString errorMsg = QString::fromNSString(error.localizedDescription);
            QMetaObject::invokeMethod(this, [this, errorMsg]() {
                emit errorOccurred(errorMsg);
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                emit removalSucceeded();
            }, Qt::QueuedConnection);
        }
    }];
}

void CoreSpotlightBackend::uninstall()
{
    removeAllItems();
}
