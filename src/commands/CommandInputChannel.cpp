#include "commands/CommandInputChannel.h"

#include "commands/Request.h"
#include "common/NetConfig.h"
#include "common/TrinityError.h"

#include "mocca/base/ByteArray.h"
#include "mocca/log/LogManager.h"
#include "mocca/net/ConnectionFactorySelector.h"
#include "mocca/net/NetworkError.h"

using namespace trinity::commands;
using namespace trinity::common;


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

    auto serialRequest = Request::createByteArray(request);
    m_mainChannel->send(std::move(serialRequest));
}

mocca::net::Endpoint CommandInputChannel::getEndpoint() const {
    return m_endpoint;
}

std::unique_ptr<Reply> CommandInputChannel::getReply(const std::chrono::milliseconds& ms) const {
    auto serialReply = m_mainChannel->receive(ms);

    if (serialReply.isEmpty()) {
        throw TrinityError("(chn) no reply arrived", __FILE__, __LINE__);
    }

    auto reply = Reply::createFromByteArray(serialReply);
    return reply;
}