#define WLR_USE_UNSTABLE

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/ipc/s1/S1.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprutils/signal/Signal.hpp>

#include "GammaManager.hpp"

#include <functional>
#include <string>
#include <memory>
#include <sstream>
#include <vector>

inline HANDLE                      g_pluginHandle = nullptr;
inline std::unique_ptr<GammaManager> g_gammaManager;

inline Hyprutils::Signal::CHyprSignalListener g_monitorAdded;
inline Hyprutils::Signal::CHyprSignalListener g_monitorRemoved;

// ── hyprctl command: list ──────────────────────────────────────────────────
static std::string cmdList(IPC::Socket1::eOutputFormat format, std::string) {
    if (!g_gammaManager)
        return "error: gamma manager not initialized";

    if (format == IPC::Socket1::FORMAT_JSON)
        return g_gammaManager->getSettingsJson();
    else
        return g_gammaManager->getMonitorListText();
}

// ── hyprctl command: set ───────────────────────────────────────────────────
// hyprgamma:set <monitor> <key> <value>
static std::string cmdSet(IPC::Socket1::eOutputFormat, std::string args) {
    if (!g_gammaManager)
        return "error: gamma manager not initialized";

    // hyprctl passes the full command line including our prefix, e.g.
    // "hyprgamma:set DP-1 brightness 0.8"
    // Strip the "hyprgamma:set " prefix from the full request command.
    auto prefix = std::string("hyprgamma:set");
    if (args.starts_with(prefix))
        args = (args.size() > prefix.size() && args[prefix.size()] == ' ') ? args.substr(prefix.size() + 1) : "";

    std::istringstream ss(args);
    std::string monitor, key;
    float value = 0.0f;

    ss >> monitor;
    if (monitor.empty())
        return "error: missing monitor name";

    ss >> key;
    if (key.empty())
        return "error: missing key (brightness|contrast|gamma|gamma_r|gamma_g|gamma_b)";

    ss >> value;
    if (ss.fail())
        return "error: missing or invalid value";

    g_gammaManager->setParam(monitor, key, value);
    g_gammaManager->saveConfig();
    return "ok";
}

// ── hyprctl command: reset ─────────────────────────────────────────────────
// hyprgamma:reset [monitor]
static std::string cmdReset(IPC::Socket1::eOutputFormat, std::string args) {
    if (!g_gammaManager)
        return "error: gamma manager not initialized";

    auto prefix = std::string("hyprgamma:reset");
    if (args.starts_with(prefix))
        args = (args.size() > prefix.size() && args[prefix.size()] == ' ') ? args.substr(prefix.size() + 1) : "";

    if (args.empty()) {
        g_gammaManager->resetAll();
    } else {
        std::istringstream ss(args);
        std::string monitor;
        ss >> monitor;
        if (!monitor.empty())
            g_gammaManager->reset(monitor);
    }

    g_gammaManager->saveConfig();
    return "ok";
}

// ── hyprctl command: enable / disable ──────────────────────────────────────
static std::string cmdEnable(IPC::Socket1::eOutputFormat, std::string args) {
    if (!g_gammaManager)
        return "error: gamma manager not initialized";

    auto prefix = std::string("hyprgamma:enable");
    if (args.starts_with(prefix))
        args = (args.size() > prefix.size() && args[prefix.size()] == ' ') ? args.substr(prefix.size() + 1) : "";

    std::istringstream ss(args);
    std::string monitor;
    ss >> monitor;
    if (monitor.empty())
        return "error: missing monitor name";

    g_gammaManager->setEnabled(monitor, true);
    g_gammaManager->saveConfig();
    return "ok";
}

static std::string cmdDisable(IPC::Socket1::eOutputFormat, std::string args) {
    if (!g_gammaManager)
        return "error: gamma manager not initialized";

    auto prefix = std::string("hyprgamma:disable");
    if (args.starts_with(prefix))
        args = (args.size() > prefix.size() && args[prefix.size()] == ' ') ? args.substr(prefix.size() + 1) : "";

    std::istringstream ss(args);
    std::string monitor;
    ss >> monitor;
    if (monitor.empty())
        return "error: missing monitor name";

    g_gammaManager->setEnabled(monitor, false);
    g_gammaManager->saveConfig();
    return "ok";
}

