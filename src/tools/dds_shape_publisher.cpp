#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <dds/dds.hpp>
#include "gateway/model/ShapeType.hpp"

std::atomic<bool> g_stop{false};

void sigHandler(int) {
    g_stop = true;
}

int main(int argc, char* argv[]) {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  Cyclone DDS Shape Publisher (OMG ShapeType Demo)        " << std::endl;
    std::cout << "==========================================================" << std::endl;

    std::string color = "BLUE";
    int32_t domainId = 0;
    if (argc > 1) color = argv[1];

    std::signal(SIGINT, sigHandler);
    std::signal(SIGTERM, sigHandler);

    try {
        dds::domain::DomainParticipant participant(domainId);
        dds::topic::Topic<ShapeType> topic(participant, "Circle");
        dds::pub::Publisher pub(participant);

        dds::pub::qos::DataWriterQos qos = pub.default_datawriter_qos();
        qos << dds::core::policy::Reliability::Reliable();
        qos << dds::core::policy::Durability::TransientLocal();

        dds::pub::DataWriter<ShapeType> writer(pub, topic, qos);

        std::cout << "[DDS Publisher] Publishing live '" << color << "' circles on Domain " << domainId 
                  << " Topic 'Circle'..." << std::endl;
        std::cout << "[DDS Publisher] Watch changes live in UaExpert! Press Ctrl+C to terminate.\n" << std::endl;

        int32_t x = 100;
        int32_t y = 150;
        int32_t dx = 4;
        int32_t dy = 2;
        int32_t shapesize = 30;

        while (!g_stop) {
            x += dx;
            y += dy;
            if (x <= 10 || x >= 240) dx = -dx;
            if (y <= 10 || y >= 240) dy = -dy;

            ShapeType sample(color, x, y, shapesize);
            writer.write(sample);

            std::cout << "\r[RTPS Sample Sent] color=" << color 
                      << " | x=" << x << " | y=" << y << " | size=" << shapesize << "   " << std::flush;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "\n[DDS Publisher] Stopped." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "DDS Publisher Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
