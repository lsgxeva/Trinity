#include "window.h"
#include <QApplication>

#include "mocca/net/ConnectionFactorySelector.h"
#include "mocca/log/ConsoleLog.h"
#include "mocca/log/LogManager.h"

using namespace mocca::net;

void init() {
    using mocca::LogManager;
    LogManager::initialize(LogManager::LogLevel::Debug, true);
    auto console = new mocca::ConsoleLog();
    LogMgr.addLog(console);
    ConnectionFactorySelector::addDefaultFactories();
}

int main(int argc, char *argv[])
{
    init();

    QApplication a(argc, argv);
    Window w;
    w.show();

    return a.exec();
}