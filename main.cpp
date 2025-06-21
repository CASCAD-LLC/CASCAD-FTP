#include "mainwindow.h"
#include <QApplication>
#include <csignal>
#include <QTimer>

static void handleSignal(int sig) {
    qCritical() << "Received signal:" << sig;
    exit(1);
}

int main(int argc, char *argv[])
{
    signal(SIGSEGV, handleSignal);
    signal(SIGABRT, handleSignal);

    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("CASCAD FTP");
    w.show();
    return a.exec();
}
