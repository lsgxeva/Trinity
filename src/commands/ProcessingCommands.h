#pragma once

#include "common/IRenderer.h"
#include "commands/ISerializable.h"
#include "commands/Reply.h"
#include "commands/Request.h"

#include "mocca/net/Endpoint.h"

namespace trinity {

class StreamingParams : public SerializableTemplate<StreamingParams> {
public:
    StreamingParams();
    explicit StreamingParams(int resX, int resY);

    void serialize(ISerialWriter& writer) const override;
    void deserialize(const ISerialReader& reader) override;

    int getResX() const;
    int getResY() const;

    std::string toString() const;
    bool equals(const StreamingParams& other) const;

private:
    int m_resX, m_resY;
};

bool operator==(const StreamingParams& lhs, const StreamingParams& rhs);
std::ostream& operator<<(std::ostream& os, const StreamingParams& obj);

struct InitProcessingSessionCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;
        explicit RequestParams(const std::string& protocol, const VclType& renderType,
                               const std::string& fileId, const mocca::net::Endpoint& ioEndpoint,
                               const StreamingParams& p);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string getProtocol() const;
        mocca::net::Endpoint getIoEndpoint() const;
        std::string getFileId() const;
        VclType getRenderType() const;
        StreamingParams getStreamingParams() const;

        std::string toString() const;
        bool equals(const RequestParams& other) const;

    private:
        std::string m_protocol;
        VclType m_renderType;
        std::string m_fileId;
        mocca::net::Endpoint m_ioEndpoint;
        StreamingParams m_streamingParams;
    };

    class ReplyParams : public SerializableTemplate<ReplyParams> {
    public:
        ReplyParams() = default;
        explicit ReplyParams(const std::string& controlPort, const std::string& visPort);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string getControlPort() const;
        std::string getVisPort() const;

        std::string toString() const;
        bool equals(const ReplyParams& other) const;

    private:
        std::string m_controlPort;
        std::string m_visPort;
    };
};

bool operator==(const InitProcessingSessionCmd::RequestParams& lhs, const InitProcessingSessionCmd::RequestParams& rhs);
bool operator==(const InitProcessingSessionCmd::ReplyParams& lhs, const InitProcessingSessionCmd::ReplyParams& rhs);
std::ostream& operator<<(std::ostream& os, const InitProcessingSessionCmd::RequestParams& obj);
std::ostream& operator<<(std::ostream& os, const InitProcessingSessionCmd::ReplyParams& obj);

using InitProcessingSessionRequest = RequestTemplate<InitProcessingSessionCmd>;
using InitProcessingSessionReply = ReplyTemplate<InitProcessingSessionCmd>;

struct SetIsoValueCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;
        explicit RequestParams(float value);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        float getIsoValue() const;

        std::string toString() const;
        bool equals(const RequestParams& other) const;

    private:
        float m_isoValue;
    };
};

bool operator==(const SetIsoValueCmd::RequestParams& lhs, const SetIsoValueCmd::RequestParams& rhs);
std::ostream& operator<<(std::ostream& os, const SetIsoValueCmd::RequestParams& obj);

using SetIsoValueRequest = RequestTemplate<SetIsoValueCmd>;

struct InitContextCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;
        explicit RequestParams(int32_t value);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const RequestParams& other) const;

    private:
        int32_t m_dummy;
    };
};

bool operator==(const InitContextCmd::RequestParams& lhs, const InitContextCmd::RequestParams& rhs);
std::ostream& operator<<(std::ostream& os, const InitContextCmd::RequestParams& obj);

using InitContextRequest = RequestTemplate<InitContextCmd>;

struct ZoomCameraCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;
        explicit RequestParams(float value);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const RequestParams& other) const;
        float getZoom() const;
    private:
        float m_zoom;
    };

    class ReplyParams : public SerializableTemplate<ReplyParams> {
    public:
        ReplyParams() = default;

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const ReplyParams& other) const;
    };
};

bool operator==(const ZoomCameraCmd::RequestParams& lhs, const ZoomCameraCmd::RequestParams& rhs);
bool operator==(const ZoomCameraCmd::ReplyParams& lhs, const ZoomCameraCmd::ReplyParams& rhs);
std::ostream& operator<<(std::ostream& os, const ZoomCameraCmd::RequestParams& obj);
std::ostream& operator<<(std::ostream& os, const ZoomCameraCmd::ReplyParams& obj);

using ZoomCameraRequest = RequestTemplate<ZoomCameraCmd>;
using ZoomCameraReply = ReplyTemplate<ZoomCameraCmd>;

struct SetRenderModeCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;
        RequestParams(IRenderer::ERenderMode renderMode);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const RequestParams& other) const;

        IRenderer::ERenderMode getRenderMode() const;

    private:
        IRenderer::ERenderMode m_renderMode;
    };
};

