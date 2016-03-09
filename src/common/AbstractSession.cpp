#include "common/AbstractSession.h"

#include "mocca/base/Error.h"
#include "mocca/base/StringTools.h"
#include "mocca/log/LogManager.h"
#include "mocca/net/ConnectionFactorySelector.h"
#include "mocca/net/NetworkError.h"

using namespace trinity;

int AbstractSession::m_basePort = 5990;
int AbstractSession::m_nextSid = 1;

AbstractSession::AbstractSession(const std::string& protocol)
    : m_sid(m_nextSid++)
    , m_controlEndpoint(protocol, "localhost", std::to_string(m_basePort++)) {
    while (!m_acceptor) {
        try {
            m_acceptor = std::move(mocca::net::ConnectionFactorySelector::bind(m_controlEndpoint));
        } catch (const mocca::net::NetworkError&) {
            LINFO("(session) cannot bind on port " << m_basePort << ", rebinding...");
            m_controlEndpoint.port = std::to_string(m_basePort++);
        }
    }
}

AbstractSession::~AbstractSession() {
    join();
}

void AbstractSession::run() {
    LINFO("(session) session control at \"" + m_controlEndpoint.toString() + "\"");

    try {
        while (!m_controlConnection && !isInterrupted()) {
            m_controlConnection = m_acceptor->accept(); // auto-timeout
        }
    } catch (const mocca::net::NetworkError& err) {
        LERROR("(session) cannot bind the render session: " << err.what());
        return;
    }

    // dmc: isn't this code very similar to the code in Node.cpp? maybe related to the comment in Node.cpp
    while (!isInterrupted()) {
        try {
            auto bytepacket = m_controlConnection->receive();
            if (!bytepacket.isEmpty()) {
                auto request = Request::createFromByteArray(bytepacket);
                //LINFO("request: " << *request);
                
                auto handler = createHandler(*request);
                auto reply = handler->execute();
                if (reply != nullptr) { // not tested yet
                    //LINFO("reply: " << *reply);
                    auto serialReply = Reply::createByteArray(*reply);
                    m_controlConnection->send(std::move(serialReply));
                }
            }
        } catch (const mocca::net::NetworkError& err) {
            LWARNING("(session) interrupting because remote session has gone: " << err.what());
            interrupt();
        }
    }
}