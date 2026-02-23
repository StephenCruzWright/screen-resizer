#include "settings.h"

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>

namespace {

std::optional<size_t> LocateValueStart(const std::string& raw, const std::string& key) {
    const std::string quotedKey = "\"" + key + "\"";
    size_t keyPos = raw.find(quotedKey);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }

    size_t colonPos = raw.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos) {
        return std::nullopt;
    }

    size_t valuePos = raw.find_first_not_of(" \t\r\n", colonPos + 1);
    if (valuePos == std::string::npos) {
        return std::nullopt;
    }

    return valuePos;
}

std::optional<std::string> ExtractString(const std::string& raw, const std::string& key) {
    const auto valuePos = LocateValueStart(raw, key);
    if (!valuePos || raw[*valuePos] != '"') {
        return std::nullopt;
    }

    std::string result;
    for (size_t i = *valuePos + 1; i < raw.size(); ++i) {
        const char c = raw[i];
        if (c == '\\' && i + 1 < raw.size()) {
            const char escaped = raw[++i];
            switch (escaped) {
                case '"':
                    result += '"';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case 'n':
                    result += '\n';
                    break;
                case 'r':
                    result += '\r';
                    break;
                case 't':
                    result += '\t';
                    break;
                default:
                    result += escaped;
                    break;
            }
            continue;
        }

        if (c == '"') {
            return result;
        }

        result += c;
    }

    return std::nullopt;
}

template <typename NumericType>
std::optional<NumericType> ExtractNumber(const std::string& raw, const std::string& key) {
    const auto valuePos = LocateValueStart(raw, key);
    if (!valuePos) {
        return std::nullopt;
    }

    size_t consumed = 0;
    try {
        if constexpr (std::is_same_v<NumericType, int>) {
            const long parsed = std::stol(raw.substr(*valuePos), &consumed);
            if (consumed == 0 || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
                return std::nullopt;
            }
            return static_cast<int>(parsed);
        } else {
            const float parsed = std::stof(raw.substr(*valuePos), &consumed);
            if (consumed == 0) {
                return std::nullopt;
            }
            return parsed;
        }
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> ExtractInt(const std::string& raw, const std::string& key) {
    return ExtractNumber<int>(raw, key);
}

std::optional<float> ExtractFloat(const std::string& raw, const std::string& key) {
    return ExtractNumber<float>(raw, key);
}

std::optional<bool> ExtractBool(const std::string& raw, const std::string& key) {
    const auto valuePos = LocateValueStart(raw, key);
    if (!valuePos) {
        return std::nullopt;
    }

    if (raw.compare(*valuePos, 4, "true") == 0) {
        return true;
    }
    if (raw.compare(*valuePos, 5, "false") == 0) {
        return false;
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
