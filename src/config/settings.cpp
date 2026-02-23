#include "settings.h"

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>

namespace {

std::optional<std::string> ExtractString(const std::string& raw, const std::string& key) {
    std::regex rx("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (std::regex_search(raw, match, rx)) {
        return match[1].str();
    }
    return std::nullopt;
}

std::optional<int> ExtractInt(const std::string& raw, const std::string& key) {
    std::regex rx("\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (std::regex_search(raw, match, rx)) {
        return std::stoi(match[1].str());
    }
    return std::nullopt;
}

std::optional<float> ExtractFloat(const std::string& raw, const std::string& key) {
    std::regex rx("\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(raw, match, rx)) {
        return std::stof(match[1].str());
    }
    return std::nullopt;
}

std::optional<bool> ExtractBool(const std::string& raw, const std::string& key) {
    std::regex rx("\\\"" + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(raw, match, rx)) {
        return match[1].str() == "true";
    }
    return std::nullopt;
}

std::string EscapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    for (char c : input) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
                break;
        }
    }

    return out;
}

}  // namespace

namespace config {

Settings SettingsStore::Defaults() {
    return Settings{};
}

std::wstring SettingsStore::ConfigPath() {
#ifdef _WIN32
    const std::filesystem::path localSettings = std::filesystem::current_path() / L"settings.json";
    if (std::filesystem::exists(localSettings)) {
        return localSettings.wstring();
    }

    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        return L"settings.json";
    }

    std::filesystem::path path(localAppData);
    CoTaskMemFree(localAppData);

    path /= L"ScreenResizer";
    std::filesystem::create_directories(path);
    path /= L"settings.json";
    return path.wstring();
#else
    std::filesystem::path path = std::filesystem::current_path();
    path /= "settings.json";
    return path.wstring();
#endif
}

Settings SettingsStore::ParseAndValidate(const std::string& raw, bool* ok) {
    Settings settings = Defaults();
    bool allValid = true;

    const auto onboarding = ExtractBool(raw, "onboardingComplete");
    const auto intro = ExtractString(raw, "introFlowState");
    const auto viewportWidth = ExtractInt(raw, "viewportWidth");
    const auto viewportHeight = ExtractInt(raw, "viewportHeight");
    const auto offsetX = ExtractFloat(raw, "viewportOffsetX");
    const auto offsetY = ExtractFloat(raw, "viewportOffsetY");
    const auto zoom = ExtractFloat(raw, "zoom");
    const auto profile = ExtractString(raw, "profileName");
    const auto launchAtStartup = ExtractBool(raw, "launchAtStartup");

    if (onboarding) {
        settings.onboardingComplete = *onboarding;
    }
    if (intro) {
        settings.introFlowState = *intro;
    }

    if (viewportWidth && *viewportWidth >= 320 && *viewportWidth <= 10000) {
        settings.viewportWidth = *viewportWidth;
    } else if (viewportWidth) {
        allValid = false;
    }

    if (viewportHeight && *viewportHeight >= 200 && *viewportHeight <= 10000) {
        settings.viewportHeight = *viewportHeight;
    } else if (viewportHeight) {
        allValid = false;
    }

    if (offsetX) {
        settings.viewportOffsetX = std::clamp(*offsetX, -5000.0f, 5000.0f);
    }
    if (offsetY) {
        settings.viewportOffsetY = std::clamp(*offsetY, -5000.0f, 5000.0f);
    }

    if (zoom && *zoom >= 0.1f && *zoom <= 6.0f) {
        settings.zoom = *zoom;
    } else if (zoom) {
        allValid = false;
    }

    if (profile && !profile->empty() && profile->size() <= 64) {
        settings.profileName = *profile;
    } else if (profile) {
        allValid = false;
    }

    if (launchAtStartup) {
        settings.launchAtStartup = *launchAtStartup;
    }

    if (ok) {
        *ok = allValid;
    }
    return settings;
}

Settings SettingsStore::LoadWithValidation() {
    std::ifstream in{std::filesystem::path(ConfigPath())};
    if (!in.is_open()) {
        return Defaults();
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    bool ok = true;
    Settings settings = ParseAndValidate(buffer.str(), &ok);

    if (!ok) {
        Save(settings);
    }
    return settings;
}

bool SettingsStore::Save(const Settings& settings) {
    std::ofstream out{std::filesystem::path(ConfigPath()), std::ios::trunc};
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << "  \"onboardingComplete\": " << (settings.onboardingComplete ? "true" : "false") << ",\n";
    out << "  \"introFlowState\": \"" << EscapeJsonString(settings.introFlowState) << "\",\n";
    out << "  \"viewportWidth\": " << settings.viewportWidth << ",\n";
    out << "  \"viewportHeight\": " << settings.viewportHeight << ",\n";
    out << "  \"viewportOffsetX\": " << settings.viewportOffsetX << ",\n";
    out << "  \"viewportOffsetY\": " << settings.viewportOffsetY << ",\n";
    out << "  \"zoom\": " << settings.zoom << ",\n";
    out << "  \"profileName\": \"" << EscapeJsonString(settings.profileName) << "\",\n";
    out << "  \"launchAtStartup\": " << (settings.launchAtStartup ? "true" : "false") << "\n";
    out << "}\n";

    return true;
}

}  // namespace config
