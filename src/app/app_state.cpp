#include "app_state.h"

#include <Windows.h>

namespace {
constexpr const wchar_t* kRunKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kRunValueName = L"ScreenResizer";
}  // namespace

namespace app {

bool AppState::Initialize() {
    settings = config::SettingsStore::LoadWithValidation();
    ApplyStartupRegistration();

    if (!settings.onboardingComplete) {
        MessageBoxW(nullptr,
            L"Welcome to Screen Resizer!\n\nUse S to open settings and configure resolution profiles.",
            L"Screen Resizer Onboarding",
            MB_OK | MB_ICONINFORMATION);
        settings.onboardingComplete = true;
        settings.introFlowState = "completed";
        Save();
    }

    return true;
}

bool AppState::Save() {
    ApplyStartupRegistration();
    return config::SettingsStore::Save(settings);
}

void AppState::ApplyStartupRegistration() {
    SetLaunchAtStartup(settings.launchAtStartup);
}

bool AppState::SetLaunchAtStartup(bool enabled) {
    HKEY runKey = nullptr;
    LONG openResult = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        0,
        nullptr,
        0,
        KEY_SET_VALUE,
        nullptr,
        &runKey,
        nullptr);

    if (openResult != ERROR_SUCCESS) {
        return false;
    }

    if (!enabled) {
        RegDeleteValueW(runKey, kRunValueName);
        RegCloseKey(runKey);
        return true;
    }

    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
        RegCloseKey(runKey);
        return false;
    }

    LONG setResult = RegSetValueExW(
        runKey,
        kRunValueName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(modulePath),
        static_cast<DWORD>((wcslen(modulePath) + 1) * sizeof(wchar_t)));

    RegCloseKey(runKey);
    return setResult == ERROR_SUCCESS;
}

}  // namespace app
