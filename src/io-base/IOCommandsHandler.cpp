#include "io-base/IOCommandsHandler.h"

#include "common/TrinityError.h"
#include "io-base/IOCommandFactory.h"
#include "io-base/IOSession.h"

#include "mocca/base/Memory.h"

using namespace trinity;

InitIOSessionHdl::InitIOSessionHdl(const InitIOSessionRequest& request, IONode* node)
    : m_node(node), m_request(request) {}

std::unique_ptr<Reply> InitIOSessionHdl::execute() {
    auto requestParams = m_request.getParams();

    auto fileID = requestParams.getFileId();
    auto& listData = m_node->getListDataForID(fileID);
    auto session = mocca::make_unique<IOSession>(requestParams.getProtocol(), IOSession::createIO(fileID, listData));
    session->start();
    
    InitIOSessionCmd::ReplyParams replyParams(session->getControlPort());
    std::unique_ptr<Reply> reply = mocca::make_unique<InitIOSessionReply>(replyParams, m_request.getRid(), session->getSid());
    m_node->addSession(std::move(session));
    return reply;
}


GetLODLevelCountHdl::GetLODLevelCountHdl(const GetLODLevelCountRequest& request, IOSession* session)
    : m_session(session), m_request(request) {}

std::unique_ptr<Reply> GetLODLevelCountHdl::execute() {
    const auto& io = m_session->getIO();
    GetLODLevelCountCmd::ReplyParams replyParams(io.getLODLevelCount(m_request.getParams().getModality()));
    return mocca::make_unique<GetLODLevelCountReply>(replyParams, m_request.getRid(), m_request.getSid());
}


ListFilesHdl::ListFilesHdl(const ListFilesRequest& request, IONode* node)
    : m_node(node), m_request(request) {}

std::unique_ptr<Reply> ListFilesHdl::execute() {
    std::vector<IOData> ioDataVec;
    auto dirID = m_request.getParams().getDirID();
    auto& listData = m_node->getListDataForID(dirID);
    ListFilesCmd::ReplyParams replyParams(listData.listData(dirID));
    return mocca::make_unique<ListFilesReply>(replyParams, m_request.getRid(), m_request.getSid());
}

#define PYTHON_MAGIC



