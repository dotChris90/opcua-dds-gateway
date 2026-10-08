#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#include "gateway/core/Gateway.hpp"

std::atomic<bool> g_shutdownRequested{false};

void signalHandler(int sig) {
    (void)sig;
    std::cout << "\n[Gateway] Interrupted by user. Shutting down gracefully..." << std::endl;
    g_shutdownRequested = true;
}

int main(int argc, char* argv[]) {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  OMG OPC UA / DDS Gateway (formal/20-01-01) v1.0.0       " << std::endl;
    std::cout << "==========================================================" << std::endl;

    std::string configFile = "configs/gateway_shape_example.xml";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            configFile = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [-c <config_path>]" << std::endl;
            std::cout << "Default config: configs/gateway_shape_example.xml" << std::endl;
            return 0;
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    gateway::core::Gateway gateway;
    if (!gateway.loadConfigFromFile(configFile)) {
        std::cerr << "[Gateway] Could not load configuration file: " << configFile << std::endl;
        return 1;
    }

    if (!gateway.initialize()) {
        std::cerr << "[Gateway] Initialization failed!" << std::endl;
        return 1;
    }

    gateway.start();
    std::cout << "[Gateway] Running. Press Ctrl+C to terminate..." << std::endl;

    while (!g_shutdownRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    gateway.stop();
    std::cout << "[Gateway] Exited cleanly." << std::endl;
    return 0;
}
