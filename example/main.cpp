#include <QApplication>
#include "ListDemoWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QSearchableListDemo");
    app.setOrganizationDomain("com.EricGregory.QSearchableListDemo");

    ListDemoWindow window;
    window.show();

    return app.exec();
}