// ── hyprctl command: save / reload ─────────────────────────────────────────
static std::string cmdSave(IPC::Socket1::eOutputFormat, std::string) {
    if (!g_gammaManager)
        return "error: gamma manager not initialized";
    return g_gammaManager->saveConfig() ? "ok" : "error: failed to save";
}

static std::string cmdReload(IPC::Socket1::eOutputFormat, std::string) {
    if (!g_gammaManager)
        return "error: gamma manager not initialized";
    g_gammaManager->loadConfig();
    g_gammaManager->applyAll();
    return "ok";
}

// ── dispatchers (for keybinds) ─────────────────────────────────────────────
static SDispatchResult dispatchReset(std::string) {
    if (g_gammaManager) {
        g_gammaManager->resetAll();
        g_gammaManager->saveConfig();
    }
    return {.passEvent = false, .success = true};
}

static SDispatchResult dispatchReload(std::string) {
    if (g_gammaManager) {
        g_gammaManager->loadConfig();
        g_gammaManager->applyAll();
    }
    return {.passEvent = false, .success = true};
}

// ═══════════════════════════════════════════════════════════════════════════
// Plugin API
// ═══════════════════════════════════════════════════════════════════════════
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    g_pluginHandle = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(g_pluginHandle,
            "[hyprgamma] Version mismatch (headers != running hyprland). Plugin may crash.",
            CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hyprgamma] Version mismatch");
    }

    g_gammaManager = std::make_unique<GammaManager>();

    // Load saved settings and apply
    g_gammaManager->loadConfig();
    g_gammaManager->applyAll();

    // Register hyprctl commands
    auto registerCmd = [&](const char* name, std::function<std::string(IPC::Socket1::eOutputFormat, std::string)> fn) {
        auto cmd = HyprlandAPI::registerHyprCtlCommand(g_pluginHandle, IPC::Socket1::SCommand{
            .name = name,
            .match = IPC::Socket1::COMMAND_MATCH_PREFIX,
            .handler = [fn](const IPC::Socket1::SRequest& request) -> IPC::Socket1::SResponse {
                return fn(request.format, request.command);
            },
        });
        if (!cmd) {
            HyprlandAPI::addNotification(g_pluginHandle,
                std::string("[hyprgamma] failed to register hyprctl command ") + name,
                CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        }
    };

    registerCmd("hyprgamma:list",    cmdList);
    registerCmd("hyprgamma:set",     cmdSet);
    registerCmd("hyprgamma:reset",   cmdReset);
    registerCmd("hyprgamma:enable",  cmdEnable);
    registerCmd("hyprgamma:disable", cmdDisable);
    registerCmd("hyprgamma:save",    cmdSave);
    registerCmd("hyprgamma:reload",  cmdReload);

    // Register dispatchers for keybinds
    HyprlandAPI::addDispatcherV2(g_pluginHandle, "hyprgamma:reset", dispatchReset);
    HyprlandAPI::addDispatcherV2(g_pluginHandle, "hyprgamma:reload", dispatchReload);

    // Listen for monitor hotplug events
    g_monitorAdded = Event::bus()->m_events.monitor.added.listen([](PHLMONITOR mon) {
        if (!g_gammaManager || !mon)
            return;
        g_gammaManager->applyAll(); // re-apply to new monitor too
    });

    // Notify ready
    HyprlandAPI::addNotification(g_pluginHandle,
        "[hyprgamma] Loaded! Use hyprctl hyprgamma:list",
        CHyprColor{0.2, 1.0, 0.2, 1.0}, 3000);

    return {
        .name = "hypr-gamma",
        .description = "Per-monitor brightness, contrast, and gamma control",
        .author = "donnie",
        .version = "1.0.0",
    };
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (g_gammaManager) {
        g_gammaManager->saveConfig();
        g_gammaManager->clearAll();
    }
    g_gammaManager.reset();
    g_monitorAdded.reset();
    g_monitorRemoved.reset();

    HyprlandAPI::addNotification(g_pluginHandle,
        "[hyprgamma] Unloaded.",
        CHyprColor{1.0, 0.5, 0.0, 1.0}, 3000);
}
