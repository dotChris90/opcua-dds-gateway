cmake_minimum_required(VERSION 3.20)
project(opcua_dds_gateway LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies provided by Conan
find_package(open62541 REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)
find_package(CycloneDDS REQUIRED)
find_package(pugixml REQUIRED)

set(GATEWAY_SOURCES
    src/model/ShapeType.cpp
    src/config/ConfigParser.cpp
    src/dds_to_opcua/AddressSpaceBuilder.cpp
    src/dds_to_opcua/DdsToOpcUaBridge.cpp
    src/opcua_to_dds/OpcUaToDdsBridge.cpp
    src/core/Gateway.cpp
)

add_library(gateway_core STATIC ${GATEWAY_SOURCES})
target_include_directories(gateway_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(gateway_core PUBLIC
    open62541::open62541
    CycloneDDS-CXX::CycloneDDS-CXX
    CycloneDDS::CycloneDDS
    pugixml::pugixml
)

# Gateway CLI executable
add_executable(opcua_dds_gateway src/main.cpp)
target_link_libraries(opcua_dds_gateway PRIVATE gateway_core)

# Tests executable
add_executable(gateway_tests tests/test_gateway.cpp)
target_link_libraries(gateway_tests PRIVATE gateway_core)

# DDS Shape Publisher simulation tool for UaExpert live testing
add_executable(dds_shape_publisher src/tools/dds_shape_publisher.cpp)
target_link_libraries(dds_shape_publisher PRIVATE gateway_core)
