#pragma once
#include <memory>
#include <string>
#include "common/IRenderer.h"
#include "mocca/net/Endpoint.h"
#include "mocca/net/IMessageConnection.h"
#include "mocca/base/Thread.h"
#include "common/Commands.h"



namespace trinity {
    
enum class RenderType { DUMMY, GRIDLEAPER };
    
    class RenderSession : public mocca::Runnable {
    
public:
    
    RenderSession(const VclType&);
    ~RenderSession();
    unsigned int getSid() const;
    int getPort() const;
    
    void provideOwnEndpoint(std::unique_ptr<mocca::net::Endpoint>);
    
    
        
        
private:
    static unsigned int m_nextSid;
    static int m_basePort;
    int m_port;
    unsigned int m_sid;
    Vcl m_vcl;
    std::unique_ptr<IRenderer> m_renderer;
    std::unique_ptr<mocca::net::Endpoint> m_endpoint;
    std::unique_ptr<mocca::net::IMessageConnection> m_connection;
    
    // renderer factory
    static std::unique_ptr<IRenderer> createRenderer(const VclType& t);
    void run() override;
    
        
        
// todo: one day, we might want to release ids
    
    
    
};
}