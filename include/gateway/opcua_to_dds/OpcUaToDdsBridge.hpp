#pragma once

#include "gateway/config/GatewayConfig.hpp"
#include "gateway/model/ShapeType.hpp"

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_subscriptions.h>
#include <dds/dds.hpp>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace gateway {
namespace opcua_to_dds {

struct MonitoredItemContext {
    class OpcUaToDdsBridge* bridge;
    std::string inputName;
    std::string dataItemName;
    uint32_t clientHandle;
};

class OpcUaToDdsBridge {
public:
    explicit OpcUaToDdsBridge(const config::OpcUaToDdsBridgeConfig& config,
                             const std::vector<config::OpcUaConnectionConfig>& connections,
                             const std::vector<config::DomainParticipantConfig>& participants);
    ~OpcUaToDdsBridge();

    bool initialize();
    void start();
    void stop();

    void onDataChange(const std::string& inputName, const std::string& itemName, const UA_DataValue* value);

private:
    void runClientLoop();

    config::OpcUaToDdsBridgeConfig bridgeConfig_;
    std::map<std::string, config::OpcUaConnectionConfig> connections_;
    std::map<std::string, config::DomainParticipantConfig> participants_;

    std::map<std::string, UA_Client*> clients_;
    std::vector<std::unique_ptr<MonitoredItemContext>> itemContexts_;

    // Cyclone DDS entities
    dds::domain::DomainParticipant participant_ = dds::core::null;
    dds::topic::Topic<ShapeType> shapeTopic_ = dds::core::null;
    dds::pub::DataWriter<ShapeType> shapeWriter_ = dds::core::null;

    // Current output state buffers (topicName -> ShapeType)
    std::map<std::string, ShapeType> outputBuffers_;
    std::mutex bufferMutex_;

    std::thread clientThread_;
    std::atomic<bool> running_{false};
};

} // namespace opcua_to_dds
} // namespace gateway
