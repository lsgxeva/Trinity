#include "io-base/IOSession.h"

#include "common/TrinityError.h"
#include "commands/Vcl.h"
#include "fractal/FractalIO.h"
#include "io-base/DummyIO.h"

#include "mocca/base/Error.h"
#include "mocca/base/StringTools.h"
#include "mocca/log/LogManager.h"
#include "mocca/net/ConnectionFactorySelector.h"
#include "mocca/net/NetworkError.h"

using namespace trinity;

IOSession::IOSession(const std::string& protocol, std::unique_ptr<ICommandFactory> factory, int fileId)
    : ISession(protocol, std::move(factory)) {
    m_io = createIO(fileId);
}

std::unique_ptr<trinity::IIO> IOSession::createIO(int fileId) {
    switch (fileId) {
    case 0:  // Dummy File
        return std::unique_ptr<DummyIO>(new DummyIO());
        break;

    case 24: // Fractal File
        return std::unique_ptr<FractalIO>(new FractalIO());
        break;

    default:
        throw TrinityError("can't create renderer: no such type", __FILE__, __LINE__);
        break;
    }
}
