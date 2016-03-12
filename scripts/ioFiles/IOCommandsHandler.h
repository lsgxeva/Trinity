#pragma once

#include "commands/ICommandHandler.h"
#include "commands/IOCommands.h"
#include "io-base/IONode.h"
#include "io-base/IOSession.h"

namespace trinity {
// command-pattern like execution of trinity commands
class InitIOSessionHdl : public ICommandHandler {
public:
    InitIOSessionHdl(const InitIOSessionRequest& request, IONode* node);

    std::unique_ptr<Reply> execute() override;

private:
    IONode* m_node;
    InitIOSessionRequest m_request;
};


class GetLODLevelCountHdl : public ICommandHandler {
public:
    GetLODLevelCountHdl(const GetLODLevelCountRequest& request, IOSession* session);

    std::unique_ptr<Reply> execute() override;

private:
    IOSession* m_session;
    GetLODLevelCountRequest m_request;
};


class ListFilesHdl : public ICommandHandler {
public:
    ListFilesHdl(const ListFilesRequest& request, IONode* node);

    std::unique_ptr<Reply> execute() override;

private:
    IONode* m_node;
    ListFilesRequest m_request;
};
}