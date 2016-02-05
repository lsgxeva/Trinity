#pragma once
#include "ICommand.h"
#include "StreamingParams.h"

namespace trinity {
namespace common {

class InitRendererCmd : public ICommand {
    
public:
    InitRendererCmd();
    InitRendererCmd(int sid, int rid, const std::string& protocol,
                         const VclType& renderType, const StreamingParams& p);
    
    virtual ~InitRendererCmd();
    
    virtual VclType getType() const;
    const std::string& getProtocol() const;
    const StreamingParams& getParams() const;
    
    virtual void serialize(ISerialObject& serial) const;
    virtual void deserialize(ISerialObject& serial);
    
private:
    std::string m_protocol;
    VclType m_renderType;
    StreamingParams m_streamingParams;
};


class ReplyInitRendererCmd : public ICommand {
    
public:
    ReplyInitRendererCmd();
    ReplyInitRendererCmd(int sid, int rid);
    virtual ~ReplyInitRendererCmd();
    
    virtual VclType getType() const override;
    int getControlPort() const;
    int getVisPort() const;
    void setControlPort(const int port);
    void setVisPort(const int port);
    void setNewSid(const int sid);
    
    virtual void serialize(ISerialObject& serial) const override;
    virtual void deserialize(ISerialObject& serial) override;
    
private:
    int m_controlPort;
    int m_visPort;
};
}
}