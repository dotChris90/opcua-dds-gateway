#pragma once

#include "gateway/config/GatewayConfig.hpp"
#include "gateway/dds_to_opcua/AddressSpaceBuilder.hpp"
#include "gateway/model/ShapeType.hpp"

#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <dds/dds.hpp>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace gateway {
namespace dds_to_opcua {

class DdsToOpcUaBridge {
public:
    explicit DdsToOpcUaBridge(const config::DdsToOpcUaBridgeConfig& config,
                             const config::OpcUaServerConfig& serverConfig,
                             const config::DomainParticipantConfig& dpConfig);
    ~DdsToOpcUaBridge();

    bool initialize();
    void start();
    void stop();
    void processDdsEventsOnce();

    UA_Server* getServer() const { return server_; }

private:
    void runServerLoop();
    void setupShapeTopicBridge(const UA_NodeId& domainNodeId, const config::DdsTopicConfig& topicConfig);
    void handleDdsSample(const ShapeType& sample, const dds::sub::SampleInfo& info, const std::string& topicName);

    config::DdsToOpcUaBridgeConfig bridgeConfig_;
    config::OpcUaServerConfig serverConfig_;
    config::DomainParticipantConfig dpConfig_;

    UA_Server* server_ = nullptr;
    std::unique_ptr<AddressSpaceBuilder> builder_;

    dds::domain::DomainParticipant participant_ = dds::core::null;
    dds::topic::Topic<ShapeType> shapeTopic_ = dds::core::null;
    dds::sub::DataReader<ShapeType> shapeReader_ = dds::core::null;
    dds::pub::DataWriter<ShapeType> shapeWriter_ = dds::core::null;

    std::map<std::string, InstanceNodeInfo> shapeInstances_;
    std::mutex instancesMutex_;

    std::thread serverThread_;
    std::thread ddsPollThread_;
    std::atomic<bool> running_{false};
};

} // namespace dds_to_opcua
} // namespace gateway
