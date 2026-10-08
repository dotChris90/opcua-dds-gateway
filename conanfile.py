from conan import ConanFile
from conan.tools.cmake import cmake_layout

class OpcUaDdsGatewayConan(ConanFile):
    name = "opcua_dds_gateway"
    version = "0.1.0"
    package_type = "application"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("open62541/1.4.6")
        self.requires("cyclonedds/0.10.5")
        self.requires("cyclonedds-cxx/0.10.5")
        self.requires("pugixml/1.14")
