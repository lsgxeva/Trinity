#include "gtest/gtest.h"
#include "processing-dummy/ProcessingNode.h"
#include "frontend-dummy/ProcessingPrx.h"

#include "mocca/net/NetworkServiceLocator.h"
#include "mocca/net/Endpoint.h"


using namespace mocca::net;

class ProcessingTest : public ::testing::Test {
protected:
    
    ProcessingTest() {
        NetworkServiceLocator::provideAll();
    }

    virtual ~ProcessingTest() {
        NetworkServiceLocator::removeAll();
    }
};

TEST_F(ProcessingTest, AcceptInitRendererTest) {
    
    Endpoint endpoint (NetworkServiceLocator::loopback(), "5678");
    
    trinity::ProcessingNode node(endpoint);
    node.start();
    
    trinity::ProcessingPrx proxy(endpoint);
    
    ASSERT_TRUE(proxy.connect());
    ASSERT_NO_THROW(proxy.spawnRenderer(trinity::vcl::DUMMY_RENDERER));

    node.interrupt();
}


TEST_F(ProcessingTest, SpawnRendererTest) {
    
    Endpoint endpoint (NetworkServiceLocator::loopback(), "5678");
    
    trinity::ProcessingNode node(endpoint);
    node.start();
    trinity::ProcessingPrx proxy(endpoint);

    ASSERT_TRUE(proxy.connect());
    std::unique_ptr<trinity::RendererPrx> renderer;
    ASSERT_NO_THROW(renderer = proxy.spawnRenderer(trinity::vcl::DUMMY_RENDERER));
    ASSERT_TRUE(renderer->connect());
    
    node.interrupt();
}


TEST_F(ProcessingTest, FrameBufferTest) {
    
    Endpoint endpoint (NetworkServiceLocator::loopback(), "5678");
    
    trinity::ProcessingNode node(endpoint);
    node.start();
    trinity::ProcessingPrx proxy(endpoint);
    
    ASSERT_TRUE(proxy.connect());
    std::unique_ptr<trinity::RendererPrx> renderer;
    ASSERT_NO_THROW(renderer = proxy.spawnRenderer(trinity::vcl::DUMMY_RENDERER));
    ASSERT_TRUE(renderer->connect());
    
    ASSERT_NO_THROW(renderer->getFrameBuffer());
    
    node.interrupt();
}