GetMaxBrickSizeHdl::GetMaxBrickSizeHdl(const GetMaxBrickSizeRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetMaxBrickSizeHdl::execute() {
    GetMaxBrickSizeCmd::ReplyParams params(m_session->getIO().getMaxBrickSize());
    return mocca::make_unique<GetMaxBrickSizeReply>(params, m_request.getRid(), m_session->getSid());
}

GetMaxUsedBrickSizesHdl::GetMaxUsedBrickSizesHdl(const GetMaxUsedBrickSizesRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetMaxUsedBrickSizesHdl::execute() {
    GetMaxUsedBrickSizesCmd::ReplyParams params(m_session->getIO().getMaxUsedBrickSizes());
    return mocca::make_unique<GetMaxUsedBrickSizesReply>(params, m_request.getRid(), m_session->getSid());
}

MaxMinForKeyHdl::MaxMinForKeyHdl(const MaxMinForKeyRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> MaxMinForKeyHdl::execute() {
    auto brickKey = m_request.getParams().getBrickKey();
    auto minMax = m_session->getIO().maxMinForKey(brickKey);
    MaxMinForKeyCmd::ReplyParams params(minMax);
    return mocca::make_unique<MaxMinForKeyReply>(params, m_request.getRid(), m_session->getSid());
}

GetNumberOfTimestepsHdl::GetNumberOfTimestepsHdl(const GetNumberOfTimestepsRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetNumberOfTimestepsHdl::execute() {
    GetNumberOfTimestepsCmd::ReplyParams params(m_session->getIO().getNumberOfTimesteps());
    return mocca::make_unique<GetNumberOfTimestepsReply>(params, m_request.getRid(), m_session->getSid());
}

GetDomainSizeHdl::GetDomainSizeHdl(const GetDomainSizeRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetDomainSizeHdl::execute() {
    auto lod = m_request.getParams().getLod();
    auto modality = m_request.getParams().getModality();
    GetDomainSizeCmd::ReplyParams params(m_session->getIO().getDomainSize(lod, modality));
    return mocca::make_unique<GetDomainSizeReply>(params, m_request.getRid(), m_session->getSid());
}

GetTransformationHdl::GetTransformationHdl(const GetTransformationRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetTransformationHdl::execute() {
    GetTransformationCmd::ReplyParams params(m_session->getIO().getTransformation(m_request.getParams().getModality()));
    return mocca::make_unique<GetTransformationReply>(params, m_request.getRid(), m_session->getSid());
}

GetBrickOverlapSizeHdl::GetBrickOverlapSizeHdl(const GetBrickOverlapSizeRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetBrickOverlapSizeHdl::execute() {
    GetBrickOverlapSizeCmd::ReplyParams params(m_session->getIO().getBrickOverlapSize());
    return mocca::make_unique<GetBrickOverlapSizeReply>(params, m_request.getRid(), m_session->getSid());
}

GetLargestSingleBrickLODHdl::GetLargestSingleBrickLODHdl(const GetLargestSingleBrickLODRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetLargestSingleBrickLODHdl::execute() {
    auto modality = m_request.getParams().getModality();
    GetLargestSingleBrickLODCmd::ReplyParams params(m_session->getIO().getLargestSingleBrickLOD(modality));
    return mocca::make_unique<GetLargestSingleBrickLODReply>(params, m_request.getRid(), m_session->getSid());
}

GetBrickVoxelCountsHdl::GetBrickVoxelCountsHdl(const GetBrickVoxelCountsRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetBrickVoxelCountsHdl::execute() {
    GetBrickVoxelCountsCmd::ReplyParams params(m_session->getIO().getBrickVoxelCounts(m_request.getParams().getBrickKey()));
    return mocca::make_unique<GetBrickVoxelCountsReply>(params, m_request.getRid(), m_session->getSid());
}

GetBrickExtentsHdl::GetBrickExtentsHdl(const GetBrickExtentsRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetBrickExtentsHdl::execute() {
    GetBrickExtentsCmd::ReplyParams params(m_session->getIO().getBrickExtents(m_request.getParams().getBrickKey()));
    return mocca::make_unique<GetBrickExtentsReply>(params, m_request.getRid(), m_session->getSid());
}

GetBrickLayoutHdl::GetBrickLayoutHdl(const GetBrickLayoutRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetBrickLayoutHdl::execute() {
    auto lod = m_request.getParams().getLod();
    auto modality = m_request.getParams().getModality();
    GetBrickLayoutCmd::ReplyParams params(m_session->getIO().getBrickLayout(lod, modality));
    return mocca::make_unique<GetBrickLayoutReply>(params, m_request.getRid(), m_session->getSid());
}

GetModalityCountHdl::GetModalityCountHdl(const GetModalityCountRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetModalityCountHdl::execute() {
    GetModalityCountCmd::ReplyParams params(m_session->getIO().getModalityCount());
    return mocca::make_unique<GetModalityCountReply>(params, m_request.getRid(), m_session->getSid());
}

GetComponentCountHdl::GetComponentCountHdl(const GetComponentCountRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetComponentCountHdl::execute() {
    GetComponentCountCmd::ReplyParams params(m_session->getIO().getComponentCount(m_request.getParams().getModality()));
    return mocca::make_unique<GetComponentCountReply>(params, m_request.getRid(), m_session->getSid());
}

GetRangeHdl::GetRangeHdl(const GetRangeRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetRangeHdl::execute() {
    GetRangeCmd::ReplyParams params(m_session->getIO().getRange(m_request.getParams().getModality()));
    return mocca::make_unique<GetRangeReply>(params, m_request.getRid(), m_session->getSid());
}

GetTotalBrickCountHdl::GetTotalBrickCountHdl(const GetTotalBrickCountRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetTotalBrickCountHdl::execute() {
    GetTotalBrickCountCmd::ReplyParams params(m_session->getIO().getTotalBrickCount(m_request.getParams().getModality()));
    return mocca::make_unique<GetTotalBrickCountReply>(params, m_request.getRid(), m_session->getSid());
}

GetBrickHdl::GetBrickHdl(const GetBrickRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetBrickHdl::execute() {
    auto brickKey = m_request.getParams().getBrickKey();
    auto brick = std::make_shared<std::vector<uint8_t>>();
    auto success = m_session->getIO().getBrick(brickKey, *brick);
    GetBrickCmd::ReplyParams params(brick, success);
    return mocca::make_unique<GetBrickReply>(params, m_request.getRid(), m_session->getSid());
}

GetTypeHdl::GetTypeHdl(const GetTypeRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetTypeHdl::execute() {
    GetTypeCmd::ReplyParams params(m_session->getIO().getType(m_request.getParams().getModality()));
    return mocca::make_unique<GetTypeReply>(params, m_request.getRid(), m_session->getSid());
}

GetSemanticHdl::GetSemanticHdl(const GetSemanticRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetSemanticHdl::execute() {
    GetSemanticCmd::ReplyParams params(m_session->getIO().getSemantic(m_request.getParams().getModality()));
    return mocca::make_unique<GetSemanticReply>(params, m_request.getRid(), m_session->getSid());
}

GetDefault1DTransferFunctionCountHdl::GetDefault1DTransferFunctionCountHdl(const GetDefault1DTransferFunctionCountRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetDefault1DTransferFunctionCountHdl::execute() {
    GetDefault1DTransferFunctionCountCmd::ReplyParams params(m_session->getIO().getDefault1DTransferFunctionCount());
    return mocca::make_unique<GetDefault1DTransferFunctionCountReply>(params, m_request.getRid(), m_session->getSid());
}

GetDefault2DTransferFunctionCountHdl::GetDefault2DTransferFunctionCountHdl(const GetDefault2DTransferFunctionCountRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> GetDefault2DTransferFunctionCountHdl::execute() {
    GetDefault2DTransferFunctionCountCmd::ReplyParams params(m_session->getIO().getDefault2DTransferFunctionCount());
    return mocca::make_unique<GetDefault2DTransferFunctionCountReply>(params, m_request.getRid(), m_session->getSid());
}

Get1DHistogramHdl::Get1DHistogramHdl(const Get1DHistogramRequest& request, IOSession* session)
    : m_request(request), m_session(session) {}

std::unique_ptr<Reply> Get1DHistogramHdl::execute() {
    Get1DHistogramCmd::ReplyParams params(m_session->getIO().get1DHistogram());
    return mocca::make_unique<Get1DHistogramReply>(params, m_request.getRid(), m_session->getSid());
}

#undef PYTHON_MAGIC