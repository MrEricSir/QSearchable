#include "ListDemoWindow.h"

#include "QSearchableIndex.h"
#include "QSearchableItem.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

ListDemoWindow::ListDemoWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("QSearchable List Demo");
    resize(400, 350);

    auto *mainLayout = new QVBoxLayout(this);

    // Windows: Install / Uninstall buttons.
    // In a real application, registration would be handled by the installer (MSI,
    // NSIS, etc.) but we include this here for demonstration purposes.
#ifdef Q_OS_WIN
    auto *installBar = new QHBoxLayout;
    installButton = new QPushButton("Install");
    uninstallButton = new QPushButton("Uninstall");
    installBar->addWidget(installButton);
    installBar->addWidget(uninstallButton);
    mainLayout->addLayout(installBar);

    connect(installButton, &QPushButton::clicked, this, [this]() {
        statusLabel->setText("Installing…");
        if (!runInstaller("install")) {
            statusLabel->setText("Install cancelled or failed");
            return;
        }
        updateInstallButtons();
        statusLabel->setText(QSearchableIndex::Get()->isInstalled()
                             ? "Installed" : "Install failed");
    });
    connect(uninstallButton, &QPushButton::clicked, this, [this]() {
        statusLabel->setText("Uninstalling…");
        if (!runInstaller("uninstall")) {
            statusLabel->setText("Uninstall cancelled or failed");
            return;
        }
        QSearchableIndex::Get()->removeAllItems();
        updateInstallButtons();
        statusLabel->setText(!QSearchableIndex::Get()->isInstalled()
                             ? "Uninstalled" : "Uninstall failed");
    });
#endif

    // Description label.
    auto *description = new QLabel(
        "Items in this list are indexed for your platform's desktop search: "
#ifdef Q_OS_MACOS
        "Spotlight"
#elif defined(Q_OS_WIN)
        "Windows Search"
#elif defined(Q_OS_LINUX)
        "GNOME Shell / KRunner"
#endif
        ". Edit the list and they will appear in search results in real time."
    );
    description->setWordWrap(true);
    mainLayout->addWidget(description);

    rowLayout = new QVBoxLayout;
    mainLayout->addLayout(rowLayout);

    mainLayout->addStretch();

    auto *bottomBar = new QHBoxLayout;
    auto *addButton = new QPushButton("+");
    statusLabel = new QLabel("Ready");
    bottomBar->addWidget(addButton);
    bottomBar->addWidget(statusLabel, 1);
    mainLayout->addLayout(bottomBar);

    connect(addButton, &QPushButton::clicked, this, [this]() { addItem(); saveItems(); });

    auto *index = QSearchableIndex::Get();
    connect(index, &QSearchableIndex::indexingSucceeded,
            this, &ListDemoWindow::onIndexingSucceeded);
    connect(index, &QSearchableIndex::errorOccurred,
            this, &ListDemoWindow::onErrorOccurred);
    connect(index, &QSearchableIndex::activated,
            this, &ListDemoWindow::onActivated);

    updateInstallButtons();

    // Restore items from the previous session. If none, or pre-populate with samples.
    loadItems();

    indexAllItems();
}

void ListDemoWindow::addItem(const QString &text)
{
    int id = nextId++;

    auto *row = new QHBoxLayout;
    auto *lineEdit = new QLineEdit(text);
    auto *removeButton = new QPushButton("-");

    row->addWidget(lineEdit);
    row->addWidget(removeButton);
    rowLayout->addLayout(row);

    items.insert(id, lineEdit);

    connect(lineEdit, &QLineEdit::editingFinished, this, [this, id]() {
        onEditingFinished(id);
        saveItems();
    });

    connect(lineEdit, &QLineEdit::textEdited, this, [this, lineEdit]() {
        if (lineEdit == highlightedItem) {
            clearHighlight();
        }
    });

    connect(removeButton, &QPushButton::clicked, this, [this, id, row]() {
        removeItem(id);
        // Clean up rows.
        while (row->count() > 0) {
            auto *item = row->takeAt(0);
            delete item->widget();
            delete item;
        }
        rowLayout->removeItem(row);
        delete row;
    });

    lineEdit->setFocus();
}

void ListDemoWindow::removeItem(int id)
{
    if (items.value(id) == highlightedItem) {
        clearHighlight();
    }

    items.remove(id);
    saveItems();

    QString identifier = QStringLiteral("test-") + QString::number(id);
    QSearchableIndex::Get()->removeItems({identifier});
    statusLabel->setText(QString("Removed item %1").arg(id));
}

void ListDemoWindow::onEditingFinished(int id)
{
    auto it = items.find(id);
    if (it == items.end()) {
        return;
    }

    indexItem(id, it.value()->text());
}

void ListDemoWindow::indexItem(int id, const QString &text)
{
    QSearchableItem item(QStringLiteral("test-") + QString::number(id));
    item.setDomainIdentifier(QStringLiteral("testharness"));
    item.setTitle(text);

    QSearchableIndex::Get()->indexItems({item});
}

void ListDemoWindow::indexAllItems()
{
    QList<QSearchableItem> items;

    for (auto it = this->items.constBegin(); it != this->items.constEnd(); ++it) {
        QSearchableItem item(QStringLiteral("test-") + QString::number(it.key()));
        item.setDomainIdentifier(QStringLiteral("testharness"));
        item.setTitle(it.value()->text());
        items.append(item);
    }

    if (!items.isEmpty()) {
        QSearchableIndex::Get()->indexItems(items);
    }
}

