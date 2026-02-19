/*
 * reflector.h: Drop-in scene explorer & profiler for C++ applications
 *
 * Embeds a CivetWeb HTTP server exposing a JSON REST API compatible with
 * the Reflector UI (Vue-based scene explorer). Inherit from reflector::Server,
 * implement three virtual methods, and you're done.
 *
 * Profiling:
 *   Call REFLECTOR_FRAME() once per frame, REFLECTOR_PROFILE("name") in scopes
 *   you want to measure. The server aggregates per-frame zone timings and
 *   exposes them via /api/profile for real-time visualization.
 *
 * Usage:
 *   1. Add civetweb.c to your build (compiled as C).
 *   2. Add lib/, lib/vendor/civetweb/, lib/vendor/nlohmann/ to include paths.
 *   3. In exactly ONE .cpp file:
 *        #define REFLECTOR_IMPLEMENTATION
 *        #include "reflector.h"
 *   4. In all other files, just #include "reflector.h" normally.
 *
 * MIT License: see repository root for full text.
 */

#ifndef REFLECTOR_H
#define REFLECTOR_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// ---------------------------------------------------------------------------
// Configuration: override before including if needed
// ---------------------------------------------------------------------------

// Number of frames kept in the ring buffer (default ~10s at 60fps)
#ifndef REFLECTOR_PROFILE_RING_SIZE
#define REFLECTOR_PROFILE_RING_SIZE 600
#endif

// Max zone entries per frame (zones beyond this are silently dropped)
#ifndef REFLECTOR_MAX_ZONES_PER_FRAME
#define REFLECTOR_MAX_ZONES_PER_FRAME 256
#endif

// Minimum zone duration in microseconds to record (0 = record everything)
#ifndef REFLECTOR_MIN_ZONE_US
#define REFLECTOR_MIN_ZONE_US 0
#endif

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace reflector {

struct PerfMetrics {
    float fps;
    float frameTimeMs;
    int entityCount;
};

struct SceneNode {
    uintptr_t id;
    uintptr_t parentId; // 0 for root nodes
    std::string type;
    std::string name; // empty if unnamed
};

enum class PropertyType { Float,
    Int,
    String,
    Color,
    Points2D };

struct Property {
    std::string name;
    PropertyType type;
    nlohmann::json value;

    static Property Float(std::string name, float v)
    {
        return { std::move(name), PropertyType::Float, v };
    }
    static Property Int(std::string name, int v)
    {
        return { std::move(name), PropertyType::Int, v };
    }
    static Property String(std::string name, std::string v)
    {
        return { std::move(name), PropertyType::String, std::move(v) };
    }
    static Property Color(std::string name, std::string hex)
    {
        return { std::move(name), PropertyType::Color, std::move(hex) };
    }
    static Property Points2D(std::string name, std::vector<std::pair<float, float>> pts)
    {
        nlohmann::json arr = nlohmann::json::array();
        for (auto& [x, y] : pts)
            arr.push_back({ x, y });
        return { std::move(name), PropertyType::Points2D, std::move(arr) };
    }
};

struct EntityInfo {
    uintptr_t id;
    std::string type;
    std::string name; // empty if unnamed
    std::vector<Property> properties;
};

// ---------------------------------------------------------------------------
// Profiling data structures
// ---------------------------------------------------------------------------

struct ZoneRecord {
    uint16_t nameId; // index into Profiler::nameTable_
    uint16_t depth;
    uint32_t durationUs;
};

struct FrameBuffer {
    uint64_t startUs = 0; // absolute timestamp (microseconds since epoch)
    uint64_t durationUs = 0; // total frame time
    uint32_t count = 0; // number of zones recorded this frame
    ZoneRecord zones[REFLECTOR_MAX_ZONES_PER_FRAME];

    void clear()
    {
        startUs = 0;
        durationUs = 0;
        count = 0;
    }
};

// ---------------------------------------------------------------------------
// Profiler: manages the frame ring buffer and zone name table
// ---------------------------------------------------------------------------

class Profiler {
public:
    Profiler();

    // Called by REFLECTOR_FRAME() — advances to a new frame slot.
    void beginFrame();

    // Register a zone name, returns a stable ID. Thread-safe; after warmup
    // (first few frames) the lock is never contended because all names exist.
    uint16_t registerName(const char* name);

    // Record a completed zone into the current frame. Called by ProfileScope
    // destructor. Only writes if duration >= minZoneUs.
    void recordZone(uint16_t nameId, uint16_t depth, uint32_t durationUs);

    // Current nesting depth (per-thread via thread_local in the impl).
    static uint16_t& currentDepth();

    // Read access for the HTTP handler — returns JSON for /api/profile.
    // Reads completed frames (not the one currently being written).
    nlohmann::json toJson() const;

    uint32_t minZoneUs = REFLECTOR_MIN_ZONE_US;

private:
    FrameBuffer ring_[REFLECTOR_PROFILE_RING_SIZE];
    std::atomic<uint32_t> writeHead_ { 0 };
    uint64_t prevFrameStartUs_ = 0;

    // Zone name table: nameId -> string
    std::vector<std::string> nameTable_;
    std::unordered_map<std::string, uint16_t> nameIndex_;
    mutable std::mutex nameMutex_;

    // Per-zone EMA baselines (indexed by nameId, maintained on read)
    mutable std::vector<double> ema_;
    mutable bool emaInitialized_ = false;

    uint64_t nowUs() const;
};

// ---------------------------------------------------------------------------
// ProfileScope: RAII zone timer
// ---------------------------------------------------------------------------

class ProfileScope {
public:
    ProfileScope(Profiler& profiler, uint16_t nameId)
        : profiler_(profiler)
        , nameId_(nameId)
        , depth_(Profiler::currentDepth()++)
        , startUs_(std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
                  .count())
    {
    }

    ~ProfileScope()
    {
        auto endUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                         .count();
        auto dur = static_cast<uint32_t>(endUs - startUs_);
        Profiler::currentDepth()--;
        profiler_.recordZone(nameId_, depth_, dur);
    }

    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;

private:
    Profiler& profiler_;
    uint16_t nameId_;
    uint16_t depth_;
    uint64_t startUs_;
};

} // namespace reflect

