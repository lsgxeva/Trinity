#pragma once
#include <memory>
#include "mocca/base/BidirectionalMap.h"


// subject of refactoring
namespace trinity {
namespace common {
    
enum class VclType {

    TrinityReturn,
    TrinityError,
    InitConnection,
    CloseConnection,
    InitRenderer,
    CloseRenderer,
    DummyRenderer,
    GridLeaper,
    GetFrameBuffer,
    PushMode,
    PullMode
};
    

// use this class to create and parse trinity commands
class Vcl {
    
public:
    
    // throws Error if entry not found
    std::string toString(const VclType& t) {
        return m_cmdMap.getBySecond(t);
    }
    
    // throws Error if entry not found
    VclType toType(const std::string& str) {
        return m_cmdMap.getByFirst(str);
    }
    
    
    bool isSoundCommand(std::vector<std::string>& args) {
        return (args.size() < m_cmdLength)? false : true;
    }
    
    
    std::string assembleInitConnection(int sid, int rid) {
        std::stringstream cmd;
        cmd << toString(VclType::InitConnection)
        << "_" << std::to_string(sid) << "_" <<  rid;
        return cmd.str();
    }
    
    std::string assembleCloseConnection(int sid, int rid) {
        std::stringstream cmd;
        cmd << toString(VclType::CloseConnection)
        << "_" << std::to_string(sid) << "_" <<  rid;
        return cmd.str();
    }
    
    std::string assembleInitRenderer(int sid, int rid, const VclType& t) {
        std::stringstream cmd;
        cmd << toString(VclType::InitRenderer)
        << "_" << std::to_string(sid) << "_" <<  rid << "_" << toString(t);
        return cmd.str();
    }
    
    std::string assembleCloseRenderer(int sid, int rid) {
        std::stringstream cmd;
        cmd << toString(VclType::CloseRenderer)
        << "_" << std::to_string(sid) << "_" <<  rid;
        return cmd.str();
    }
    
    std::string assembleGetFrameBuffer(int sid, int rid) {
        std::stringstream cmd;
        cmd << toString(VclType::GetFrameBuffer)
        << "_" << std::to_string(sid) << "_" <<  rid;
        return cmd.str();
    }
    
    std::string assembleError(int sid, int rid, int errorCode) {
        std::stringstream cmd;
        cmd << toString(VclType::TrinityError)
        << "_" << std::to_string(sid) << "_" <<  rid << "_" << errorCode;
        return cmd.str();
    }
    
    std::string assembleRetSpawnRenderer(int sid, int rid, int newSid, int port) {
        std::stringstream cmd;
        cmd << toString(VclType::TrinityReturn)
        << "_" << std::to_string(sid) << "_" <<  rid << "_" << newSid << "_" << port;
        return cmd.str();
    }
    
    std::stringstream assembleRetHeader(int sid, int rid) {
        std::stringstream cmd;
        cmd << toString(VclType::GetFrameBuffer)
        << "_" << std::to_string(sid) << "_" <<  rid << "_";
        return cmd;
    }

    Vcl() {
        
        m_cmdMap.insert("RET", VclType::TrinityReturn);
        m_cmdMap.insert("ERR", VclType::TrinityError);
        m_cmdMap.insert("CON", VclType::InitConnection);
        m_cmdMap.insert("EXT", VclType::CloseConnection);
        m_cmdMap.insert("INR", VclType::InitRenderer);
        m_cmdMap.insert("CLR", VclType::CloseRenderer);
        m_cmdMap.insert("DRN", VclType::DummyRenderer);
        m_cmdMap.insert("GRN", VclType::GridLeaper);
        m_cmdMap.insert("GFB", VclType::GetFrameBuffer);
        m_cmdMap.insert("PSH", VclType::PushMode);
        m_cmdMap.insert("PLL", VclType::PullMode);
        
    }
    
    
private:
    const int m_cmdLength = 3;
    mocca::BidirectionalMap<std::string, VclType> m_cmdMap;
};
    


class IDGenerator {
private:
     int m_id;
    
public:
    IDGenerator() : m_id(1) {}
    int nextID() { return m_id++; }
};
}
} // end trinity