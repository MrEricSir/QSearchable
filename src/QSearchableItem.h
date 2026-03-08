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

#ifndef QSEARCHABLEITEM_H
#define QSEARCHABLEITEM_H

#include <QByteArray>
#include <QDateTime>
#include <QSharedDataPointer>
#include <QString>
#include <QStringList>
#include <QUrl>

class QSearchableItemData;

/*!
    \class QSearchableItem
    \inmodule QSearchable
    \brief The QSearchableItem class describes an item to be indexed for
    platform search.

    Each item has a mandatory \l uniqueIdentifier and optional metadata
    such as \l title, \l contentDescription, \l keywords, and more.
    Items can be grouped by \l domainIdentifier so they can be removed
    as a batch with QSearchableIndex::removeItemsInDomains().

    QSearchableItem uses implicit sharing, so copies are inexpensive.

    \sa QSearchableIndex
*/
class QSearchableItem
{
public:
    /*!
        Constructs an item with the given \a uniqueIdentifier.

        The identifier must be unique within the search index and is used
        to update or remove the item later.
    */
    explicit QSearchableItem(const QString &uniqueIdentifier);
    QSearchableItem(const QSearchableItem &other);
    QSearchableItem &operator=(const QSearchableItem &other);
    ~QSearchableItem();

    /*!
        Returns the unique identifier for this item.
    */
    QString uniqueIdentifier() const;

    /*!
        Sets the \a domainIdentifier used to group related items.
    */
    void setDomainIdentifier(const QString &domainIdentifier);

    /*!
        Returns the domain identifier, or an empty string if none was set.
    */
    QString domainIdentifier() const;

    /*!
        Sets the \a title displayed in search results.
    */
    void setTitle(const QString &title);

    /*!
        Returns the title.
    */
    QString title() const;

    /*!
        Sets the \a contentDescription shown as secondary text in search results.
    */
    void setContentDescription(const QString &contentDescription);

    /*!
        Returns the content description.
    */
    QString contentDescription() const;

    /*!
        Sets the \a displayName for the item.
    */
    void setDisplayName(const QString &displayName);

    /*!
        Returns the display name.
    */
    QString displayName() const;

    /*!
        Sets the \a keywords used for matching search queries.
    */
    void setKeywords(const QStringList &keywords);

    /*!
        Returns the keywords.
    */
    QStringList keywords() const;

    /*!
        Sets the \a thumbnailData (e.g. PNG or JPEG bytes) for the item.
    */
    void setThumbnailData(const QByteArray &thumbnailData);

    /*!
        Returns the thumbnail data.
    */
    QByteArray thumbnailData() const;

    /*!
        Sets the MIME \a contentType of the item.
    */
    void setContentType(const QString &contentType);

    /*!
        Returns the content type.
    */
    QString contentType() const;

    /*!
        Sets a \a url associated with the item.
    */
    void setUrl(const QUrl &url);

    /*!
        Returns the URL.
    */
    QUrl url() const;

    /*!
        Sets the \a timestamp for the item's content.
    */
    void setTimestamp(const QDateTime &timestamp);

    /*!
        Returns the timestamp.
    */
    QDateTime timestamp() const;

private:
    QSharedDataPointer<QSearchableItemData> d;
};

#endif // QSEARCHABLEITEM_H