// Forward-declare CivetWeb opaque type at global scope
struct mg_context;

namespace reflector {

namespace detail {
    struct ServerAccess;
}

class Server {
public:
    explicit Server(int port = 7700);
    virtual ~Server();

    void start();
    void stop();

    bool isRunning() const { return ctx_ != nullptr; }

    // Access the profiler for macros.
    Profiler& profiler() { return profiler_; }

protected:
    virtual PerfMetrics onGetPerf() = 0;
    virtual std::vector<SceneNode> onGetScene() = 0;
    virtual std::optional<EntityInfo> onGetEntity(uintptr_t id) = 0;

private:
    friend struct detail::ServerAccess;
    int port_;
    ::mg_context* ctx_ = nullptr;
    Profiler profiler_;
};

} // namespace reflector

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------

// Helper to generate unique variable names
#define REFLECTOR_CONCAT_(a, b) a##b
#define REFLECTOR_CONCAT(a, b) REFLECTOR_CONCAT_(a, b)

// REFLECTOR_FRAME(): call once per frame. Advances the ring buffer.
// `server` is a reflector::Server& or reflector::Server*.
#define REFLECTOR_FRAME(server)          \
    do {                                 \
        auto& _rp = (server).profiler(); \
        _rp.beginFrame();                \
    } while (0)

// REFLECTOR_PROFILE(server, name): profile a scope. `name` is a string literal.
// Creates an RAII guard that records the zone on scope exit.
#define REFLECTOR_PROFILE(server, name)                                                           \
    static uint16_t REFLECTOR_CONCAT(_rz_id_, __LINE__) = (server).profiler().registerName(name); \
    reflector::ProfileScope REFLECTOR_CONCAT(_rz_scope_, __LINE__)(                               \
        (server).profiler(), REFLECTOR_CONCAT(_rz_id_, __LINE__))

#endif // REFLECTOR_H

// ===========================================================================
// Implementation: include only when REFLECTOR_IMPLEMENTATION is defined
// ===========================================================================

#ifdef REFLECTOR_IMPLEMENTATION
#ifndef REFLECTOR_IMPLEMENTATION_GUARD
#define REFLECTOR_IMPLEMENTATION_GUARD

#include <civetweb.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace reflector {

// ---------------------------------------------------------------------------
// Profiler implementation
// ---------------------------------------------------------------------------

Profiler::Profiler()
{
    nameTable_.reserve(64);
    for (auto& f : ring_)
        f.clear();
}

