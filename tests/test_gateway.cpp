#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

#include "gateway/config/ConfigParser.hpp"
#include "gateway/types/OpcUaHelpers.hpp"
#include "gateway/dds_to_opcua/DdsToOpcUaBridge.hpp"
#include "gateway/model/ShapeType.hpp"

using namespace gateway;

void test_type_mappings() {
    std::cout << "[TEST] Running test_type_mappings..." << std::endl;

    // Test time conversions
    int64_t ddsSec = 1700000000LL;
    uint32_t ddsNano = 500000000U;
    UA_DateTime uaDt = types::ddsTimeToOpcUaDateTime(ddsSec, ddsNano);

    int64_t recoveredSec = 0;
    uint32_t recoveredNano = 0;
    types::opcUaDateTimeToDdsTime(uaDt, recoveredSec, recoveredNano);

    assert(recoveredSec == ddsSec);
    assert(recoveredNano == ddsNano);

    // Test state string conversions (OMG Table 9.52)
    assert(std::string(types::sampleStateToString(1)) == "READ");
    assert(std::string(types::sampleStateToString(2)) == "NOT_READ");
    assert(std::string(types::viewStateToString(1)) == "NEW");
    assert(std::string(types::viewStateToString(2)) == "NOT_NEW");
    assert(std::string(types::instanceStateToString(1)) == "ALIVE");
    assert(std::string(types::instanceStateToString(2)) == "NOT_ALIVE_DISPOSED");
    assert(std::string(types::instanceStateToString(4)) == "NOT_ALIVE_NO_WRITERS");

    std::cout << "  -> test_type_mappings PASSED!" << std::endl;
}

void test_config_parser() {
    std::cout << "[TEST] Running test_config_parser..." << std::endl;

    config::GatewayConfig cfg;
    bool ok = config::ConfigParser::parseFromFile("configs/gateway_shape_example.xml", cfg);
    assert(ok);
    assert(cfg.gateway_name == "OMG_Shape_Gateway");
    assert(!cfg.domain_participants.empty());
    assert(cfg.domain_participants[0].name == "DomainParticipant0");
    assert(cfg.domain_participants[0].domain_id == 0);

    assert(!cfg.dds_to_opcua_bridges.empty());
    const auto& dds2ua = cfg.dds_to_opcua_bridges[0];
    assert(dds2ua.name == "MyDds2OpcUaBridge");
    assert(!dds2ua.domains.empty());
    assert(!dds2ua.domains[0].topics.empty());

    const auto& circleTopic = dds2ua.domains[0].topics[0];
    assert(circleTopic.name == "Circle");
    assert(circleTopic.registered_type_name == "ShapeType");
    assert(circleTopic.preregistered_instances.size() == 2);
    assert(circleTopic.preregistered_instances[0].name == "BLUE");
    assert(circleTopic.preregistered_instances[0].field_values.at("x") == "100");

    assert(!cfg.opcua_to_dds_bridges.empty());
    const auto& ua2dds = cfg.opcua_to_dds_bridges[0];
    assert(!ua2dds.subscriptions.empty());
    assert(!ua2dds.subscriptions[0].assignments.empty());

    std::cout << "  -> test_config_parser PASSED!" << std::endl;
}

