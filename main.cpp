#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow w;

    // Auf Android startet Qt Apps automatisch im Vollbild,
    // auf Windows zeigen wir es als normales Fenster.
    w.show();

    return a.exec();
}
