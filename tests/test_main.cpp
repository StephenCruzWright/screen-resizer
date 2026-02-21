#include <filesystem>
#include <fstream>
#include <iostream>

#include "config/settings.h"

namespace {

bool Assert(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << std::endl;
    return false;
  }
  return true;
}

bool TestDefaultsWhenMissingFile() {
  const std::filesystem::path original = std::filesystem::current_path();
  const std::filesystem::path tempDir = original / "test_output" / "missing_file";
  std::filesystem::create_directories(tempDir);
  std::filesystem::remove(tempDir / "settings.json");
  std::filesystem::current_path(tempDir);

  const config::Settings settings = config::SettingsStore::LoadWithValidation();
  std::filesystem::current_path(original);

  return Assert(settings.viewportWidth == 1280, "expected default viewportWidth") &&
         Assert(settings.profileName == "Default", "expected default profile") &&
         Assert(!settings.launchAtStartup, "expected default launchAtStartup");
}

bool TestInvalidValuesAreSanitized() {
  const std::filesystem::path original = std::filesystem::current_path();
  const std::filesystem::path tempDir = original / "test_output" / "invalid_values";
  std::filesystem::create_directories(tempDir);
  std::filesystem::current_path(tempDir);

  std::ofstream out("settings.json", std::ios::trunc);
  out << "{\n"
         "  \"viewportWidth\": 10,\n"
         "  \"viewportHeight\": 720,\n"
         "  \"zoom\": 9.0,\n"
         "  \"profileName\": \"Demo\",\n"
         "  \"launchAtStartup\": true\n"
         "}\n";
  out.close();

  const config::Settings settings = config::SettingsStore::LoadWithValidation();
  std::filesystem::current_path(original);

  return Assert(settings.viewportWidth == 1280, "invalid width should fall back to default") &&
         Assert(settings.viewportHeight == 720, "valid height should be preserved") &&
         Assert(settings.zoom == 1.0f, "invalid zoom should fall back to default") &&
         Assert(settings.profileName == "Demo", "valid profile should be preserved") &&
         Assert(settings.launchAtStartup, "valid bool should be preserved");
}

bool TestSaveEscapesJsonStrings() {
  const std::filesystem::path original = std::filesystem::current_path();
  const std::filesystem::path tempDir = original / "test_output" / "escaped_strings";
  std::filesystem::create_directories(tempDir);
  std::filesystem::current_path(tempDir);

  config::Settings settings = config::SettingsStore::Defaults();
  settings.profileName = "Main \"Display\"";
  settings.introFlowState = "done\\final";

  const bool saved = config::SettingsStore::Save(settings);
  std::ifstream in("settings.json");
  std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::filesystem::current_path(original);

  return Assert(saved, "expected settings save to succeed") &&
         Assert(raw.find("Main \\\"Display\\\"") != std::string::npos,
                "expected quotes to be escaped in JSON") &&
         Assert(raw.find("done\\\\final") != std::string::npos,
                "expected backslashes to be escaped in JSON");
}

}  // namespace

int main() {
  if (!TestDefaultsWhenMissingFile() || !TestInvalidValuesAreSanitized() || !TestSaveEscapesJsonStrings()) {
    return 1;
  }

  std::cout << "all tests passed" << std::endl;
  return 0;
}
