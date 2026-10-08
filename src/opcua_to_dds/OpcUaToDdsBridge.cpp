#include "gateway/opcua_to_dds/OpcUaToDdsBridge.hpp"
#include "gateway/types/OpcUaHelpers.hpp"
#include <iostream>
#include <chrono>

namespace gateway {
namespace opcua_to_dds {

static void handler_dataChangeNotification(UA_Client *client, UA_UInt32 subId, void *subContext,
                                           UA_UInt32 monId, void *monContext,
                                           UA_DataValue *value) {
    (void)client;
    (void)subId;
    (void)subContext;
    (void)monId;
    if (!monContext || !value || !value->hasValue) return;

    auto* ctx = static_cast<MonitoredItemContext*>(monContext);
    ctx->bridge->onDataChange(ctx->inputName, ctx->dataItemName, value);
}

OpcUaToDdsBridge::OpcUaToDdsBridge(const config::OpcUaToDdsBridgeConfig& config,
                                 const std::vector<config::OpcUaConnectionConfig>& connections,
                                 const std::vector<config::DomainParticipantConfig>& participants)
    : bridgeConfig_(config) {
    for (const auto& conn : connections) {
        connections_[conn.name] = conn;
    }
    for (const auto& p : participants) {
        participants_[p.name] = p;
    }
}

OpcUaToDdsBridge::~OpcUaToDdsBridge() {
    stop();
    for (auto& pair : clients_) {
        if (pair.second) {
            UA_Client_disconnect(pair.second);
            UA_Client_delete(pair.second);
        }
    }
    clients_.clear();
}

bool OpcUaToDdsBridge::initialize() {
    std::cout << "[OpcUaToDdsBridge] Initializing bridge: " << bridgeConfig_.name << std::endl;

    for (const auto& subCfg : bridgeConfig_.subscriptions) {
        // 1. Initialize DDS Outputs
        for (const auto& ddsOut : subCfg.dds_outputs) {
            std::cout << "[OpcUaToDdsBridge] Setting up DDS Output topic: " << ddsOut.topic_name << std::endl;
            auto itPart = participants_.find(ddsOut.domain_participant_ref);
            int32_t domainId = (itPart != participants_.end()) ? itPart->second.domain_id : 0;

            if (participant_ == dds::core::null) {
                participant_ = dds::domain::DomainParticipant(domainId);
            }

            shapeTopic_ = dds::topic::Topic<ShapeType>(participant_, ddsOut.topic_name);

            dds::pub::Publisher pub(participant_);
            dds::pub::qos::DataWriterQos dwQos = pub.default_datawriter_qos();
            dwQos << dds::core::policy::Reliability::Reliable();
            shapeWriter_ = dds::pub::DataWriter<ShapeType>(pub, shapeTopic_, dwQos);

            // Initialize buffer with default constant values
            ShapeType defaultSample;
            for (const auto& assign : subCfg.assignments) {
                if (assign.dds_output_ref == ddsOut.name) {
                    for (const auto& f : assign.field_assignments) {
                        if (f.is_constant) {
                            if (f.dds_output_field_ref == "color") {
                                defaultSample.color(f.constant_value);
                            } else if (f.dds_output_field_ref == "x") {
                                defaultSample.x(std::stoi(f.constant_value));
                            } else if (f.dds_output_field_ref == "y") {
                                defaultSample.y(std::stoi(f.constant_value));
                            } else if (f.dds_output_field_ref == "shapesize") {
                                defaultSample.shapesize(std::stoi(f.constant_value));
                            }
                        }
                    }
                }
            }
            outputBuffers_[ddsOut.topic_name] = defaultSample;
        }

        // 2. Connect to OPC UA Servers and create Subscriptions
        for (const auto& inputCfg : subCfg.opcua_inputs) {
            auto itConn = connections_.find(inputCfg.opcua_connection_ref);
            if (itConn == connections_.end()) {
                std::cerr << "[OpcUaToDdsBridge] Connection not found: " << inputCfg.opcua_connection_ref << std::endl;
                continue;
            }

            UA_Client* client = UA_Client_new();
            UA_ClientConfig_setDefault(UA_Client_getConfig(client));

            std::cout << "[OpcUaToDdsBridge] Connecting to OPC UA Server: " << itConn->second.server_endpoint_url << std::endl;
            UA_StatusCode status = UA_Client_connect(client, itConn->second.server_endpoint_url.c_str());
            if (status != UA_STATUSCODE_GOOD) {
                std::cerr << "[OpcUaToDdsBridge] Failed to connect to server: " << UA_StatusCode_name(status) 
                          << " (will retry during runtime)" << std::endl;
            }
            clients_[inputCfg.name] = client;

            if (status == UA_STATUSCODE_GOOD) {
                // Create subscription (§8.4.1.1)
                UA_CreateSubscriptionRequest req = UA_CreateSubscriptionRequest_default();
                req.requestedPublishingInterval = inputCfg.requested_publishing_interval_ms;
                req.requestedLifetimeCount = inputCfg.requested_lifetime_count;
                req.requestedMaxKeepAliveCount = inputCfg.requested_max_keepalive_count;
                UA_CreateSubscriptionResponse resp = UA_Client_Subscriptions_create(client, req, nullptr, nullptr, nullptr);

                if (resp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
                    uint32_t subId = resp.subscriptionId;

                    // Create MonitoredItems (§8.4.1.2)
                    for (const auto& item : inputCfg.data_items) {
                        UA_NodeId nodeId;
                        if (item.is_string_id) {
                            nodeId = UA_NODEID_STRING_ALLOC(item.namespace_index, item.string_identifier.c_str());
                        } else {
                            nodeId = UA_NODEID_NUMERIC(item.namespace_index, item.numeric_identifier);
                        }

                        auto itemCtx = std::make_unique<MonitoredItemContext>();
                        itemCtx->bridge = this;
                        itemCtx->inputName = inputCfg.name;
                        itemCtx->dataItemName = item.name;
                        itemCtx->clientHandle = 1000 + static_cast<uint32_t>(itemContexts_.size());

                        UA_MonitoredItemCreateRequest monReq = UA_MonitoredItemCreateRequest_default(nodeId);
                        monReq.requestedParameters.samplingInterval = item.sampling_interval_ms;
                        monReq.requestedParameters.queueSize = item.queue_size;
                        monReq.requestedParameters.discardOldest = item.discard_oldest;
                        monReq.requestedParameters.clientHandle = itemCtx->clientHandle;

                        UA_MonitoredItemCreateResult monResp = UA_Client_MonitoredItems_createDataChange(
                            client, subId, UA_TIMESTAMPSTORETURN_BOTH,
                            monReq, itemCtx.get(), &handler_dataChangeNotification, nullptr);

                        if (monResp.statusCode == UA_STATUSCODE_GOOD) {
                            std::cout << "[OpcUaToDdsBridge] Successfully monitored item: " << item.name << std::endl;
                        } else {
                            std::cerr << "[OpcUaToDdsBridge] Failed to monitor item: " << item.name 
                                      << " error: " << UA_StatusCode_name(monResp.statusCode) << std::endl;
                        }

                        itemContexts_.push_back(std::move(itemCtx));
                        UA_NodeId_clear(&nodeId);
                    }
                }
            }
        }
    }

    return true;
}

void OpcUaToDdsBridge::onDataChange(const std::string& inputName, const std::string& itemName, const UA_DataValue* value) {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    // Find all assignments mapped to this input and dataItem (§8.4.3.2.1)
    for (const auto& subCfg : bridgeConfig_.subscriptions) {
        for (const auto& assign : subCfg.assignments) {
            if (assign.opcua_input_ref == inputName) {
                for (const auto& f : assign.field_assignments) {
                    if (!f.is_constant && f.data_item_ref == itemName) {
                        // Find target output topic
                        for (const auto& ddsOut : subCfg.dds_outputs) {
                            if (ddsOut.name == assign.dds_output_ref) {
                                auto& sample = outputBuffers_[ddsOut.topic_name];
                                
                                // Assign value based on type
                                if (UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_INT32])) {
                                    int32_t val = *reinterpret_cast<int32_t*>(value->value.data);
                                    if (f.dds_output_field_ref == "x") sample.x(val);
                                    else if (f.dds_output_field_ref == "y") sample.y(val);
                                    else if (f.dds_output_field_ref == "shapesize") sample.shapesize(val);
                                } else if (UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_DOUBLE])) {
                                    int32_t val = static_cast<int32_t>(*reinterpret_cast<double*>(value->value.data));
                                    if (f.dds_output_field_ref == "x") sample.x(val);
                                    else if (f.dds_output_field_ref == "y") sample.y(val);
                                    else if (f.dds_output_field_ref == "shapesize") sample.shapesize(val);
                                } else if (UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_STRING])) {
                                    UA_String* uStr = reinterpret_cast<UA_String*>(value->value.data);
                                    std::string valStr = types::fromUaString(*uStr);
                                    if (f.dds_output_field_ref == "color") sample.color(valStr);
                                }

                                // Publish updated sample over DDS! (§8.4.3)
                                if (shapeWriter_ != dds::core::null) {
                                    shapeWriter_.write(sample);
                                    std::cout << "[OpcUaToDdsBridge] Published sample to DDS Topic [" << ddsOut.topic_name 
                                              << "]: color=" << sample.color() << ", x=" << sample.x() 
                                              << ", y=" << sample.y() << ", shapesize=" << sample.shapesize() << std::endl;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void OpcUaToDdsBridge::start() {
    if (running_) return;
    running_ = true;

    clientThread_ = std::thread([this]() {
        runClientLoop();
    });

    std::cout << "[OpcUaToDdsBridge] Started." << std::endl;
}

void OpcUaToDdsBridge::runClientLoop() {
    while (running_) {
        for (auto& pair : clients_) {
            if (pair.second) {
                UA_Client_run_iterate(pair.second, 20);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void OpcUaToDdsBridge::stop() {
    if (!running_) return;
    running_ = false;

    if (clientThread_.joinable()) {
        clientThread_.join();
    }
    std::cout << "[OpcUaToDdsBridge] Stopped." << std::endl;
}

} // namespace opcua_to_dds
} // namespace gateway
