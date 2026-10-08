#include "gateway/config/ConfigParser.hpp"
#include <pugixml.hpp>
#include <iostream>

namespace gateway {
namespace config {

static void parseGatewayNode(const pugi::xml_node& gwNode, GatewayConfig& config) {
    config.gateway_name = gwNode.attribute("name").as_string("DefaultGateway");

    // OPC UA Connections
    for (pugi::xml_node connNode : gwNode.children("opcua_connection")) {
        OpcUaConnectionConfig conn;
        conn.name = connNode.attribute("name").as_string();
        conn.server_endpoint_url = connNode.attribute("server_endpoint_url").as_string();
        if (conn.server_endpoint_url.empty()) {
            conn.server_endpoint_url = connNode.child("server_endpoint_url").text().as_string();
        }
        conn.timeout_ms = connNode.child("timeout").text().as_uint(5000);
        config.opcua_connections.push_back(conn);
    }

    // OPC UA Servers
    for (pugi::xml_node srvNode : gwNode.children("opcua_server")) {
        OpcUaServerConfig srv;
        srv.name = srvNode.attribute("name").as_string();
        srv.port = static_cast<uint16_t>(srvNode.attribute("port").as_uint(4840));
        srv.endpoint_url = srvNode.child("endpoint_url").text().as_string("opc.tcp://0.0.0.0:4840");
        config.opcua_servers.push_back(srv);
    }

    // DDS Domain Participants
    for (pugi::xml_node dpNode : gwNode.children("domain_participant")) {
        DomainParticipantConfig dp;
        dp.name = dpNode.attribute("name").as_string();
        dp.domain_id = dpNode.attribute("domain_id").as_int(0);
        config.domain_participants.push_back(dp);
    }

    // DDS to OPC UA Bridges
    for (pugi::xml_node bridgeNode : gwNode.children("dds_to_opcua_bridge")) {
        DdsToOpcUaBridgeConfig bridge;
        bridge.name = bridgeNode.attribute("name").as_string();
        bridge.opcua_server_ref = bridgeNode.attribute("opcua_server_ref").as_string();

        for (pugi::xml_node domainNode : bridgeNode.children("domain")) {
            DdsDomainConfig domain;
            domain.domain_participant_ref = domainNode.attribute("domain_participant_ref").as_string();

            for (pugi::xml_node topicNode : domainNode.children("topic")) {
                DdsTopicConfig topic;
                topic.name = topicNode.attribute("name").as_string();
                topic.registered_type_name = topicNode.child("registered_type_name").text().as_string();
                
                pugi::xml_node readAccess = topicNode.child("read_access");
                if (readAccess) {
                    topic.read_access_enabled = readAccess.child("enabled").text().as_bool(true);
                }

                pugi::xml_node writeAccess = topicNode.child("write_access");
                if (writeAccess) {
                    topic.write_access_enabled = writeAccess.child("enabled").text().as_bool(true);
                    for (pugi::xml_node instNode : writeAccess.child("preregistered_instances").children("instance")) {
                        PreregisteredInstanceConfig inst;
                        inst.name = instNode.attribute("name").as_string();
                        for (pugi::xml_node fieldNode : instNode.children("field")) {
                            inst.field_values[fieldNode.attribute("name").as_string()] = fieldNode.text().as_string();
                        }
                        topic.preregistered_instances.push_back(inst);
                    }
                }

                domain.topics.push_back(topic);
            }
            bridge.domains.push_back(domain);
        }
        config.dds_to_opcua_bridges.push_back(bridge);
    }

    // OPC UA to DDS Bridges
    for (pugi::xml_node bridgeNode : gwNode.children("opcua_to_dds_bridge")) {
        OpcUaToDdsBridgeConfig bridge;
        bridge.name = bridgeNode.attribute("name").as_string();

        for (pugi::xml_node subNode : bridgeNode.children("subscription")) {
            SubscriptionConfig sub;
            sub.name = subNode.attribute("name").as_string();

            // OPC UA Inputs
            for (pugi::xml_node inputNode : subNode.children("opcua_input")) {
                OpcUaInputConfig input;
                input.name = inputNode.attribute("name").as_string();
                input.opcua_connection_ref = inputNode.attribute("opcua_connection_ref").as_string();

                pugi::xml_node protoNode = inputNode.child("subscription_protocol");
                if (protoNode) {
                    input.requested_publishing_interval_ms = protoNode.child("requested_publishing_interval").text().as_double(100.0);
                    input.requested_lifetime_count = protoNode.child("requested_lifetime_count").text().as_uint(3000);
                    input.requested_max_keepalive_count = protoNode.child("requested_max_keep_alive_count").text().as_uint(1000);
                }

                pugi::xml_node itemsNode = inputNode.child("monitored_items");
                for (pugi::xml_node dataItemNode : itemsNode.children("data_item")) {
                    OpcUaDataItemConfig item;
                    item.name = dataItemNode.attribute("name").as_string();

                    pugi::xml_node nodeId = dataItemNode.child("node_id");
                    if (nodeId) {
                        item.namespace_index = static_cast<uint16_t>(nodeId.child("namespace_index").text().as_uint(1));
                        if (nodeId.child("string_identifier")) {
                            item.string_identifier = nodeId.child("string_identifier").text().as_string();
                            item.is_string_id = true;
                        } else if (nodeId.child("numeric_identifier")) {
                            item.numeric_identifier = nodeId.child("numeric_identifier").text().as_uint();
                            item.is_string_id = false;
                        }
                    }

                    item.attribute_id = dataItemNode.child("attribute_id").text().as_string("VALUE");
                    item.sampling_interval_ms = dataItemNode.child("sampling_interval").text().as_double(100.0);
                    item.queue_size = dataItemNode.child("queue_size").text().as_uint(1);
                    item.discard_oldest = dataItemNode.child("discard_oldest").text().as_bool(true);

                    input.data_items.push_back(item);
                }
                sub.opcua_inputs.push_back(input);
            }

            // DDS Outputs
            for (pugi::xml_node outNode : subNode.children("dds_output")) {
                DdsOutputConfig ddsOut;
                ddsOut.name = outNode.attribute("name").as_string();
                ddsOut.domain_participant_ref = outNode.attribute("domain_participant_ref").as_string();
                ddsOut.topic_name = outNode.child("topic_name").text().as_string();
                ddsOut.registered_type_name = outNode.child("registered_type_name").text().as_string();
                sub.dds_outputs.push_back(ddsOut);
            }

            // Mappings / Assignments
            pugi::xml_node mapNode = subNode.child("mapping");
            if (mapNode) {
                for (pugi::xml_node assignNode : mapNode.children("assignment")) {
                    AssignmentConfig assign;
                    assign.dds_output_ref = assignNode.attribute("dds_output_ref").as_string();
                    assign.opcua_input_ref = assignNode.attribute("opcua_input_ref").as_string();

                    for (pugi::xml_node fieldNode : assignNode.children("field")) {
                        FieldAssignmentConfig fAssign;
                        fAssign.dds_output_field_ref = fieldNode.attribute("dds_output_field_ref").as_string();
                        if (fieldNode.child("data_item")) {
                            fAssign.data_item_ref = fieldNode.child("data_item").attribute("data_item_ref").as_string();
                            fAssign.is_constant = false;
                        } else if (fieldNode.child("value")) {
                            fAssign.constant_value = fieldNode.child("value").text().as_string();
                            fAssign.is_constant = true;
                        }
                        assign.field_assignments.push_back(fAssign);
                    }
                    sub.assignments.push_back(assign);
                }
            }

            bridge.subscriptions.push_back(sub);
        }
        config.opcua_to_dds_bridges.push_back(bridge);
    }
}

bool ConfigParser::parseFromFile(const std::string& filepath, GatewayConfig& outConfig) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    if (!result) {
        std::cerr << "[ConfigParser] Failed to parse XML file: " << filepath << " (" << result.description() << ")" << std::endl;
        return false;
    }

    pugi::xml_node root = doc.child("dds");
    if (!root) {
        root = doc.first_child();
    }
    
    pugi::xml_node gwNode = root.child("ddsopcua_gateway");
    if (!gwNode) {
        gwNode = root; // Allow root itself to be gateway or child
    }

    parseGatewayNode(gwNode, outConfig);
    return true;
}

bool ConfigParser::parseFromString(const std::string& xmlContent, GatewayConfig& outConfig) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlContent.c_str());
    if (!result) {
        std::cerr << "[ConfigParser] Failed to parse XML string: (" << result.description() << ")" << std::endl;
        return false;
    }

    pugi::xml_node root = doc.child("dds");
    if (!root) {
        root = doc.first_child();
    }

    pugi::xml_node gwNode = root.child("ddsopcua_gateway");
    if (!gwNode) {
        gwNode = root;
    }

    parseGatewayNode(gwNode, outConfig);
    return true;
}

} // namespace config
} // namespace gateway
