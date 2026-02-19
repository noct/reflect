/*
 * Reflector: minimal example with profiling
 *
 * Build:
 *   gcc -std=c11 -DNO_SSL -DNO_CGI -Ivendor/civetweb -c vendor/civetweb/civetweb.c -o civetweb.o
 *   g++ -std=c++17 -I. -Ivendor -Ivendor/civetweb -DNO_SSL -DNO_CGI -c example.cpp -o example.o
 *   g++ example.o civetweb.o -lpthread -ldl -o reflector_example
 *
 * Run:
 *   ./reflector_example
 *   # Open the Reflector UI and switch to the Performance tab.
 */

#define REFLECTOR_IMPLEMENTATION
#include "reflector.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <thread>

static std::atomic<bool> g_running { true };

void onSignal(int) { g_running = false; }

// ---------------------------------------------------------------------------
// Example: a tiny fake game scene with profiled systems
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
        case 0xA200:
            return reflector::EntityInfo { id, "Camera", "MainCamera", {
                                                                           reflector::Property::Float("fov", 75.0f),
                                                                           reflector::Property::Float("near", 0.1f),
                                                                           reflector::Property::Float("far", 1000.0f),
                                                                       } };
        default:
            return std::nullopt;
        }
    }
};

// Simulate variable workloads with a busy-wait
static void busyWaitUs(int us)
{
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - start)
               .count()
        < us) {
    }
}

int main()
{
    std::signal(SIGINT, onSignal);

    MyGameServer server(7700);
    server.start();

    std::printf("Press Ctrl+C to stop.\n");
    std::printf("Simulating a game loop with profiled systems...\n");

    int frame = 0;
    while (g_running) {
        // Mark frame boundary
        REFLECTOR_FRAME(server);

        // Simulate game systems with varying cost
        {
            REFLECTOR_PROFILE(server, "Physics.Broadphase");
            busyWaitUs(1200 + std::rand() % 400);
        }
        {
            REFLECTOR_PROFILE(server, "Physics.Narrowphase");
            busyWaitUs(600 + std::rand() % 300);
        }
        {
            REFLECTOR_PROFILE(server, "Render.Culling");
            busyWaitUs(800 + std::rand() % 200);
        }
        {
            REFLECTOR_PROFILE(server, "Render.DrawCalls");
            busyWaitUs(5000 + std::rand() % 1000);
        }
        {
            REFLECTOR_PROFILE(server, "Render.PostFX");
            busyWaitUs(1000 + std::rand() % 400);
        }
        {
            REFLECTOR_PROFILE(server, "AI.Pathfinding");
            busyWaitUs(500 + std::rand() % 200);

            // Simulate an occasional spike every ~5 seconds
            if (frame % 300 == 299) {
                busyWaitUs(8000);
            }
        }
        {
            REFLECTOR_PROFILE(server, "Audio.Mix");
            busyWaitUs(300 + std::rand() % 100);
        }

        frame++;

        // Target ~60fps (subtract approximate work time)
        std::this_thread::sleep_for(std::chrono::milliseconds(6));
    }

    server.stop();
    std::printf("Stopped after %d frames.\n", frame);
    return 0;
}
