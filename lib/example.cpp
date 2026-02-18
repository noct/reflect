/*
 * Reflector: minimal example
 *
 * Build:
 *   gcc -std=c11 -DNO_SSL -DNO_CGI -Ivendor/civetweb -c vendor/civetweb/civetweb.c -o civetweb.o
 *   g++ -std=c++17 -I. -Ivendor -Ivendor/civetweb -DNO_SSL -DNO_CGI -c example.cpp -o example.o
 *   g++ example.o civetweb.o -lpthread -ldl -o reflector_example
 *
 * Run:
 *   ./reflector_example
 *   # Then open http://localhost:7700/api/perf in a browser or curl it.
 */

#define REFLECTOR_IMPLEMENTATION
#include "reflector.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

static std::atomic<bool> g_running { true };

void onSignal(int) { g_running = false; }

// ---------------------------------------------------------------------------
// Example: a tiny fake game scene
// ---------------------------------------------------------------------------

class MyGameServer : public reflector::Server {
public:
    using Server::Server;

protected:
    reflector::PerfMetrics onGetPerf() override
    {
        return { 60.1f, 16.6f, 5 };
    }

    std::vector<reflector::SceneNode> onGetScene() override
    {
        // Flat list with parent IDs (the library builds the tree)
        return {
            { 0xA000, 0, "Transform", "Root" },
            { 0xA100, 0xA000, "Transform", "World" },
            { 0xA200, 0xA100, "Camera", "MainCamera" },
            { 0xA300, 0xA100, "Light", "Sun" },
            { 0xA400, 0xA000, "Canvas", "UI" },
        };
    }

    std::optional<reflector::EntityInfo> onGetEntity(uintptr_t id) override
    {
        switch (id) {
        case 0xA000:
            return reflector::EntityInfo { id, "Transform", "Root", {
                                                                        reflector::Property::Float("position.x", 0.0f),
                                                                        reflector::Property::Float("position.y", 0.0f),
                                                                        reflector::Property::Float("position.z", 0.0f),
                                                                    } };
        case 0xA100:
            return reflector::EntityInfo { id, "Transform", "World", {
                                                                         reflector::Property::Int("entityCount", 3),
                                                                         reflector::Property::String("tag", "world"),
                                                                     } };
        case 0xA200:
            return reflector::EntityInfo { id, "Camera", "MainCamera", {
                                                                           reflector::Property::Float("fov", 75.0f),
                                                                           reflector::Property::Float("near", 0.1f),
                                                                           reflector::Property::Float("far", 1000.0f),
                                                                           reflector::Property::Color("clearColor", "#1a1a2e"),
                                                                       } };
        case 0xA300:
            return reflector::EntityInfo { id, "Light", "Sun", {
                                                                   reflector::Property::String("lightType", "directional"),
                                                                   reflector::Property::Float("intensity", 1.2f),
                                                                   reflector::Property::Color("color", "#FFFDE7"),
                                                               } };
        case 0xA400:
            return reflector::EntityInfo { id, "Canvas", "UI", {
                                                                   reflector::Property::Int("enabled", 1),
                                                                   reflector::Property::String("renderMode", "screenSpace"),
                                                               } };
        default:
            return std::nullopt;
        }
    }
};

int main()
{
    std::signal(SIGINT, onSignal);

    MyGameServer server(7700);
    server.start();

    std::printf("Press Ctrl+C to stop.\n");
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.stop();
    std::printf("Stopped.\n");
    return 0;
}
