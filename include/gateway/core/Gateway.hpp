#pragma once

#include "gateway/config/GatewayConfig.hpp"
#include "gateway/config/ConfigParser.hpp"
#include "gateway/dds_to_opcua/DdsToOpcUaBridge.hpp"
#include "gateway/opcua_to_dds/OpcUaToDdsBridge.hpp"

#include <string>
#include <vector>
#include <memory>
#include <atomic>

namespace gateway {
namespace core {

class Gateway {
public:
    Gateway();
    ~Gateway();

    bool loadConfigFromFile(const std::string& configFile);
    bool loadConfigFromString(const std::string& configXml);

    bool initialize();
    void start();
    void stop();
    void run(); // Blocks until interrupted

    const config::GatewayConfig& getConfig() const { return config_; }

private:
    config::GatewayConfig config_;
    std::vector<std::unique_ptr<dds_to_opcua::DdsToOpcUaBridge>> ddsToUaBridges_;
    std::vector<std::unique_ptr<opcua_to_dds::OpcUaToDdsBridge>> uaToDdsBridges_;
    std::atomic<bool> isRunning_{false};
};

} // namespace core
} // namespace gateway
