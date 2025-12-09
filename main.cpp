#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow w;

    // ÄNDERUNG: Statt w.show() nutzen wir w.showMaximized()
    w.showMaximized();

    return a.exec();
}
