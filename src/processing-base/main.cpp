#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

#include "mocca/base/CommandLineParser.h"
#include "mocca/base/ContainerTools.h"
#include "mocca/log/ConsoleLog.h"
#include "mocca/log/HTMLLog.h"
#include "mocca/log/LogManager.h"
#include "mocca/net/ConnectionFactorySelector.h"
#include "mocca/net/Endpoint.h"

#include "processing-base/ProcessingNode.h"

#include <silverbullet/base/DetectEnv.h>


using namespace trinity;
using namespace mocca::net;

std::atomic<bool> exitFlag{false};

void exitHandler(int s) {
    std::cout << "Trinity exit on signal " << std::to_string(s) << std::endl;
    exitFlag = true;
}

mocca::CommandLineParser::Option TcpPortOption(int16_t& port) {
    mocca::CommandLineParser::Option option;
    option.key = "--TcpPort";
    option.help = "TCP port (default: 8678)";
    option.callback = [&](const std::string& value) { port = std::stoi(value); };
    return option;
}

mocca::CommandLineParser::Option WsPortOption(int16_t& port) {
    mocca::CommandLineParser::Option option;
    option.key = "--WsPort";
    option.help = "WebSocket port (default: 8679)";
    option.callback = [&](const std::string& value) { port = std::stoi(value); };
    return option;
}

void init() {
    using mocca::LogManager;
    LogManager::initialize(LogManager::LogLevel::Debug, true);
    auto console = new mocca::ConsoleLog();
    LogMgr.addLog(console);
#ifdef DETECTED_OS_LINUX
    auto web = new mocca::HTMLLog("processinglog.html");
    LogMgr.addLog(web);
#endif
    signal(SIGINT, exitHandler);
    ConnectionFactorySelector::addDefaultFactories();
}

int main(int argc, const char** argv) {
    init();

    int16_t TcpPort = 8678;
    int16_t WsPort = 8679;

    mocca::CommandLineParser parser;
    parser.addOption(TcpPortOption(TcpPort));
    parser.addOption(WsPortOption(WsPort));

    try {
        parser.parse(argc, argv);
    }
    catch (const mocca::CommandLineParser::ParserError& err) {
        std::cerr << err.what() << std::endl;
        std::exit(0);
    }

    // endpoints
    Endpoint e1(ConnectionFactorySelector::tcpPrefixed(), "localhost", std::to_string(TcpPort));
    Endpoint e2(ConnectionFactorySelector::tcpWebSocket(), "localhost", std::to_string(WsPort));

    // connection acceptors for the endpoints
    std::vector<std::unique_ptr<mocca::net::IMessageConnectionAcceptor>> acceptors =
        mocca::makeUniquePtrVec<IMessageConnectionAcceptor>(ConnectionFactorySelector::bind(e1), ConnectionFactorySelector::bind(e2));


    std::unique_ptr<ConnectionAggregator> aggregator(
        new ConnectionAggregator(std::move(acceptors), ConnectionAggregator::DisconnectStrategy::RemoveConnection));

    ProcessingNode node(std::move(aggregator));

    node.start();
    while (!exitFlag) {
        try {
            node.rethrowException();
        }
        catch (const std::runtime_error& err) {
            LERROR("(p) Error occurred: " << err.what());
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    node.join();
}
