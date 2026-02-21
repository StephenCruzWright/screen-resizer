#pragma once

#include <Windows.h>

#include "../config/settings.h"

namespace app {

class AppState {
public:
    bool Initialize();
    bool Save();

    config::Settings settings;

private:
    void ApplyStartupRegistration();
    bool SetLaunchAtStartup(bool enabled);
};

}  // namespace app
