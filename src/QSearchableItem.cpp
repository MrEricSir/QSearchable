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

#include "QSearchableItem.h"
#include <QSharedData>

class QSearchableItemData : public QSharedData
{
public:
    QString uniqueIdentifier;
    QString domainIdentifier;
    QString title;
    QString contentDescription;
    QString displayName;
    QStringList keywords;
    QByteArray thumbnailData;
    QString contentType;
    QUrl url;
    QDateTime timestamp;
};

QSearchableItem::QSearchableItem(const QString &uniqueIdentifier)
    : d(new QSearchableItemData)
{
    d->uniqueIdentifier = uniqueIdentifier;
}

QSearchableItem::QSearchableItem(const QSearchableItem &other) = default;

QSearchableItem &QSearchableItem::operator=(const QSearchableItem &other) = default;

QSearchableItem::~QSearchableItem() = default;

QString QSearchableItem::uniqueIdentifier() const
{
    return d->uniqueIdentifier;
}

void QSearchableItem::setDomainIdentifier(const QString &domainIdentifier)
{
    d->domainIdentifier = domainIdentifier;
}

QString QSearchableItem::domainIdentifier() const
{
    return d->domainIdentifier;
}

void QSearchableItem::setTitle(const QString &title)
{
    d->title = title;
}

QString QSearchableItem::title() const
{
    return d->title;
}

void QSearchableItem::setContentDescription(const QString &contentDescription)
{
    d->contentDescription = contentDescription;
}

QString QSearchableItem::contentDescription() const
{
    return d->contentDescription;
}

void QSearchableItem::setDisplayName(const QString &displayName)
{
    d->displayName = displayName;
}

QString QSearchableItem::displayName() const
{
    return d->displayName;
}

void QSearchableItem::setKeywords(const QStringList &keywords)
{
    d->keywords = keywords;
}

QStringList QSearchableItem::keywords() const
{
    return d->keywords;
}

void QSearchableItem::setThumbnailData(const QByteArray &thumbnailData)
{
    d->thumbnailData = thumbnailData;
}

QByteArray QSearchableItem::thumbnailData() const
{
    return d->thumbnailData;
}

void QSearchableItem::setContentType(const QString &contentType)
{
    d->contentType = contentType;
}

QString QSearchableItem::contentType() const
{
    return d->contentType;
}

void QSearchableItem::setUrl(const QUrl &url)
{
    d->url = url;
}

QUrl QSearchableItem::url() const
{
    return d->url;
}

void QSearchableItem::setTimestamp(const QDateTime &timestamp)
{
    d->timestamp = timestamp;
}

QDateTime QSearchableItem::timestamp() const
{
    return d->timestamp;
}