bool operator==(const SetRenderModeCmd::RequestParams& lhs, const SetRenderModeCmd::RequestParams& rhs);
std::ostream& operator<<(std::ostream& os, const SetRenderModeCmd::RequestParams& obj);

using SetRenderModeRequest = RequestTemplate<SetRenderModeCmd>;


struct SupportsRenderModeCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;
        RequestParams(IRenderer::ERenderMode renderMode);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const RequestParams& other) const;

        IRenderer::ERenderMode getRenderMode() const;

    private:
        IRenderer::ERenderMode m_renderMode;
    };

    class ReplyParams : public SerializableTemplate<ReplyParams> {
    public:
        ReplyParams() = default;
        ReplyParams(bool result);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const ReplyParams& other) const;

        bool getResult() const;

    private:
        bool m_result;
    };
};

bool operator==(const SupportsRenderModeCmd::RequestParams& lhs, const SupportsRenderModeCmd::RequestParams& rhs);
bool operator==(const SupportsRenderModeCmd::ReplyParams& lhs, const SupportsRenderModeCmd::ReplyParams& rhs);
std::ostream& operator<<(std::ostream& os, const SupportsRenderModeCmd::RequestParams& obj);
std::ostream& operator<<(std::ostream& os, const SupportsRenderModeCmd::ReplyParams& obj);

using SupportsRenderModeRequest = RequestTemplate<SupportsRenderModeCmd>;
using SupportsRenderModeReply = ReplyTemplate<SupportsRenderModeCmd>;

struct SetActiveModalityCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;
        RequestParams(uint64_t modality);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const RequestParams& other) const;
    
        uint64_t getModality() const;

    private:
        uint64_t m_modality;
    };
};

bool operator==(const SetActiveModalityCmd::RequestParams& lhs, const SetActiveModalityCmd::RequestParams& rhs);
std::ostream& operator<<(std::ostream& os, const SetActiveModalityCmd::RequestParams& obj);

using SetActiveModalityRequest = RequestTemplate<SetActiveModalityCmd>;

struct GetActiveModalityCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const RequestParams& other) const;
    };

    class ReplyParams : public SerializableTemplate<ReplyParams> {
    public:
        ReplyParams() = default;
        ReplyParams(uint64_t modality);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const ReplyParams& other) const;

        uint64_t getModality() const;

    private:
        uint64_t m_modality;
    };
};

bool operator==(const GetActiveModalityCmd::RequestParams& lhs, const GetActiveModalityCmd::RequestParams& rhs);
bool operator==(const GetActiveModalityCmd::ReplyParams& lhs, const GetActiveModalityCmd::ReplyParams& rhs);
std::ostream& operator<<(std::ostream& os, const GetActiveModalityCmd::RequestParams& obj);
std::ostream& operator<<(std::ostream& os, const GetActiveModalityCmd::ReplyParams& obj);

using GetActiveModalityRequest = RequestTemplate<GetActiveModalityCmd>;
using GetActiveModalityReply = ReplyTemplate<GetActiveModalityCmd>;

struct SetActiveTimestepCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;
        RequestParams(uint64_t timestep);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const RequestParams& other) const;

        uint64_t getTimestep() const;

    private:
        uint64_t m_timestep;
    };
};

bool operator==(const SetActiveTimestepCmd::RequestParams& lhs, const SetActiveTimestepCmd::RequestParams& rhs);
std::ostream& operator<<(std::ostream& os, const SetActiveTimestepCmd::RequestParams& obj);

using SetActiveTimestepRequest = RequestTemplate<SetActiveTimestepCmd>;

struct GetActiveTimestepCmd {
    static VclType Type;

    class RequestParams : public SerializableTemplate<RequestParams> {
    public:
        RequestParams() = default;

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const RequestParams& other) const;
    };

    class ReplyParams : public SerializableTemplate<ReplyParams> {
    public:
        ReplyParams() = default;
        ReplyParams(uint64_t timestep);

        void serialize(ISerialWriter& writer) const override;
        void deserialize(const ISerialReader& reader) override;

        std::string toString() const;
        bool equals(const ReplyParams& other) const;

        uint64_t getTimestep() const;

    private:
        uint64_t m_timestep;
    };
};

bool operator==(const GetActiveTimestepCmd::RequestParams& lhs, const GetActiveTimestepCmd::RequestParams& rhs);
bool operator==(const GetActiveTimestepCmd::ReplyParams& lhs, const GetActiveTimestepCmd::ReplyParams& rhs);
std::ostream& operator<<(std::ostream& os, const GetActiveTimestepCmd::RequestParams& obj);
std::ostream& operator<<(std::ostream& os, const GetActiveTimestepCmd::ReplyParams& obj);

using GetActiveTimestepRequest = RequestTemplate<GetActiveTimestepCmd>;
using GetActiveTimestepReply = ReplyTemplate<GetActiveTimestepCmd>;

/* AUTOGEN CommandHeader */
}