uint64_t Profiler::nowUs() const
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void Profiler::beginFrame()
{
    auto now = nowUs();

    // Finalize previous frame's duration
    uint32_t prev = writeHead_.load(std::memory_order_relaxed);
    if (prevFrameStartUs_ != 0) {
        ring_[prev].durationUs = now - prevFrameStartUs_;
    }

    // Advance to next slot
    uint32_t next = (prev + 1) % REFLECTOR_PROFILE_RING_SIZE;
    ring_[next].clear();
    ring_[next].startUs = now;
    prevFrameStartUs_ = now;

    // Publish the new write head (readers skip this slot)
    writeHead_.store(next, std::memory_order_release);

    // Reset per-thread depth
    currentDepth() = 0;
}

uint16_t Profiler::registerName(const char* name)
{
    std::lock_guard<std::mutex> lock(nameMutex_);
    auto it = nameIndex_.find(name);
    if (it != nameIndex_.end())
        return it->second;

    uint16_t id = static_cast<uint16_t>(nameTable_.size());
    nameTable_.emplace_back(name);
    nameIndex_[name] = id;
    return id;
}

void Profiler::recordZone(uint16_t nameId, uint16_t depth, uint32_t durationUs)
{
    if (durationUs < minZoneUs)
        return;

    uint32_t head = writeHead_.load(std::memory_order_relaxed);
    auto& frame = ring_[head];
    uint32_t idx = frame.count;
    if (idx >= REFLECTOR_MAX_ZONES_PER_FRAME)
        return; // frame is full

    frame.zones[idx] = { nameId, depth, durationUs };
    frame.count = idx + 1;
}

uint16_t& Profiler::currentDepth()
{
    thread_local uint16_t depth = 0;
    return depth;
}

nlohmann::json Profiler::toJson() const
{
    uint32_t head = writeHead_.load(std::memory_order_acquire);

    // Snapshot the name table
    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lock(nameMutex_);
        names = nameTable_;
    }

    if (names.empty()) {
        return { { "zones", nlohmann::json::array() } };
    }

    // Initialize EMA if needed
    if (!emaInitialized_ || ema_.size() < names.size()) {
        ema_.resize(names.size(), 0.0);
        emaInitialized_ = true;
    }

    // Per-zone history: accumulate per-frame durations from completed frames.
    // Walk backwards from (head-1) which is the most recently completed frame.
    const uint32_t ringSize = REFLECTOR_PROFILE_RING_SIZE;
    const uint32_t maxFrames = ringSize - 1; // skip the active write slot

    // Per-zone: vector of per-frame ms values
    std::vector<std::vector<float>> histories(names.size());
    for (auto& h : histories)
        h.reserve(maxFrames);

    // Temporary per-frame accumulator
    std::vector<float> frameAccum(names.size(), 0.0f);

    for (uint32_t i = 1; i <= maxFrames; ++i) {
        uint32_t idx = (head + ringSize - i) % ringSize;
        const auto& frame = ring_[idx];
        if (frame.startUs == 0)
            break; // uninitialised slot

        // Zero the accumulator
        std::fill(frameAccum.begin(), frameAccum.end(), 0.0f);

        // Sum durations per zone for this frame (only depth-0 zones to
        // avoid double-counting parent+child)
        for (uint32_t z = 0; z < frame.count; ++z) {
            const auto& rec = frame.zones[z];
            if (rec.nameId < names.size() && rec.depth == 0) {
                frameAccum[rec.nameId] += rec.durationUs / 1000.0f;
            }
        }

        // Prepend to each zone's history (we're walking newest-first)
        for (size_t z = 0; z < names.size(); ++z) {
            histories[z].push_back(frameAccum[z]);
        }
    }

    // Reverse histories so index 0 = oldest
    for (auto& h : histories)
        std::reverse(h.begin(), h.end());

    // Update EMAs
    const double alpha = 0.002;
    for (size_t z = 0; z < names.size(); ++z) {
        for (float val : histories[z]) {
            double clamped = std::min(static_cast<double>(val), ema_[z] * 2.5);
            if (ema_[z] < 0.001)
                ema_[z] = val; // bootstrap
            else
                ema_[z] += alpha * (clamped - ema_[z]);
        }
    }

    // Build JSON response
    nlohmann::json zones = nlohmann::json::array();
    for (size_t z = 0; z < names.size(); ++z) {
        // Skip zones that never recorded anything
        bool allZero = true;
        for (float v : histories[z]) {
            if (v > 0.0f) {
                allZero = false;
                break;
            }
        }
        if (allZero && histories[z].empty())
            continue;

        zones.push_back({
            { "name", names[z] },
            { "parent", nullptr },
            { "history", histories[z] },
            { "ema", std::round(ema_[z] * 1000.0) / 1000.0 },
        });
    }

    return { { "zones", zones } };
}