void ListDemoWindow::onIndexingSucceeded(int count)
{
    statusLabel->setText(QString("Indexed %1 item(s)").arg(count));
}

void ListDemoWindow::onErrorOccurred(const QString &errorMessage)
{
    statusLabel->setText(QString("Error: %1").arg(errorMessage));
}

void ListDemoWindow::onActivated(const QString &uniqueIdentifier)
{
    if (!uniqueIdentifier.startsWith(QLatin1String("test-"))) {
        return;
    }

    bool ok;
    int id = uniqueIdentifier.mid(5).toInt(&ok);
    if (!ok) {
        return;
    }

    auto it = items.find(id);
    if (it == items.end()) {
        return;
    }

    clearHighlight();

    QLineEdit *lineEdit = it.value();
    lineEdit->setStyleSheet(QStringLiteral("QLineEdit { background-color: #FFEB3B; }"));
    highlightedItem = lineEdit;

    // Bring window to front.
    showNormal();
    raise();
    activateWindow();

    statusLabel->setText(QString("Activated: %1").arg(lineEdit->text()));
}

void ListDemoWindow::clearHighlight()
{
    if (highlightedItem) {
        highlightedItem->setStyleSheet(QString());
        highlightedItem = nullptr;
    }
}

void ListDemoWindow::saveItems()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("ListDemo"));
    settings.remove(QString()); // clear the group

    QStringList entries;
    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        entries.append(QStringLiteral("%1=%2").arg(it.key()).arg(it.value()->text()));
    }

    settings.setValue(QStringLiteral("items"), entries);
    settings.setValue(QStringLiteral("nextId"), nextId);
    settings.endGroup();
}

void ListDemoWindow::loadItems()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("ListDemo"));

    QStringList entries = settings.value(QStringLiteral("items")).toStringList();
    nextId = settings.value(QStringLiteral("nextId"), 0).toInt();
    settings.endGroup();

    if (entries.isEmpty()) {
        // Pre-populate with sample list.
        addItem(QStringLiteral("Alpaca Nebula"));
        addItem(QStringLiteral("Pangolin Planet"));
        addItem(QStringLiteral("Axolotl Worlds"));
        saveItems();
        return;
    }

    for (const QString &entry : entries) {
        int eq = entry.indexOf(QLatin1Char('='));
        if (eq < 0) continue;

        int id = entry.left(eq).toInt();
        QString text = entry.mid(eq + 1);

        auto *row = new QHBoxLayout;
        auto *lineEdit = new QLineEdit(text);
        auto *removeButton = new QPushButton(QStringLiteral("-"));

        row->addWidget(lineEdit);
        row->addWidget(removeButton);
        rowLayout->addLayout(row);

        items.insert(id, lineEdit);
        if (id >= nextId) nextId = id + 1;

        connect(lineEdit, &QLineEdit::editingFinished, this, [this, id]() {
            onEditingFinished(id);
            saveItems();
        });

        connect(lineEdit, &QLineEdit::textEdited, this, [this, lineEdit]() {
            if (lineEdit == highlightedItem) {
                clearHighlight();
            }
        });

        connect(removeButton, &QPushButton::clicked, this, [this, id, row]() {
            removeItem(id);
            while (row->count() > 0) {
                auto *item = row->takeAt(0);
                delete item->widget();
                delete item;
            }
            rowLayout->removeItem(row);
            delete row;
        });
    }
}

void ListDemoWindow::updateInstallButtons()
{
#ifdef Q_OS_WIN
    bool installed = QSearchableIndex::Get()->isInstalled();
    installButton->setEnabled(!installed);
    uninstallButton->setEnabled(installed);
#endif
}

bool ListDemoWindow::runInstaller(const QString &mode)
{
#ifdef Q_OS_WIN
    QString appDir = QCoreApplication::applicationDirPath();
    QString installerPath = QDir::toNativeSeparators(
        appDir + QStringLiteral("/QSearchableWindowsInstaller.exe"));
    QString exePath = QDir::toNativeSeparators(
        QCoreApplication::applicationFilePath());

    if (!QFile::exists(installerPath)) {
        statusLabel->setText(QStringLiteral("Error: installer not found at %1").arg(installerPath));
        return false;
    }

    QString params = QStringLiteral("\"%1\" \"%2\"").arg(mode, exePath);

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<const wchar_t *>(installerPath.utf16());
    sei.lpParameters = reinterpret_cast<const wchar_t *>(params.utf16());
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        statusLabel->setText(QStringLiteral("Error: ShellExecuteExW failed (%1)")
                             .arg(GetLastError()));
        return false;
    }

    if (sei.hProcess) {
        DWORD waitResult = WaitForSingleObject(sei.hProcess, 30000);
        DWORD exitCode = 1;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);

        if (waitResult == WAIT_TIMEOUT) {
            statusLabel->setText("Error: installer timed out");
            return false;
        }
        if (exitCode != 0) {
            statusLabel->setText(QStringLiteral("Error: installer exited with code %1")
                                 .arg(exitCode));
            return false;
        }
        return true;
    }

    statusLabel->setText("Error: no process handle returned");
    return false;
#else
    Q_UNUSED(mode);
    return true;
#endif
}
