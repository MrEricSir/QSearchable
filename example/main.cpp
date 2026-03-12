#include <QApplication>
#include <QIcon>
#include "ListDemoWindow.h"
#include "QSearchableIndex.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QSearchableListDemo");
    app.setOrganizationDomain("com.EricGregory.QSearchableListDemo");
    app.setWindowIcon(QIcon(":/icon_256.png"));

    // If launched to relay a search-result activation to an existing
    // instance, skip showing UI — the process will quit on its own.
    if (QSearchableIndex::Get()->isRelayInstance()) {
        return app.exec();
    }

    ListDemoWindow window;
    window.show();

    return app.exec();
}
