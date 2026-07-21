#include "GammaManager.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cstdlib>

using json = nlohmann::json;

GammaManager::GammaManager() = default;
GammaManager::~GammaManager() = default;

std::string GammaManager::getConfigPath() const {
    const char* configHome = getenv("XDG_CONFIG_HOME");
    if (configHome && configHome[0])
        return std::string(configHome) + "/hypr/hyprgamma.json";
    const char* home = getenv("HOME");
    if (home && home[0])
        return std::string(home) + "/.config/hypr/hyprgamma.json";
    return "/tmp/hyprgamma.json";
}

PHLMONITOR GammaManager::findMonitor(const std::string& name) const {
    for (auto& m : State::monitorState()->monitors()) {
        if (m && m->m_name == name)
            return m;
    }
    return nullptr;
}

std::vector<uint16_t> GammaManager::computeGammaTable(size_t gammaSize, const MonitorGammaSettings& settings) {
    std::vector<uint16_t> table(gammaSize * 3);

    if (!settings.enabled) {
        // identity ramp
        for (size_t i = 0; i < gammaSize; i++) {
            uint16_t val = (uint16_t)((double)i / (gammaSize - 1) * 65535.0);
            table[i * 3 + 0] = val;
            table[i * 3 + 1] = val;
            table[i * 3 + 2] = val;
        }
        return table;
    }

    for (size_t i = 0; i < gammaSize; i++) {
        double n = (double)i / (gammaSize - 1);

        double r = pow(n, settings.gammaR);
        double g = pow(n, settings.gammaG);
        double b = pow(n, settings.gammaB);

        r = (r - 0.5) * settings.contrast + 0.5;
        g = (g - 0.5) * settings.contrast + 0.5;
        b = (b - 0.5) * settings.contrast + 0.5;

        r *= settings.brightness;
        g *= settings.brightness;
        b *= settings.brightness;

        r = std::clamp(r, 0.0, 1.0);
        g = std::clamp(g, 0.0, 1.0);
        b = std::clamp(b, 0.0, 1.0);

        table[i * 3 + 0] = (uint16_t)(r * 65535.0);
        table[i * 3 + 1] = (uint16_t)(g * 65535.0);
        table[i * 3 + 2] = (uint16_t)(b * 65535.0);
    }

    return table;
}

void GammaManager::applyToMonitor(const std::string& name, const MonitorGammaSettings& settings) {
    auto mon = findMonitor(name);
    if (!mon || !mon->m_output)
        return;

    if (mon->m_output->getGammaSize() < 10)
        return;

    auto gammaTable = computeGammaTable(mon->m_output->getGammaSize(), settings);
    mon->m_output->state->setGammaLut(gammaTable);
    g_pHyprRenderer->damageMonitor(mon);
}

void GammaManager::applyToMonitor(const std::string& name) {
    auto it = m_monitorSettings.find(name);
    if (it == m_monitorSettings.end())
        return;
    applyToMonitor(name, it->second);
}

void GammaManager::applyAll() {
    for (auto& m : State::monitorState()->monitors()) {
        if (!m || !m->m_output)
            continue;

        auto it = m_monitorSettings.find(m->m_name);
        if (it == m_monitorSettings.end()) {
            // seed with defaults
            m_monitorSettings[m->m_name] = m_defaultSettings;
            applyToMonitor(m->m_name, m_defaultSettings);
        } else {
            applyToMonitor(m->m_name, it->second);
        }
    }
}

void GammaManager::applyToAllEnabled() {
    for (auto& m : State::monitorState()->monitors()) {
        if (!m || !m->m_output)
            continue;
        auto it = m_monitorSettings.find(m->m_name);
        if (it != m_monitorSettings.end() && it->second.enabled)
            applyToMonitor(m->m_name, it->second);
    }
}

void GammaManager::clearAll() {
    for (auto& m : State::monitorState()->monitors()) {
        if (!m || !m->m_output)
            continue;
        m->m_output->state->setGammaLut({});
    }
}

void GammaManager::setParam(const std::string& monitor, const std::string& key, float value) {
    auto it = m_monitorSettings.find(monitor);
    if (it == m_monitorSettings.end())
        return;

    auto& s = it->second;
    if (key == "brightness")        s.brightness = value;
    else if (key == "contrast")     s.contrast   = value;
    else if (key == "gamma") {      // "gamma" adjusts all three channels at once
        s.gammaR = value;
        s.gammaG = value;
        s.gammaB = value;
    }
    else if (key == "gamma_r")      s.gammaR     = value;
    else if (key == "gamma_g")      s.gammaG     = value;
    else if (key == "gamma_b")      s.gammaB     = value;

    applyToMonitor(monitor, s);
}

