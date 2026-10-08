#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace gateway {
namespace config {

// Represents an OPC UA Client connection configuration (§10.3.1.2)
struct OpcUaConnectionConfig {
    std::string name;
    std::string server_endpoint_url;
    uint32_t timeout_ms = 5000;
};

// Represents an OPC UA Server configuration hosted by the gateway (§10.3.2.2)
struct OpcUaServerConfig {
    std::string name;
    uint16_t port = 4840;
    std::string endpoint_url = "opc.tcp://0.0.0.0:4840";
};

// Represents a DDS DomainParticipant configuration (§10.3.1.2)
struct DomainParticipantConfig {
    std::string name;
    int32_t domain_id = 0;
};

// Pre-registered instances configured for a DDS Topic (§10.3.2.3.2)
struct PreregisteredInstanceConfig {
    std::string name;
    std::map<std::string, std::string> field_values;
};

// DDS Topic exposed in OPC UA (§10.3.2.3.2)
struct DdsTopicConfig {
    std::string name;
    std::string registered_type_name;
    bool read_access_enabled = true;
    bool write_access_enabled = true;
    bool historical_access_enabled = false;
    std::vector<PreregisteredInstanceConfig> preregistered_instances;
};

// DDS Domain exposed in OPC UA (§10.3.2.3.1)
struct DdsDomainConfig {
    std::string domain_participant_ref;
    std::vector<DdsTopicConfig> topics;
};

// DDS to OPC UA Bridge Configuration (§9 & §10.3.2)
struct DdsToOpcUaBridgeConfig {
    std::string name;
    std::string opcua_server_ref;
    std::vector<DdsDomainConfig> domains;
};

// Monitored Item / DataItem definition for OPC UA input (§10.3.1.5.1.2)
struct OpcUaDataItemConfig {
    std::string name;
    uint16_t namespace_index = 1;
    std::string string_identifier;
    uint32_t numeric_identifier = 0;
    bool is_string_id = true;
    std::string attribute_id = "VALUE";
    double sampling_interval_ms = 100.0;
    uint32_t queue_size = 1;
    bool discard_oldest = true;
};

// OPC UA Input Subscription (§10.3.1.5.1)
struct OpcUaInputConfig {
    std::string name;
    std::string opcua_connection_ref;
    double requested_publishing_interval_ms = 100.0;
    uint32_t requested_lifetime_count = 3000;
    uint32_t requested_max_keepalive_count = 1000;
    std::vector<OpcUaDataItemConfig> data_items;
};

// DDS Output publication (§10.3.1.5.2)
struct DdsOutputConfig {
    std::string name;
    std::string domain_participant_ref;
    std::string topic_name;
    std::string registered_type_name;
};

// Field Assignment from OPC UA Item or Constant to DDS Field (§10.3.1.5.3)
struct FieldAssignmentConfig {
    std::string dds_output_field_ref;
    std::string data_item_ref;
    std::string constant_value;
    bool is_constant = false;
};

// Assignment linking DDS Output with OPC UA Input (§10.3.1.5.3)
struct AssignmentConfig {
    std::string dds_output_ref;
    std::string opcua_input_ref;
    std::vector<FieldAssignmentConfig> field_assignments;
};

// Subscription mapping block (§10.3.1.5)
struct SubscriptionConfig {
    std::string name;
    std::vector<OpcUaInputConfig> opcua_inputs;
    std::vector<DdsOutputConfig> dds_outputs;
    std::vector<AssignmentConfig> assignments;
};

// OPC UA to DDS Bridge Configuration (§8 & §10.3.1)
struct OpcUaToDdsBridgeConfig {
    std::string name;
    std::vector<SubscriptionConfig> subscriptions;
};

// Complete Gateway Configuration (§10)
struct GatewayConfig {
    std::string gateway_name;
    std::vector<OpcUaConnectionConfig> opcua_connections;
    std::vector<OpcUaServerConfig> opcua_servers;
    std::vector<DomainParticipantConfig> domain_participants;
    std::vector<DdsToOpcUaBridgeConfig> dds_to_opcua_bridges;
    std::vector<OpcUaToDdsBridgeConfig> opcua_to_dds_bridges;

    const OpcUaConnectionConfig* findConnection(const std::string& name) const {
        for (const auto& conn : opcua_connections) {
            if (conn.name == name) return &conn;
        }
        return nullptr;
    }

    const OpcUaServerConfig* findServer(const std::string& name) const {
        for (const auto& srv : opcua_servers) {
            if (srv.name == name) return &srv;
        }
        return nullptr;
    }

    const DomainParticipantConfig* findParticipant(const std::string& name) const {
        for (const auto& part : domain_participants) {
            if (part.name == name) return &part;
        }
        return nullptr;
    }
};

} // namespace config
} // namespace gateway
