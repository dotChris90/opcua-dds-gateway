#pragma once

#include "gateway/config/GatewayConfig.hpp"
#include <string>

namespace gateway {
namespace config {

class ConfigParser {
public:
    static bool parseFromFile(const std::string& filepath, GatewayConfig& outConfig);
    static bool parseFromString(const std::string& xmlContent, GatewayConfig& outConfig);
};

} // namespace config
} // namespace gateway
