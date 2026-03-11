#include <QApplication>
#include <QIcon>
#include "ListDemoWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QSearchableListDemo");
    app.setOrganizationDomain("com.EricGregory.QSearchableListDemo");
    app.setWindowIcon(QIcon(":/icon_256.png"));

    ListDemoWindow window;
    window.show();

    return app.exec();
}