void test_dds_to_opcua_address_space_and_sample_update() {
    std::cout << "[TEST] Running test_dds_to_opcua_address_space_and_sample_update..." << std::endl;

    config::DdsToOpcUaBridgeConfig bridgeCfg;
    bridgeCfg.name = "TestBridge";
    bridgeCfg.opcua_server_ref = "TestServer";

    config::DdsDomainConfig domCfg;
    domCfg.domain_participant_ref = "DP0";

    config::DdsTopicConfig topCfg;
    topCfg.name = "TestCircle";
    topCfg.registered_type_name = "ShapeType";
    topCfg.read_access_enabled = true;
    topCfg.write_access_enabled = true;

    config::PreregisteredInstanceConfig prereg;
    prereg.name = "BLUE";
    prereg.field_values["x"] = "10";
    prereg.field_values["y"] = "20";
    prereg.field_values["shapesize"] = "30";
    topCfg.preregistered_instances.push_back(prereg);

    domCfg.topics.push_back(topCfg);
    bridgeCfg.domains.push_back(domCfg);

    config::OpcUaServerConfig srvCfg;
    srvCfg.name = "TestServer";
    srvCfg.port = 4845; // Test port

    config::DomainParticipantConfig dpCfg;
    dpCfg.name = "DP0";
    dpCfg.domain_id = 0;

    dds_to_opcua::DdsToOpcUaBridge bridge(bridgeCfg, srvCfg, dpCfg);
    bool initOk = bridge.initialize();
    assert(initOk);

    // Verify initial values in OPC UA Server
    UA_Server* server = bridge.getServer();
    assert(server != nullptr);

    UA_Variant val;
    UA_Variant_init(&val);

    // Check pre-registered BLUE instance x variable: TestCircle.Instance_BLUE.x
    UA_NodeId xNodeId = UA_NODEID_STRING_ALLOC(1, "TestCircle.Instance_BLUE.x");
    UA_StatusCode readStatus = UA_Server_readValue(server, xNodeId, &val);
    assert(readStatus == UA_STATUSCODE_GOOD);
    assert(*(UA_Int32*)val.data == 10);
    UA_Variant_clear(&val);
    UA_NodeId_clear(&xNodeId);

    // Now write a sample to DDS and test ingestion
    dds::domain::DomainParticipant participant(0);
    dds::topic::Topic<ShapeType> topic(participant, "TestCircle");
    dds::pub::Publisher pub(participant);
    dds::pub::DataWriter<ShapeType> writer(pub, topic);

    ShapeType updateSample("BLUE", 88, 99, 42);
    writer.write(updateSample);

    // Wait and process DDS events
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bridge.processDdsEventsOnce();
    }

    // Verify OPC UA Server variable was updated to 88!
    xNodeId = UA_NODEID_STRING_ALLOC(1, "TestCircle.Instance_BLUE.x");
    readStatus = UA_Server_readValue(server, xNodeId, &val);
    assert(readStatus == UA_STATUSCODE_GOOD);
    UA_Int32 currentX = *(UA_Int32*)val.data;
    assert(currentX == 88);
    UA_Variant_clear(&val);
    UA_NodeId_clear(&xNodeId);

    // Verify shapesize was updated to 42
    UA_NodeId sizeNodeId = UA_NODEID_STRING_ALLOC(1, "TestCircle.Instance_BLUE.shapesize");
    readStatus = UA_Server_readValue(server, sizeNodeId, &val);
    assert(readStatus == UA_STATUSCODE_GOOD);
    assert(*(UA_Int32*)val.data == 42);
    UA_Variant_clear(&val);
    UA_NodeId_clear(&sizeNodeId);

    // Verify SampleInfo ValidData is true
    UA_NodeId validDataNodeId = UA_NODEID_STRING_ALLOC(1, "TestCircle.Instance_BLUE.ValidData");
    readStatus = UA_Server_readValue(server, validDataNodeId, &val);
    assert(readStatus == UA_STATUSCODE_GOOD);
    assert(*(UA_Boolean*)val.data == true);
    UA_Variant_clear(&val);
    UA_NodeId_clear(&validDataNodeId);

    std::cout << "  -> test_dds_to_opcua_address_space_and_sample_update PASSED!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Running OPC UA / DDS Gateway Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        test_type_mappings();
        test_config_parser();
        test_dds_to_opcua_address_space_and_sample_update();

        std::cout << "========================================" << std::endl;
        std::cout << "  ALL TESTS PASSED SUCCESSFULLY!        " << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << std::endl;
        return 1;
    }
}
