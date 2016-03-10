#include "io-base/IOCommandFactory.h"

#include "common/TrinityError.h"
#include "commands/IOCommands.h"
#include "io-base/IOCommandsHandler.h"
#include "io-base/IONode.h"
#include "io-base/IOSession.h"

#include "mocca/base/Error.h"
#include "mocca/base/Memory.h"

using namespace trinity;

std::unique_ptr<ICommandHandler> IONodeCommandFactory::createHandler(const Request& request, IONode* node) const {
    VclType type = request.getType();

    switch (type) {
    case VclType::InitIOSession:
        return mocca::make_unique<InitIOSessionHdl>(static_cast<const InitIOSessionRequest&>(request), node);
        break;
    case VclType::ListFiles:
        return mocca::make_unique<ListFilesHdl>(static_cast<const ListFilesRequest&>(request), node);
        break;
    default:
        throw TrinityError("command unknown: " + (Vcl::instance().toString(type)), __FILE__, __LINE__);
    }
}

std::unique_ptr<ICommandHandler> IOSessionCommandFactory::createHandler(const Request& request, IOSession* session) const {
    VclType type = request.getType();

    switch (type) {
    case VclType::GetLODLevelCount:
        return mocca::make_unique<GetLODLevelCountHdl>(static_cast<const GetLODLevelCountRequest&>(request), session);
        break;
    default:
        throw TrinityError("command unknown: " + (Vcl::instance().toString(type)), __FILE__, __LINE__);
    }
}