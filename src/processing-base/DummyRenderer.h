#pragma once
#include <memory>

#include "common/IRenderer.h"


namespace trinity {
namespace processing {
    
    
class DummyRenderer : public common::IRenderer {
    
public:
    
    DummyRenderer(std::shared_ptr<common::VisStream> stream);
};
}
}