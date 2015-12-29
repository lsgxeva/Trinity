#pragma once
#include <memory>
#include <string>

#include "mocca/net/Endpoint.h"
#include "mocca/net/IMessageConnection.h"
#include "mocca/base/Thread.h"

#include "common/IRenderer.h"
#include "common/Commands.h"
#include "VisStreamSender.h"




namespace trinity {
namespace processing {
    
enum class RenderType { DUMMY, GRIDLEAPER };
    
    class RenderSession : public mocca::Runnable {
    
public:
    
    RenderSession(const common::VclType& rendererType,
                  const common::StreamParams& params,
                  const std::string& protocol,
                  const std::string& visAddr);
        
    ~RenderSession();
    unsigned int getSid() const;
    int getControlPort() const;
        
        
private:
        
    static unsigned int         m_nextSid;
    static int                  m_basePort;
    int                         m_controlPort;
    unsigned int                m_sid;
        
    std::unique_ptr<common::IRenderer>                  m_renderer;
    mocca::net::Endpoint                                m_controlEndpoint;
    VisStreamSender                                     m_visSender;
    std::unique_ptr<mocca::net::IMessageConnection>     m_controlConnection;
        
    common::Vcl m_vcl;
    
    // renderer factory
    static std::unique_ptr<common::IRenderer> createRenderer(const common::VclType&,
                                                             const common::StreamParams&);
    void run() override;
    
        
        
// todo: one day, we might want to release ids
};
}
}