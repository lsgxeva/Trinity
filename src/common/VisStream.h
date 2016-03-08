#pragma once
#include <atomic>
#include <cassert>
#include <memory>
#include <thread>

#include "mocca/base/ByteArray.h"
#include "mocca/base/MessageQueue.h"

#include "commands/ProcessingCommands.h"
#include "commands/Vcl.h"

namespace trinity {

using Frame = mocca::ByteArray;

class VisStream {
public:
    VisStream(StreamingParams params);

    const StreamingParams& getStreamingParams() const;

    void put(Frame frame); // false if stream full
    mocca::Nullable<Frame> get();

private:
    StreamingParams m_streamingParams;
    mocca::MessageQueue<Frame> m_queue;
};
}