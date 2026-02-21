#pragma once

#include <string>

namespace config {

struct Settings {
    bool onboardingComplete = false;
    std::string introFlowState = "welcome";

    int viewportWidth = 1280;
    int viewportHeight = 720;
    float viewportOffsetX = 0.0f;
    float viewportOffsetY = 0.0f;
    float zoom = 1.0f;
    std::string profileName = "Default";

    bool launchAtStartup = false;
};

class SettingsStore {
public:
    static Settings Defaults();
    static std::wstring ConfigPath();
    static Settings LoadWithValidation();
    static bool Save(const Settings& settings);

private:
    static Settings ParseAndValidate(const std::string& raw, bool* ok);
};

}  // namespace config
