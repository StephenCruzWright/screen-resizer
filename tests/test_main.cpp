#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "config/settings.h"

namespace {

bool Assert(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << std::endl;
    return false;
  }
  return true;
}

constexpr const char* kConfigPathEnv = "SCREEN_RESIZER_CONFIG_PATH";

#ifdef _WIN32
constexpr const wchar_t* kConfigPathEnvWide = L"SCREEN_RESIZER_CONFIG_PATH";

std::optional<std::filesystem::path> GetConfigPathOverride() {
  if (const wchar_t* existing = _wgetenv(kConfigPathEnvWide)) {
    if (existing[0] != L'\0') {
      return std::filesystem::path(existing);
    }
  }
  return std::nullopt;
}

bool SetConfigPathOverride(const std::filesystem::path& path) {
  return _wputenv_s(kConfigPathEnvWide, path.wstring().c_str()) == 0;
}

bool ClearConfigPathOverride() {
  return _wputenv_s(kConfigPathEnvWide, L"") == 0;
}
#else
std::optional<std::filesystem::path> GetConfigPathOverride() {
  if (const char* existing = std::getenv(kConfigPathEnv)) {
    if (existing[0] != '\0') {
      return std::filesystem::path(existing);
    }
  }
  return std::nullopt;
}

bool SetConfigPathOverride(const std::filesystem::path& path) {
  return setenv(kConfigPathEnv, path.c_str(), 1) == 0;
}

bool ClearConfigPathOverride() {
  return unsetenv(kConfigPathEnv) == 0;
}
#endif

class ScopedConfigPathOverride {
 public:
  explicit ScopedConfigPathOverride(const std::filesystem::path& configPath) {
    previousValue_ = GetConfigPathOverride();
    active_ = SetConfigPathOverride(configPath);
  }

  ~ScopedConfigPathOverride() {
    if (previousValue_) {
      SetConfigPathOverride(*previousValue_);
    } else {
      ClearConfigPathOverride();
    }
  }

  bool Active() const { return active_; }

 private:
  bool active_ = false;
  std::optional<std::filesystem::path> previousValue_;
};

bool TestDefaultsWhenMissingFile() {
  const std::filesystem::path tempDir = std::filesystem::current_path() / "test_output" / "missing_file";
  std::filesystem::create_directories(tempDir);
  const std::filesystem::path configPath = tempDir / "settings.json";
  std::filesystem::remove(configPath);
  ScopedConfigPathOverride configOverride(configPath);
  if (!configOverride.Active()) {
    return Assert(false, "failed to set config path override");
  }

  const config::Settings settings = config::SettingsStore::LoadWithValidation();

  return Assert(settings.viewportWidth == 1280, "expected default viewportWidth") &&
         Assert(settings.profileName == "Default", "expected default profile") &&
         Assert(!settings.launchAtStartup, "expected default launchAtStartup");
}

bool TestInvalidValuesAreSanitized() {
  const std::filesystem::path tempDir = std::filesystem::current_path() / "test_output" / "invalid_values";
  std::filesystem::create_directories(tempDir);
  const std::filesystem::path configPath = tempDir / "settings.json";
  ScopedConfigPathOverride configOverride(configPath);
  if (!configOverride.Active()) {
    return Assert(false, "failed to set config path override");
  }

  std::ofstream out(configPath, std::ios::trunc);
  out << "{\n"
         "  \"viewportWidth\": 10,\n"
         "  \"viewportHeight\": 720,\n"
         "  \"zoom\": 9.0,\n"
         "  \"profileName\": \"Demo\",\n"
         "  \"launchAtStartup\": true\n"
         "}\n";
  out.close();

  const config::Settings settings = config::SettingsStore::LoadWithValidation();

  return Assert(settings.viewportWidth == 1280, "invalid width should fall back to default") &&
         Assert(settings.viewportHeight == 720, "valid height should be preserved") &&
         Assert(settings.zoom == 1.0f, "invalid zoom should fall back to default") &&
         Assert(settings.profileName == "Demo", "valid profile should be preserved") &&
         Assert(settings.launchAtStartup, "valid bool should be preserved");
}



bool TestLoadUnescapesJsonStrings() {
  const std::filesystem::path tempDir = std::filesystem::current_path() / "test_output" / "load_escaped_strings";
  std::filesystem::create_directories(tempDir);
  const std::filesystem::path configPath = tempDir / "settings.json";
  ScopedConfigPathOverride configOverride(configPath);
  if (!configOverride.Active()) {
    return Assert(false, "failed to set config path override");
  }

  std::ofstream out(configPath, std::ios::trunc);
  out << "{\n"
         "  \"profileName\": \"Main \\\"Display\\\"\",\n"
         "  \"introFlowState\": \"done\\\\final\"\n"
         "}\n";
  out.close();

  const config::Settings settings = config::SettingsStore::LoadWithValidation();

  return Assert(settings.profileName == "Main \"Display\"",
                "escaped quote should be unescaped when loading") &&
         Assert(settings.introFlowState == "done\\final",
                "escaped backslash should be unescaped when loading");
}

