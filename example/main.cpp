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

    // This is used for search activation. We let the event loop handle the messaging
    // and immediately exit.
    if (QSearchableIndex::Get()->isRelayInstance()) {
        return app.exec();
    }

    ListDemoWindow window;
    window.show();

    return app.exec();
}