void GammaManager::setEnabled(const std::string& monitor, bool enabled) {
    auto it = m_monitorSettings.find(monitor);
    if (it == m_monitorSettings.end())
        return;
    it->second.enabled = enabled;
    if (enabled)
        applyToMonitor(monitor, it->second);
    else
        applyToMonitor(monitor, m_defaultSettings); // identity
}

void GammaManager::reset(const std::string& monitor) {
    auto it = m_monitorSettings.find(monitor);
    if (it == m_monitorSettings.end())
        return;
    it->second = m_defaultSettings;
    applyToMonitor(monitor, it->second);
}

void GammaManager::resetAll() {
    for (auto& [name, settings] : m_monitorSettings)
        settings = m_defaultSettings;
    applyAll();
}

void GammaManager::debugApply() {
    applyAll();
}

// JSON serialization

static MonitorGammaSettings settingsFromJson(const json& j) {
    MonitorGammaSettings s;
    if (j.contains("brightness")) s.brightness = j["brightness"].get<float>();
    if (j.contains("contrast"))   s.contrast   = j["contrast"].get<float>();
    if (j.contains("gamma_r"))    s.gammaR     = j["gamma_r"].get<float>();
    if (j.contains("gamma_g"))    s.gammaG     = j["gamma_g"].get<float>();
    if (j.contains("gamma_b"))    s.gammaB     = j["gamma_b"].get<float>();
    if (j.contains("enabled"))    s.enabled    = j["enabled"].get<bool>();
    return s;
}

static json settingsToJson(const MonitorGammaSettings& s) {
    return json{
        {"brightness", s.brightness},
        {"contrast",   s.contrast},
        {"gamma_r",    s.gammaR},
        {"gamma_g",    s.gammaG},
        {"gamma_b",    s.gammaB},
        {"enabled",    s.enabled},
    };
}

static json defaultsToJson(const MonitorGammaSettings& s) {
    return json{
        {"brightness", 1.0f},
        {"contrast",   1.0f},
        {"gamma_r",    1.0f},
        {"gamma_g",    1.0f},
        {"gamma_b",    1.0f},
        {"enabled",    false},
    };
}

bool GammaManager::loadConfig() {
    auto path = getConfigPath();
    std::ifstream f(path);
    if (!f.is_open())
        return false;

    try {
        json j;
        f >> j;

        if (j.contains("version") && j["version"].get<int>() >= 1) {
            if (j.contains("defaults"))
                m_defaultSettings = settingsFromJson(j["defaults"]);

            if (j.contains("monitors") && j["monitors"].is_object()) {
                for (auto& [monName, monSettings] : j["monitors"].items())
                    m_monitorSettings[monName] = settingsFromJson(monSettings);
            }
        }
    } catch (...) {
        return false;
    }

    return true;
}

bool GammaManager::saveConfig() const {
    auto path = getConfigPath();
    auto dir = std::filesystem::path(path).parent_path();
    std::filesystem::create_directories(dir);

    std::ofstream f(path);
    if (!f.is_open())
        return false;

    json monitorsJson = json::object();
    for (auto& [name, settings] : m_monitorSettings)
        monitorsJson[name] = settingsToJson(settings);

    json j = {
        {"version",  1},
        {"defaults", defaultsToJson(m_defaultSettings)},
        {"monitors", monitorsJson},
    };

    f << j.dump(2) << std::endl;
    return true;
}

std::string GammaManager::getSettingsJson() const {
    json monitorsJson = json::object();
    for (auto& [name, settings] : m_monitorSettings)
        monitorsJson[name] = settingsToJson(settings);

    json j = {
        {"version",  1},
        {"defaults", defaultsToJson(m_defaultSettings)},
        {"monitors", monitorsJson},
    };
    return j.dump(2);
}

std::string GammaManager::getMonitorListText() const {
    std::string out;
    for (auto& m : State::monitorState()->monitors()) {
        if (!m)
            continue;
        auto it = m_monitorSettings.find(m->m_name);
        auto s = (it != m_monitorSettings.end()) ? it->second : m_defaultSettings;
        out += "Monitor: " + m->m_name + "\n";
        out += "  Description: " + m->m_description + "\n";
        out += "  Enabled: " + std::string(s.enabled ? "yes" : "no") + "\n";
        out += "  Brightness: " + std::to_string(s.brightness) + "\n";
        out += "  Contrast: " + std::to_string(s.contrast) + "\n";
        out += "  Gamma R/G/B: " + std::to_string(s.gammaR) + " / " + std::to_string(s.gammaG) + " / " + std::to_string(s.gammaB) + "\n";
        out += "\n";
    }
    return out;
}
