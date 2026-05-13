#pragma once

#include <hyprland/src/helpers/Monitor.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>

struct MonitorGammaSettings {
    float brightness = 1.0f;
    float contrast = 1.0f;
    float gammaR = 1.0f;
    float gammaG = 1.0f;
    float gammaB = 1.0f;
    bool  enabled = true;
};

class GammaManager {
public:
    GammaManager();
    ~GammaManager();

    void applyAll();
    void applyToMonitor(const std::string& name);
    void applyToAllEnabled();
    void setParam(const std::string& monitor, const std::string& key, float value);
    void setEnabled(const std::string& monitor, bool enabled);
    void reset(const std::string& monitor);
    void resetAll();
    void clearAll();

    std::string getSettingsJson() const;
    std::string getMonitorListText() const;

    bool loadConfig();
    bool saveConfig() const;

    void debugApply(); // manually re-apply to all monitors

private:
    static std::vector<uint16_t> computeGammaTable(size_t gammaSize, const MonitorGammaSettings& settings);
    void applyToMonitor(const std::string& name, const MonitorGammaSettings& settings);
    PHLMONITOR findMonitor(const std::string& name) const;
    std::string getConfigPath() const;

    std::unordered_map<std::string, MonitorGammaSettings> m_monitorSettings;
    MonitorGammaSettings m_defaultSettings;
};
