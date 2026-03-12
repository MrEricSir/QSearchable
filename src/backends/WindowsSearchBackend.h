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

#ifndef WINDOWSSEARCHBACKEND_H
#define WINDOWSSEARCHBACKEND_H

#include "QSearchableIndexBackend.h"

#include <QDir>
#include <QIcon>
#include <QString>
#include <windows.h>

class WindowsSearchBackend : public QSearchableIndexBackend
{
    Q_OBJECT

public:
    explicit WindowsSearchBackend(QObject *parent = nullptr);
    ~WindowsSearchBackend() override;

    bool isSupported() const override;

    void indexItems(const QList<QSearchableItem> &items) override;
    void removeItems(const QStringList &identifiers) override;
    void removeItemsInDomains(const QStringList &domainIdentifiers) override;
    void removeAllItems() override;
    void uninstall() override;

private:
    QString domainDir(const QString &domain) const;
    QString itemFilePath(const QString &domain, const QSearchableItem &item) const;
    QString findFileForId(const QString &domain, const QString &id) const;
    void writeItemFile(const QString &filePath, const QSearchableItem &item);
    QString parseIdFromFile(const QString &filePath) const;
    QString sanitizeTitle(const QString &title) const;

    void registerCrawlScope();
    void registerFileType();
    void unregisterCrawlScope();
    void unregisterFileType();
    QString saveAppIcon();
    bool writeIcoFile(const QString &path, const QIcon &icon);

    void setupIpc();
    void checkPendingActivation();
    void handleActivation(const QString &filePath);

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    QString baseDir;
    QString fileExtension;
    QString progId;
    QString windowClassName;
    bool scopeRegistered = false;
    HWND ipcWindow = nullptr;
};

#endif // WINDOWSSEARCHBACKEND_H
