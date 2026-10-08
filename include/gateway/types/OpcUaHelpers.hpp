#pragma once

#include <open62541/server.h>
#include <open62541/client.h>
#include <open62541/plugin/log_stdout.h>

#include <string>
#include <cstdint>
#include <chrono>

namespace gateway {
namespace types {

// Convert DDS epoch (1970-01-01) seconds & nanoseconds to OPC UA DateTime (100ns intervals since 1601-01-01)
inline UA_DateTime ddsTimeToOpcUaDateTime(int64_t sec, uint32_t nanosec) {
    // 11644473600 seconds between 1601-01-01 and 1970-01-01
    constexpr int64_t SECONDS_BETWEEN_1601_AND_1970 = 11644473600LL;
    int64_t totalSec = sec + SECONDS_BETWEEN_1601_AND_1970;
    int64_t opcUaTime = (totalSec * 10000000LL) + (nanosec / 100LL);
    return (UA_DateTime)opcUaTime;
}

// Convert OPC UA DateTime to DDS seconds and nanoseconds
inline void opcUaDateTimeToDdsTime(UA_DateTime uaTime, int64_t& sec, uint32_t& nanosec) {
    constexpr int64_t SECONDS_BETWEEN_1601_AND_1970 = 11644473600LL;
    int64_t totalSec = uaTime / 10000000LL;
    sec = totalSec - SECONDS_BETWEEN_1601_AND_1970;
    nanosec = static_cast<uint32_t>((uaTime % 10000000LL) * 100);
}

// DDS SampleInfo state mappings according to OMG Table 9.52
inline const char* sampleStateToString(uint32_t sampleState) {
    // 1 = READ, 2 = NOT_READ
    return (sampleState == 1) ? "READ" : "NOT_READ";
}

inline const char* viewStateToString(uint32_t viewState) {
    // 1 = NEW, 2 = NOT_NEW
    return (viewState == 1) ? "NEW" : "NOT_NEW";
}

inline const char* instanceStateToString(uint32_t instanceState) {
    // 1 = ALIVE, 2 = NOT_ALIVE_DISPOSED, 4 = NOT_ALIVE_NO_WRITERS
    switch (instanceState) {
        case 1: return "ALIVE";
        case 2: return "NOT_ALIVE_DISPOSED";
        case 4: return "NOT_ALIVE_NO_WRITERS";
        default: return "UNKNOWN";
    }
}

// Helper to convert std::string to UA_String
inline UA_String toUaString(const std::string& str) {
    return UA_STRING_ALLOC(str.c_str());
}

// Helper to convert UA_String to std::string
inline std::string fromUaString(const UA_String& uaStr) {
    if (uaStr.data == nullptr || uaStr.length == 0) return "";
    return std::string(reinterpret_cast<const char*>(uaStr.data), uaStr.length);
}

} // namespace types
} // namespace gateway
