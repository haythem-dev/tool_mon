#include <QtWidgets/QApplication>
#include "FileMonitorWidget.h"
#include <QtCore/QCoreApplication> // Add this include

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("YourOrganization");
    QCoreApplication::setApplicationName("FileMon");

    QApplication app(argc, argv);
    FileMonitorWidget w;
    w.show();
    return app.exec();
}