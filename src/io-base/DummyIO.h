#pragma once
#include <memory>
#include <iostream>

#include "common/IIO.h"


namespace trinity {
namespace io {
    
    class DummyIO : public common::IIO {
        
    public:
        
        // testing purpose
        virtual int getLODLevelCount()  { return 42; }
};
}
}