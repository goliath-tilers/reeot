#pragma once

#include <filesystem>
#include <string>

#include <rex/rex_app.h>

#include "goliath_engine/installer/install_paths.h"

namespace eot::installer {

struct ValidationResult {
  bool valid = false;
  std::filesystem::path runtime_root;
  std::filesystem::path patch_root;
  std::string message;
};

struct StartupResult {
  rex::PathConfig paths;
  bool ready = false;
  bool used_development_assets = false;
  InstallLayout install_layout;
  std::string message;
};

ValidationResult ValidateInstall(const InstallLayout& layout);
StartupResult PrepareStartupPaths(rex::PathConfig defaults,
                                  const std::filesystem::path& config_path);

}  // namespace eot::installer
