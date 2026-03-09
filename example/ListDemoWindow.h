#ifndef LISTDEMOWINDOW_H
#define LISTDEMOWINDOW_H

#include <QWidget>
#include <QMap>

class QLabel;
class QLineEdit;
class QVBoxLayout;

class ListDemoWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ListDemoWindow(QWidget *parent = nullptr);

private slots:
    void addItem(const QString &text = QString());
    void removeItem(int id);
    void onEditingFinished(int id);
    void onIndexingSucceeded(int count);
    void onErrorOccurred(const QString &errorMessage);
    void onActivated(const QString &uniqueIdentifier);

private:
    void indexItem(int id, const QString &text);
    void indexAllItems();
    void clearHighlight();

    QVBoxLayout *rowLayout;
    QLabel *statusLabel;
    QMap<int, QLineEdit *> items;
    QLineEdit *highlightedItem = nullptr;
    int nextId = 0;
};

#endif // LISTDEMOWINDOW_H
