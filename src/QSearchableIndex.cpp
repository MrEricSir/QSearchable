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

#include "backends/QSearchableIndexBackend.h"

#ifdef Q_OS_MACOS
#include "backends/CoreSpotlightBackend.h"
#elif defined(Q_OS_WIN)
#include "backends/WindowsSearchBackend.h"
#elif defined(Q_OS_LINUX)
#include "backends/GnomeSearchBackend.h"
#include "backends/KRunnerBackend.h"
#endif

#include "backends/NoOpBackend.h"

QSearchableIndex *QSearchableIndex::Get()
{
    static QSearchableIndex instance;
    return &instance;
}

QSearchableIndex::QSearchableIndex(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_MACOS
    backend = new CoreSpotlightBackend(this);
#elif defined(Q_OS_WIN)
    backend = new WindowsSearchBackend(this);
#elif defined(Q_OS_LINUX)
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    if (desktop.contains("KDE", Qt::CaseInsensitive))
        backend = new KRunnerBackend(this);
    else if (desktop.contains("GNOME", Qt::CaseInsensitive)
             || desktop.contains("Unity", Qt::CaseInsensitive)
             || desktop.contains("Cinnamon", Qt::CaseInsensitive))
        backend = new GnomeSearchBackend(this);
    else
        backend = new NoOpBackend(this);
#else
    backend = new NoOpBackend(this);
#endif

    connect(backend, &QSearchableIndexBackend::indexingSucceeded,
            this, &QSearchableIndex::indexingSucceeded);
    connect(backend, &QSearchableIndexBackend::removalSucceeded,
            this, &QSearchableIndex::removalSucceeded);
    connect(backend, &QSearchableIndexBackend::errorOccurred,
            this, &QSearchableIndex::errorOccurred);
    connect(backend, &QSearchableIndexBackend::activated,
            this, &QSearchableIndex::activated);
}

QSearchableIndex::~QSearchableIndex() = default;

bool QSearchableIndex::isSupported() const
{
    return backend->isSupported();
}

bool QSearchableIndex::isRelayInstance() const
{
    return backend->isRelayInstance();
}

void QSearchableIndex::indexItems(const QList<QSearchableItem> &items)
{
    backend->indexItems(items);
}

void QSearchableIndex::removeItems(const QStringList &identifiers)
{
    backend->removeItems(identifiers);
}

void QSearchableIndex::removeItemsInDomains(const QStringList &domainIdentifiers)
{
    backend->removeItemsInDomains(domainIdentifiers);
}

void QSearchableIndex::removeAllItems()
{
    backend->removeAllItems();
}

bool QSearchableIndex::isInstalled() const
{
    return backend->isInstalled();
}

