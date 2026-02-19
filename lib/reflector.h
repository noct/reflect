/*
 * reflector.h: Drop-in scene explorer server for C++ applications
 *
 * Embeds a CivetWeb HTTP server exposing a JSON REST API compatible with
 * the Reflector UI (Vue-based scene explorer). Inherit from reflector::Server,
 * implement three virtual methods, and you're done.
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

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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

} // namespace reflector

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

protected:
    virtual PerfMetrics onGetPerf() = 0;
    virtual std::vector<SceneNode> onGetScene() = 0;
    virtual std::optional<EntityInfo> onGetEntity(uintptr_t id) = 0;

private:
    friend struct detail::ServerAccess;
    int port_;
    ::mg_context* ctx_ = nullptr;
};

} // namespace reflector

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
