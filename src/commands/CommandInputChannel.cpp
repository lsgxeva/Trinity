#include "commands/CommandInputChannel.h"

#include "commands/Request.h"
#include "common/NetConfig.h"
#include "common/TrinityError.h"

#include "mocca/base/ByteArray.h"
#include "mocca/log/LogManager.h"
#include "mocca/net/ConnectionFactorySelector.h"
#include "mocca/net/NetworkError.h"
#include "mocca/net/message/NewLoopbackConnection.h"

using namespace trinity;

CommandInputChannel::CommandInputChannel(const mocca::net::Endpoint& endpoint)
    : m_endpoint(endpoint) {}

bool CommandInputChannel::connect() const {
    try {
        m_mainChannel = mocca::net::ConnectionFactorySelector::connect(m_endpoint);
    } catch (const mocca::net::ConnectFailedError&) {
        LWARNING("(chn) no connection to a node  at \"" << m_endpoint << "\": ");
        return false;
    }

    LINFO("(chn) successfully connected to a node");
    return true;
}

void CommandInputChannel::sendRequest(const Request& request) const {
    if (!m_mainChannel)
        throw TrinityError("(chn) cannot send command: channel not connected", __FILE__, __LINE__);

    static int count = 0;
    if (auto loopback = dynamic_cast<mocca::net::NewLoopbackConnection*>(m_mainChannel.get())) {
        auto serialRequest = Request::createMessage(request);
        try {
            loopback->sendNew(std::move(serialRequest));
        } catch (const mocca::net::NetworkError& err) {
            LERROR("(chn) cannot send request: " << err.what());
        }
    } else {
        auto serialRequest = Request::createByteArray(request);
        try {
            m_mainChannel->send(std::move(serialRequest));
        } catch (const mocca::net::NetworkError& err) {
            LERROR("(chn) cannot send request: " << err.what());
        }
    }
}

mocca::net::Endpoint CommandInputChannel::getEndpoint() const {
    return m_endpoint;
}

std::unique_ptr<Reply> CommandInputChannel::getReply(const std::chrono::milliseconds& ms) const {
    if (auto ptr = dynamic_cast<mocca::net::NewLoopbackConnection*>(m_mainChannel.get())) {
        auto serialReply = ptr->receiveNew(ms);
        if (serialReply.isEmpty()) {
            throw TrinityError("(chn) no reply arrived", __FILE__, __LINE__);
        }
        auto reply = Reply::createFromMessage(serialReply);
        return reply;
    } else {
        auto serialReply = m_mainChannel->receive(ms);
        if (serialReply.isEmpty()) {
            throw TrinityError("(chn) no reply arrived", __FILE__, __LINE__);
        }
        auto reply = Reply::createFromByteArray(serialReply);
        return reply;
    }
}