#pragma once
#include <memory>
#include <string>

#include "mocca/net/Endpoint.h"
#include "mocca/net/IMessageConnection.h"
#include "mocca/net/IMessageConnectionAcceptor.h"
#include "mocca/base/Thread.h"
#include "mocca/base/Singleton.h"

#include "commands/ICommandFactory.h"



namespace trinity {
namespace common {

// dmc: are subclasses of ISession really used in a runtime polymorphic way, or do they actually just inherit the implementation?

class ISession : public mocca::Runnable {
    
public:
    ISession(const std::string& protocol,
             std::unique_ptr<commands::ICommandFactory> factory);
    
    virtual ~ISession();
    
    virtual int getSid() const { return m_sid; }
    virtual int getControlPort() const { return m_controlPort; }
        
protected:
    
    int m_sid;
    static int m_nextSid;
    static int m_basePort;
    int m_controlPort;

    // connections
    std::unique_ptr<mocca::net::IMessageConnectionAcceptor> m_acceptor;
    mocca::net::Endpoint                                    m_controlEndpoint;
    std::unique_ptr<mocca::net::IMessageConnection>         m_controlConnection;
        
    commands::Vcl m_vcl;
    std::unique_ptr<commands::ICommandFactory> m_factory;
    
    void run() override;
    
        
        
// todo: one day, we might want to release ids
};

// dmc: as far as I can see, sessions in the session manager are not used in a polymorphic way,
// i.e., they are dynamic_casted to the right type. why not have two session managers (for
// io-sessions and renderer-sessions) and avoid code duplication by templateizing the manager?
class SessionManager {
    
public:
    void addSession(std::unique_ptr<ISession> session);
    void endSession(int sid);
    ISession& getSession(int sid);
    void endAllSessions();
    
private:
    std::vector<std::unique_ptr<ISession> > m_sessions;
};

typedef mocca::Singleton<SessionManager> SessionManagerSingleton;
}
}