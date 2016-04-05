#pragma once

#include "commands/ISerialReader.h"

#include "thirdparty/jsoncpp/json.h"

#include "mocca/net/Message.h"
#include "mocca/base/ByteArray.h"

namespace trinity {

class JsonReader : public ISerialReader {
public:

    JsonReader(const std::string& json);
    JsonReader(const mocca::net::Message& message);
    JsonReader(mocca::ByteArray& data);

    float getFloat(const std::string& key) const override;
    double getDouble(const std::string& key) const override;
    uint8_t getUInt8(const std::string& key) const override;
    int32_t getInt32(const std::string& key) const override;
    uint32_t getUInt32(const std::string& key) const override;
    int64_t getInt64(const std::string& key) const override;
    uint64_t getUInt64(const std::string& key) const override;
    bool getBool(const std::string& key) const override;
    std::string getString(const std::string& key) const override;

    std::vector<float> getFloatVec(const std::string& key) const override;
    std::vector<uint8_t> getUInt8Vec(const std::string& key) const override;
    std::vector<int32_t> getInt32Vec(const std::string& key) const override;
    std::vector<uint64_t> getUInt64Vec(const std::string& key) const override;
    std::vector<bool> getBoolVec(const std::string& key) const override;
    std::vector<std::string> getStringVec(const std::string& key) const override;

    SharedDataVec getBinary() const override;
    
private:
    JsonReader(const JsonCpp::Value& root, SharedDataVec binary);

    void getSerializableImpl(const std::string& key, ISerializable& prototype) const override;
    std::vector<std::unique_ptr<ISerializable>> getSerializableVecImpl(const std::string& key, const ISerializable& prototype) const override;

private:
    JsonCpp::Value m_root;
    SharedDataVec m_binary;
};
}