bool TestSaveEscapesJsonStrings() {
  const std::filesystem::path tempDir = std::filesystem::current_path() / "test_output" / "escaped_strings";
  std::filesystem::create_directories(tempDir);
  const std::filesystem::path configPath = tempDir / "settings.json";
  ScopedConfigPathOverride configOverride(configPath);
  if (!configOverride.Active()) {
    return Assert(false, "failed to set config path override");
  }

  config::Settings settings = config::SettingsStore::Defaults();
  settings.profileName = "Main \"Display\"";
  settings.introFlowState = "done\\final";

  const bool saved = config::SettingsStore::Save(settings);
  std::ifstream in(configPath);
  std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

  return Assert(saved, "expected settings save to succeed") &&
         Assert(raw.find("Main \\\"Display\\\"") != std::string::npos,
                "expected quotes to be escaped in JSON") &&
         Assert(raw.find("done\\\\final") != std::string::npos,
                "expected backslashes to be escaped in JSON");
}

bool TestMalformedTokensAreRejected() {
  const std::filesystem::path tempDir = std::filesystem::current_path() / "test_output" / "malformed_tokens";
  std::filesystem::create_directories(tempDir);
  const std::filesystem::path configPath = tempDir / "settings.json";
  ScopedConfigPathOverride configOverride(configPath);
  if (!configOverride.Active()) {
    return Assert(false, "failed to set config path override");
  }

  std::ofstream out(configPath, std::ios::trunc);
  out << "{\n"
         "  \"viewportHeight\": 900oops,\n"
         "  \"zoom\": 2.5bad,\n"
         "  \"launchAtStartup\": trueish,\n"
         "  \"profileName\": \"Demo\"\n"
         "}\n";
  out.close();

  const config::Settings settings = config::SettingsStore::LoadWithValidation();
  return Assert(settings.viewportHeight == 720, "malformed height token should fall back to default") &&
         Assert(settings.zoom == 1.0f, "malformed zoom token should fall back to default") &&
         Assert(!settings.launchAtStartup, "malformed bool token should fall back to default") &&
         Assert(settings.profileName == "Demo", "valid string value should still be loaded");
}

bool TestSaveCreatesMissingDirectories() {
  const std::filesystem::path tempDir = std::filesystem::current_path() / "test_output" / "nested" / "path";
  std::filesystem::remove_all(tempDir);
  const std::filesystem::path configPath = tempDir / "settings.json";
  ScopedConfigPathOverride configOverride(configPath);
  if (!configOverride.Active()) {
    return Assert(false, "failed to set config path override");
  }

  config::Settings settings = config::SettingsStore::Defaults();
  settings.profileName = "Nested";
  const bool saved = config::SettingsStore::Save(settings);

  return Assert(saved, "save should succeed when parent directories do not exist") &&
         Assert(std::filesystem::exists(configPath), "save should create the missing config directory tree");
}

bool TestOverridePathWithSpaces() {
  const std::filesystem::path tempDir = std::filesystem::current_path() / "test_output" / "path with spaces";
  std::filesystem::create_directories(tempDir);
  const std::filesystem::path configPath = tempDir / "settings file.json";
  ScopedConfigPathOverride configOverride(configPath);
  if (!configOverride.Active()) {
    return Assert(false, "failed to set config path override");
  }

  config::Settings toSave = config::SettingsStore::Defaults();
  toSave.profileName = "SpacePath";
  toSave.launchAtStartup = true;
  if (!config::SettingsStore::Save(toSave)) {
    return Assert(false, "save should succeed when config path contains spaces");
  }

  const config::Settings loaded = config::SettingsStore::LoadWithValidation();
  return Assert(loaded.profileName == "SpacePath", "profile should round-trip with spaced config path") &&
         Assert(loaded.launchAtStartup, "bool should round-trip with spaced config path");
}

}  // namespace

int main() {
  if (!TestDefaultsWhenMissingFile() ||
      !TestInvalidValuesAreSanitized() ||
      !TestLoadUnescapesJsonStrings() ||
      !TestSaveEscapesJsonStrings() ||
      !TestMalformedTokensAreRejected() ||
      !TestSaveCreatesMissingDirectories() ||
      !TestOverridePathWithSpaces()) {
    return 1;
  }

  std::cout << "all tests passed" << std::endl;
  return 0;
}