namespace detail {

    // ---------------------------------------------------------------------------
    // JSON serialization helpers
    // ---------------------------------------------------------------------------

    static const char* propertyTypeName(PropertyType t)
    {
        switch (t) {
        case PropertyType::Float:
            return "float";
        case PropertyType::Int:
            return "int";
        case PropertyType::String:
            return "string";
        case PropertyType::Color:
            return "color";
        case PropertyType::Points2D:
            return "points2d";
        default:
            return "unknown";
        }
    }

    static nlohmann::json perfToJson(const PerfMetrics& m)
    {
        return {
            { "fps", m.fps },
            { "frameTimeMs", m.frameTimeMs },
            { "entityCount", m.entityCount },
        };
    }

    static nlohmann::json propertyToJson(const Property& p)
    {
        return {
            { "name", p.name },
            { "type", propertyTypeName(p.type) },
            { "value", p.value },
        };
    }

    static nlohmann::json entityToJson(const EntityInfo& e)
    {
        nlohmann::json props = nlohmann::json::array();
        for (auto& p : e.properties) {
            props.push_back(propertyToJson(p));
        }
        return {
            { "id", std::to_string(e.id) },
            { "type", e.type },
            { "name", e.name.empty() ? nlohmann::json(nullptr) : nlohmann::json(e.name) },
            { "properties", props },
        };
    }

