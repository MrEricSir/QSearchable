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

#ifndef QSEARCHABLEINDEX_H
#define QSEARCHABLEINDEX_H

#include "QSearchableItem.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

// When running QDoc we have to ignore Q_OBJECT to avoid parsing Qt internals.
#ifdef Q_QDOC
#define Q_OBJECT
#endif

class QSearchableIndexBackend;

/*!
    \module QSearchable
    \title QSearchable C++ Classes
    \brief Integrates local desktop search with your cross-platform Qt app.
*/

/*!
    \class QSearchableIndex
    \inmodule QSearchable
    \brief The QSearchableIndex class indexes application content for
    the platform's native search provider.

    The following platforms are supported:

    \list
    \li \b{macOS}: Spotlight
    \li \b{Windows}: Windows Search
    \li \b{Linux (GNOME Shell)}: GNOME Tracker
    \li \b{Linux (KDE Plasma)}: KRunner
    \endlist

    On unsupported platforms a no-op backend is used and
    isSupported() returns \c false.

    QSearchableIndex is a singleton. Use Get() to obtain
    the instance.

    \section1 Example

    \code
    QSearchableItem item("contact-42");
    item.setTitle("Alice");
    item.setContentDescription("alice@example.com");

    QSearchableIndex::Get()->indexItems({item});
    \endcode

    \sa QSearchableItem
*/
class QSearchableIndex : public QObject
{
    Q_OBJECT

public:
    /*!
        Returns the singleton instance.
    */
    static QSearchableIndex *Get();

    /*!
        Returns \c true if the current platform's search backend is supported.
    */
    bool isSupported() const;

    /*!
        Returns \c true if this process was launched to relay a search-result
        activation to an already-running instance.

        When this returns \c true the application should skip showing its main
        window — the process will quit automatically after delivering the
        activation to the existing instance.
    */
    bool isRelayInstance() const;

    /*!
        Indexes (or updates) the given \a items. When the operation completes
        the indexingSucceeded() signal is emitted.
    */
    void indexItems(const QList<QSearchableItem> &items);

    /*!
        Removes items matching the given \a identifiers from the index.
    */
    void removeItems(const QStringList &identifiers);

    /*!
        Removes all items whose domain identifier is in
        \a domainIdentifiers.
    */
    void removeItemsInDomains(const QStringList &domainIdentifiers);

    /*!
        Removes every item from the index.
    */
    void removeAllItems();

    /*!
        Uninstalls all traces of the search index and related artifacts.
        Emits removalSucceeded() when complete.
    */
    void uninstall();

    /*!
        Returns \c true if system-level registration (e.g.\ the Windows
        Search property handler) has already been performed via install().

        On platforms that do not require separate installation this always
        returns \c true.
    */
    bool isInstalled() const;

    /*!
        Returns the command-line arguments for QSearchableInstaller.exe
        to register the property handler and file type.

        Use this to integrate registration into your MSI, NSIS, or Inno
        Setup installer. The returned list is suitable for passing directly
        to QProcess or similar. For example:

        \code
        QStringList args = QSearchableIndex::Get()->installerArguments();
        // args: ["install", "<ext>", "<clsid>", "<progid>", "<appname>", "<apppath>", "<dllpath>"]
        \endcode

        On platforms that do not require separate installation this
        returns an empty list.

        \sa uninstallerArguments(), install()
    */
    QStringList installerArguments() const;

    /*!
        Returns the command-line arguments for QSearchableInstaller.exe
        to remove the property handler and file type registration.

        \sa installerArguments(), uninstall()
    */
    QStringList uninstallerArguments() const;

    /*!
        Performs one-time system-level registration required for full search
        integration (e.g.\ the Windows Search property handler).

        On Windows this launches QSearchableInstaller.exe with elevation,
        which triggers a UAC prompt. Call this explicitly from your
        application's first-run or settings UI — it is \b not called
        automatically by indexItems().

        Alternatively, use installerArguments() to get the arguments and
        run QSearchableInstaller.exe from your system installer (MSI,
        NSIS, etc.) which already has admin privileges.

        On other platforms this is a no-op.
    */
    void install();

signals:
    /*!
        Emitted after indexItems() completes successfully.
        \a count is the number of items that were indexed.
    */
    void indexingSucceeded(int count);

    /*!
        Emitted after a removal operation completes successfully.
    */
    void removalSucceeded();

    /*!
        Emitted when an indexing or removal operation fails.
        \a errorMessage contains a human-readable description.
    */
    void errorOccurred(const QString &errorMessage);

    /*!
        Emitted when the user activates a search result.
        \a uniqueIdentifier is the identifier of the activated item.
    */
    void activated(const QString &uniqueIdentifier);

private:

    explicit QSearchableIndex(QObject *parent = nullptr);
    ~QSearchableIndex() override;
    QSearchableIndexBackend *backend;
};

#endif // QSEARCHABLEINDEX_H
