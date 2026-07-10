#pragma once

#include <filesystem>
#include <string>

namespace eot::installer {

struct InstallManifest {
  bool installed = false;
  std::string runtime_root;
  std::string note;
};

bool LoadInstallManifest(const std::filesystem::path& path,
                         InstallManifest* manifest);
bool SaveInstallManifest(const std::filesystem::path& path,
                         const InstallManifest& manifest,
                         std::string* error);

}  // namespace eot::installer