    // Build nested tree JSON from a flat list of SceneNodes.
    // Nodes with parentId == 0 become roots.
    static nlohmann::json buildSceneTree(const std::vector<SceneNode>& flat)
    {
        // Each node's JSON representation (without children filled yet)
        struct TreeEntry {
            nlohmann::json obj;
            uintptr_t parentId;
        };

        // Preserve insertion order: use vector + map for lookup
        std::unordered_map<uintptr_t, size_t> indexById;
        std::vector<TreeEntry> entries;
        entries.reserve(flat.size());

        for (auto& n : flat) {
            nlohmann::json obj = {
                { "id", std::to_string(n.id) },
                { "type", n.type },
                { "name", n.name.empty() ? nlohmann::json(nullptr) : nlohmann::json(n.name) },
                { "children", nlohmann::json::array() },
            };
            indexById[n.id] = entries.size();
            entries.push_back({ std::move(obj), n.parentId });
        }

        // Attach children to parents (iterate in reverse so deeper nodes are
        // attached before their parents get serialized).
        // We'll collect root indices.
        std::vector<size_t> rootIndices;
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].parentId == 0) {
                rootIndices.push_back(i);
            }
        }

        // Build children bottom-up: iterate all nodes, attach to parent
        // We need a proper approach: collect children lists, then recursively build.
        std::unordered_map<uintptr_t, std::vector<size_t>> childrenOf;
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].parentId != 0) {
                childrenOf[entries[i].parentId].push_back(i);
            }
        }

        // Recursive build
        std::function<nlohmann::json(size_t)> build = [&](size_t idx) -> nlohmann::json {
            auto& e = entries[idx];
            nlohmann::json node = std::move(e.obj);
            auto it = childrenOf.find(flat[idx].id);
            if (it != childrenOf.end()) {
                for (size_t ci : it->second) {
                    node["children"].push_back(build(ci));
                }
            }
            return node;
        };

        nlohmann::json roots = nlohmann::json::array();
        for (size_t ri : rootIndices) {
            roots.push_back(build(ri));
        }

        return { { "entities", roots } };
    }

    // ---------------------------------------------------------------------------
    // HTTP helpers
    // ---------------------------------------------------------------------------

    static void sendJson(struct mg_connection* conn, int status, const nlohmann::json& j)
    {
        std::string body = j.dump();
        mg_printf(conn,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %zu\r\n"
            "Connection: keep-alive\r\n"
            "\r\n",
            status, (status == 200 ? "OK" : "Not Found"),
            body.size());
        mg_write(conn, body.data(), body.size());
    }

    static void sendCorsOptions(struct mg_connection* conn)
    {
        mg_printf(conn,
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Content-Length: 0\r\n"
            "\r\n");
    }

    // ---------------------------------------------------------------------------
    // CivetWeb request handlers (C callbacks)
    // ---------------------------------------------------------------------------

    // Friend accessor: bridges C callbacks to protected virtual methods
    struct ServerAccess {
        static PerfMetrics getPerf(Server* s) { return s->onGetPerf(); }
        static std::vector<SceneNode> getScene(Server* s) { return s->onGetScene(); }
        static std::optional<EntityInfo> getEntity(Server* s, uintptr_t id) { return s->onGetEntity(id); }
        static Profiler& getProfiler(Server* s) { return s->profiler_; }
    };

    static int handlePerf(struct mg_connection* conn, void* cbdata)
    {
        auto* req = mg_get_request_info(conn);
        if (std::strcmp(req->request_method, "OPTIONS") == 0) {
            sendCorsOptions(conn);
            return 204;
        }
        auto* server = static_cast<Server*>(cbdata);
        auto metrics = ServerAccess::getPerf(server);
        sendJson(conn, 200, perfToJson(metrics));
        return 200;
    }

    static int handleScene(struct mg_connection* conn, void* cbdata)
    {
        auto* req = mg_get_request_info(conn);
        if (std::strcmp(req->request_method, "OPTIONS") == 0) {
            sendCorsOptions(conn);
            return 204;
        }
        auto* server = static_cast<Server*>(cbdata);
        auto nodes = ServerAccess::getScene(server);
        sendJson(conn, 200, buildSceneTree(nodes));
        return 200;
    }

    static int handleEntity(struct mg_connection* conn, void* cbdata)
    {
        auto* req = mg_get_request_info(conn);
        if (std::strcmp(req->request_method, "OPTIONS") == 0) {
            sendCorsOptions(conn);
            return 204;
        }

        // Parse entity ID from URI: /api/entity/<id>
        const char* uri = req->local_uri;
        const char* idStr = std::strrchr(uri, '/');
        if (!idStr || *(idStr + 1) == '\0') {
            sendJson(conn, 404, { { "error", "Missing entity ID" } });
            return 404;
        }
        idStr++; // skip '/'

        uintptr_t id = 0;
        auto [ptr, ec] = std::from_chars(idStr, idStr + std::strlen(idStr), id);
        if (ec != std::errc {}) {
            sendJson(conn, 404, { { "error", "Invalid entity ID" } });
            return 404;
        }

        auto* server = static_cast<Server*>(cbdata);
        auto entity = ServerAccess::getEntity(server, id);
        if (!entity) {
            sendJson(conn, 404, { { "error", "Entity not found" } });
            return 404;
        }

        sendJson(conn, 200, entityToJson(*entity));
        return 200;
    }

    static int handleProfile(struct mg_connection* conn, void* cbdata)
    {
        auto* req = mg_get_request_info(conn);
        if (std::strcmp(req->request_method, "OPTIONS") == 0) {
            sendCorsOptions(conn);
            return 204;
        }
        auto* server = static_cast<Server*>(cbdata);
        sendJson(conn, 200, ServerAccess::getProfiler(server).toJson());
        return 200;
    }

} // namespace detail

// ---------------------------------------------------------------------------
// Server implementation
// ---------------------------------------------------------------------------

Server::Server(int port)
    : port_(port)
{
}

Server::~Server()
{
    stop();
}

void Server::start()
{
    if (ctx_)
        return;

    mg_init_library(0);

    std::string portStr = std::to_string(port_);
    const char* options[] = {
        "listening_ports",
        portStr.c_str(),
        "num_threads",
        "2",
        nullptr,
    };

    struct mg_callbacks callbacks = {};
    ctx_ = mg_start(&callbacks, nullptr, options);
    if (!ctx_) {
        std::fprintf(stderr, "[reflector] Failed to start server on port %d\n", port_);
        return;
    }

    // Register handlers: pass `this` as cbdata
    mg_set_request_handler(ctx_, "/api/perf", detail::handlePerf, this);
    mg_set_request_handler(ctx_, "/api/scene", detail::handleScene, this);
    mg_set_request_handler(ctx_, "/api/entity/", detail::handleEntity, this);
    mg_set_request_handler(ctx_, "/api/profile", detail::handleProfile, this);

    std::fprintf(stdout, "[reflector] Server running on http://localhost:%d\n", port_);
}

void Server::stop()
{
    if (ctx_) {
        mg_stop(ctx_);
        mg_exit_library();
        ctx_ = nullptr;
    }
}

} // namespace reflector

#endif // REFLECTOR_IMPLEMENTATION_GUARD
#endif // REFLECTOR_IMPLEMENTATION
