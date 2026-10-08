#include "gateway/dds_to_opcua/DdsToOpcUaBridge.hpp"
#include <iostream>
#include <chrono>

namespace gateway {
namespace dds_to_opcua {

struct FieldWriteContext {
    DdsToOpcUaBridge* bridge;
    std::string instanceColor;
    std::string fieldName;
};

static void onOpcUaFieldWritten(UA_Server *server, const UA_NodeId *sessionId,
                               void *sessionContext, const UA_NodeId *nodeId,
                               void *nodeContext, const UA_NumericRange *range,
                               const UA_DataValue *data) {
    (void)server;
    (void)sessionId;
    (void)sessionContext;
    (void)nodeId;
    (void)range;
    if (!nodeContext || !data || !data->hasValue) return;
    auto* ctx = static_cast<FieldWriteContext*>(nodeContext);
    
    // We can handle field write directly or bridge updates
    std::cout << "[DdsToOpcUaBridge] OPC UA Client updated field: " << ctx->fieldName 
              << " for instance: " << ctx->instanceColor << std::endl;
}

DdsToOpcUaBridge::DdsToOpcUaBridge(const config::DdsToOpcUaBridgeConfig& config,
                                 const config::OpcUaServerConfig& serverConfig,
                                 const config::DomainParticipantConfig& dpConfig)
    : bridgeConfig_(config), serverConfig_(serverConfig), dpConfig_(dpConfig) {}

DdsToOpcUaBridge::~DdsToOpcUaBridge() {
    stop();
    if (server_) {
        UA_Server_delete(server_);
        server_ = nullptr;
    }
}

bool DdsToOpcUaBridge::initialize() {
    std::cout << "[DdsToOpcUaBridge] Initializing embedded open62541 server on port " 
              << serverConfig_.port << "..." << std::endl;

    server_ = UA_Server_new();
    UA_ServerConfig* config = UA_Server_getConfig(server_);
    UA_ServerConfig_setMinimal(config, serverConfig_.port, nullptr);

    builder_ = std::make_unique<AddressSpaceBuilder>(server_);

    // Initialize Cyclone DDS Participant
    try {
        std::cout << "[DdsToOpcUaBridge] Initializing DDS DomainParticipant (domain " 
                  << dpConfig_.domain_id << ")..." << std::endl;
        participant_ = dds::domain::DomainParticipant(dpConfig_.domain_id);
    } catch (const std::exception& e) {
        std::cerr << "[DdsToOpcUaBridge] Failed to create DomainParticipant: " << e.what() << std::endl;
        return false;
    }

    // Build AddressSpace according to Chapter 9
    for (const auto& domainCfg : bridgeConfig_.domains) {
        UA_NodeId domainNodeId = builder_->createDomainNode(dpConfig_.domain_id);

        for (const auto& topicCfg : domainCfg.topics) {
            setupShapeTopicBridge(domainNodeId, topicCfg);
        }
    }

    return true;
}

void DdsToOpcUaBridge::setupShapeTopicBridge(const UA_NodeId& domainNodeId, const config::DdsTopicConfig& topicCfg) {
    std::cout << "[DdsToOpcUaBridge] Setting up Topic in AddressSpace: " << topicCfg.name << std::endl;
    UA_NodeId topicNodeId = builder_->createTopicNode(domainNodeId, topicCfg.name, topicCfg.registered_type_name, true);

    try {
        shapeTopic_ = dds::topic::Topic<ShapeType>(participant_, topicCfg.name);

        if (topicCfg.read_access_enabled) {
            dds::sub::Subscriber sub(participant_);
            dds::sub::qos::DataReaderQos drQos = sub.default_datareader_qos();
            drQos << dds::core::policy::Reliability::Reliable();
            drQos << dds::core::policy::Durability::TransientLocal();
            drQos << dds::core::policy::History::KeepLast(10);
            shapeReader_ = dds::sub::DataReader<ShapeType>(sub, shapeTopic_, drQos);
        }

        if (topicCfg.write_access_enabled) {
            dds::pub::Publisher pub(participant_);
            dds::pub::qos::DataWriterQos dwQos = pub.default_datawriter_qos();
            dwQos << dds::core::policy::Reliability::Reliable();
            dwQos << dds::core::policy::Durability::TransientLocal();
            shapeWriter_ = dds::pub::DataWriter<ShapeType>(pub, shapeTopic_, dwQos);
        }

        // Setup pre-registered instances (§10.3.2.3.2)
        std::vector<std::pair<std::string, const UA_DataType*>> fields = {
            {"color", &UA_TYPES[UA_TYPES_STRING]},
            {"x", &UA_TYPES[UA_TYPES_INT32]},
            {"y", &UA_TYPES[UA_TYPES_INT32]},
            {"shapesize", &UA_TYPES[UA_TYPES_INT32]}
        };

        for (const auto& prereg : topicCfg.preregistered_instances) {
            std::cout << "[DdsToOpcUaBridge] Pre-registering instance: " << prereg.name << std::endl;
            InstanceNodeInfo info = builder_->createInstanceNode(topicNodeId, topicCfg.name, prereg.name, fields, "color");
            
            // Set initial field values
            builder_->updateStringField(info.field_node_ids["color"], prereg.name);
            auto itX = prereg.field_values.find("x");
            if (itX != prereg.field_values.end()) {
                builder_->updateInt32Field(info.field_node_ids["x"], std::stoi(itX->second));
            }
            auto itY = prereg.field_values.find("y");
            if (itY != prereg.field_values.end()) {
                builder_->updateInt32Field(info.field_node_ids["y"], std::stoi(itY->second));
            }
            auto itSize = prereg.field_values.find("shapesize");
            if (itSize != prereg.field_values.end()) {
                builder_->updateInt32Field(info.field_node_ids["shapesize"], std::stoi(itSize->second));
            }

            // Set initial SampleInfo (§9.3.4.1)
            builder_->updateSampleInfo(info, "NOT_READ", "NEW", "ALIVE", 0, 0, prereg.name, true);

            std::lock_guard<std::mutex> lock(instancesMutex_);
            shapeInstances_[prereg.name] = info;
        }

    } catch (const std::exception& e) {
        std::cerr << "[DdsToOpcUaBridge] DDS setup error: " << e.what() << std::endl;
    }
}

void DdsToOpcUaBridge::handleDdsSample(const ShapeType& sample, const dds::sub::SampleInfo& info, const std::string& topicName) {
    std::string key = sample.color();
    std::lock_guard<std::mutex> lock(instancesMutex_);

    auto it = shapeInstances_.find(key);
    if (it == shapeInstances_.end()) {
        // Dynamically create instance node on discovery (§9.3.4.1)
        std::vector<std::pair<std::string, const UA_DataType*>> fields = {
            {"color", &UA_TYPES[UA_TYPES_STRING]},
            {"x", &UA_TYPES[UA_TYPES_INT32]},
            {"y", &UA_TYPES[UA_TYPES_INT32]},
            {"shapesize", &UA_TYPES[UA_TYPES_INT32]}
        };

        UA_NodeId topicNodeId = UA_NODEID_STRING_ALLOC(1, topicName.c_str());
        InstanceNodeInfo newInfo = builder_->createInstanceNode(topicNodeId, topicName, key, fields, "color");
        shapeInstances_[key] = newInfo;
        it = shapeInstances_.find(key);
    }

    // Update SampleInfo (§9.3.4.2)
    std::string sampleStateStr = (info.state().sample_state() == dds::sub::status::SampleState::read()) ? "READ" : "NOT_READ";
    std::string viewStateStr = (info.state().view_state() == dds::sub::status::ViewState::new_view()) ? "NEW" : "NOT_NEW";
    std::string instanceStateStr = "ALIVE";
    if (info.state().instance_state() == dds::sub::status::InstanceState::not_alive_disposed()) {
        instanceStateStr = "NOT_ALIVE_DISPOSED";
    } else if (info.state().instance_state() == dds::sub::status::InstanceState::not_alive_no_writers()) {
        instanceStateStr = "NOT_ALIVE_NO_WRITERS";
    }

    int64_t sec = info.timestamp().sec();
    uint32_t nsec = info.timestamp().nanosec();
    std::string handleStr = key;

    builder_->updateSampleInfo(it->second, sampleStateStr, viewStateStr, instanceStateStr, sec, nsec, handleStr, info.valid());

    // If valid data, update payload fields (§9.3.4.2)
    if (info.valid()) {
        builder_->updateStringField(it->second.field_node_ids["color"], sample.color());
        builder_->updateInt32Field(it->second.field_node_ids["x"], sample.x());
        builder_->updateInt32Field(it->second.field_node_ids["y"], sample.y());
        builder_->updateInt32Field(it->second.field_node_ids["shapesize"], sample.shapesize());
    }
}

void DdsToOpcUaBridge::processDdsEventsOnce() {
    if (shapeReader_ == dds::core::null) return;

    try {
        auto samples = shapeReader_.take();
        for (const auto& s : samples) {
            handleDdsSample(s.data(), s.info(), shapeTopic_.name());
        }
    } catch (const std::exception& e) {
        // e.g. timeout or no data
    }
}

void DdsToOpcUaBridge::start() {
    if (running_) return;
    running_ = true;

    // Start OPC UA Server background thread
    serverThread_ = std::thread([this]() {
        runServerLoop();
    });

    // Start DDS poll/wait thread
    ddsPollThread_ = std::thread([this]() {
        while (running_) {
            processDdsEventsOnce();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::cout << "[DdsToOpcUaBridge] Started successfully." << std::endl;
}

void DdsToOpcUaBridge::runServerLoop() {
    UA_StatusCode st = UA_Server_run_startup(server_);
    if (st != UA_STATUSCODE_GOOD) {
        std::cerr << "[DdsToOpcUaBridge] Failed to startup server: " << UA_StatusCode_name(st) << std::endl;
        return;
    }
    std::cout << "[DdsToOpcUaBridge] OPC UA Server listening on opc.tcp://0.0.0.0:" 
              << serverConfig_.port << std::endl;

    while (running_) {
        UA_Server_run_iterate(server_, true);
    }
    UA_Server_run_shutdown(server_);
}

void DdsToOpcUaBridge::stop() {
    if (!running_) return;
    running_ = false;

    if (ddsPollThread_.joinable()) {
        ddsPollThread_.join();
    }
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    std::cout << "[DdsToOpcUaBridge] Stopped." << std::endl;
}

} // namespace dds_to_opcua
} // namespace gateway
