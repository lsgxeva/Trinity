#include "gtest/gtest.h"

#include "mocca/net/ConnectionFactorySelector.h"

#include "common/Icommand.h"
#include "common/Commands.h"
#include "common/ICommandHandler.h"
#include "common/InitRendererCmd.h"

#include "processing-base/ProcessingCommandFactory.h"
#include "processing-base/InitRendererHdl.h"
#include "processing-base/SessionManager.h"

using namespace trinity::common;
using namespace trinity::processing;

class CmdFactoryTest : public ::testing::Test {
protected:

    CmdFactoryTest() {
        mocca::net::ConnectionFactorySelector::addDefaultFactories();
    }
    
    virtual ~CmdFactoryTest() {
        SessionManagerSingleton::instance()->endAllSessions();
        mocca::net::ConnectionFactorySelector::removeAll();
    }
};


TEST_F(CmdFactoryTest, RendererExecTest) {
    StreamingParams params(2048, 1000);
    InitRendererCmd cmd(1, 2, "loopback", VclType::DummyRenderer ,params);
    std::stringstream s;
    cmd.serialize(s);
    std::cout << s.str() << std::endl;

    ProcessingCommandFactory factory;
    auto handler = factory.createHandler(s);
    
    handler->execute();
    
    std::unique_ptr<ICommand> rep = handler->getReturnValue();
    ASSERT_TRUE(rep != nullptr);
    
    ICommand* repPtr = rep.release();
    ReplyInitRendererCmd* castedPtr = dynamic_cast<ReplyInitRendererCmd*>(repPtr);
    ASSERT_TRUE(castedPtr != nullptr);
    ASSERT_EQ(castedPtr->getType(), VclType::TrinityReturn);
    ASSERT_EQ(castedPtr->getVisPort() - castedPtr->getControlPort(), 1);
     
     
     
     
    
    // todo: execute, get return value
    
}