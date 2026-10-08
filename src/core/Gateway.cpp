#include "gateway/core/Gateway.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace gateway {
namespace core {

Gateway::Gateway() {}

Gateway::~Gateway() {
    stop();
}

bool Gateway::loadConfigFromFile(const std::string& configFile) {
    std::cout << "[Gateway] Loading configuration from file: " << configFile << std::endl;
    return config::ConfigParser::parseFromFile(configFile, config_);
}

bool Gateway::loadConfigFromString(const std::string& configXml) {
    std::cout << "[Gateway] Loading configuration from XML string..." << std::endl;
    return config::ConfigParser::parseFromString(configXml, config_);
}

bool Gateway::initialize() {
    std::cout << "[Gateway] Initializing gateway: " << config_.gateway_name << std::endl;

    // 1. Initialize DDS to OPC UA Bridges
    for (const auto& bridgeCfg : config_.dds_to_opcua_bridges) {
        const auto* serverCfg = config_.findServer(bridgeCfg.opcua_server_ref);
        if (!serverCfg) {
            std::cerr << "[Gateway] Server reference not found: " << bridgeCfg.opcua_server_ref << std::endl;
            return false;
        }

        // Each domain references a domain_participant
        for (const auto& domainCfg : bridgeCfg.domains) {
            const auto* dpCfg = config_.findParticipant(domainCfg.domain_participant_ref);
            if (!dpCfg) {
                std::cerr << "[Gateway] Participant reference not found: " << domainCfg.domain_participant_ref << std::endl;
                return false;
            }

            auto bridge = std::make_unique<dds_to_opcua::DdsToOpcUaBridge>(bridgeCfg, *serverCfg, *dpCfg);
            if (!bridge->initialize()) {
                std::cerr << "[Gateway] Failed to initialize DDS to OPC UA bridge: " << bridgeCfg.name << std::endl;
                return false;
            }
            ddsToUaBridges_.push_back(std::move(bridge));
        }
    }

    // 2. Initialize OPC UA to DDS Bridges
    for (const auto& bridgeCfg : config_.opcua_to_dds_bridges) {
        auto bridge = std::make_unique<opcua_to_dds::OpcUaToDdsBridge>(
            bridgeCfg, config_.opcua_connections, config_.domain_participants);
        if (!bridge->initialize()) {
            std::cerr << "[Gateway] Failed to initialize OPC UA to DDS bridge: " << bridgeCfg.name << std::endl;
            return false;
        }
        uaToDdsBridges_.push_back(std::move(bridge));
    }

    std::cout << "[Gateway] Initialized successfully. Bridges active: DDS->UA (" 
              << ddsToUaBridges_.size() << "), UA->DDS (" << uaToDdsBridges_.size() << ")" << std::endl;
    return true;
}

void Gateway::start() {
    if (isRunning_) return;
    isRunning_ = true;

    for (auto& b : ddsToUaBridges_) {
        b->start();
    }
    for (auto& b : uaToDdsBridges_) {
        b->start();
    }

    std::cout << "[Gateway] Gateway started and processing traffic." << std::endl;
}

void Gateway::stop() {
    if (!isRunning_) return;
    isRunning_ = false;

    for (auto& b : uaToDdsBridges_) {
        b->stop();
    }
    for (auto& b : ddsToUaBridges_) {
        b->stop();
    }

    std::cout << "[Gateway] Gateway stopped." << std::endl;
}

void Gateway::run() {
    start();
    while (isRunning_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace core
} // namespace gateway
