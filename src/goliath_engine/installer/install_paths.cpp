#include "goliath_engine/installer/install_paths.h"

#include <cstdlib>
#include <system_error>

#include <rex/filesystem.h>

#include "goliath_engine/installer/install_constants.h"

namespace eot::installer {

std::filesystem::path BuildInstallerUserRoot() {
  auto migrate_legacy = [](const std::filesystem::path& root) {
    const auto legacy = root.parent_path() / kLegacyInstallerUserDirectory;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) && std::filesystem::exists(legacy, ec)) {
      std::filesystem::rename(legacy, root, ec);
    }
    return root;
  };
  const char* home = std::getenv("HOME");
#if defined(_WIN32)
  const char* appdata = std::getenv("APPDATA");
  if (appdata && *appdata) {
    return migrate_legacy(std::filesystem::path(appdata) / kInstallerUserDirectory);
  }
#elif defined(__APPLE__)
  if (home && *home) {
    const auto app_support = std::filesystem::path(home) / "Library" / "Application Support";
    if (std::filesystem::exists(app_support)) {
      return migrate_legacy(app_support / kInstallerUserDirectory);
    }
    return migrate_legacy(std::filesystem::path(home) /
                          ("." + std::string(kInstallerUserDirectory)));
  }
#else
  if (home && *home) {
    const auto config = std::filesystem::path(home) / ".config";
    if (std::filesystem::exists(config)) {
      return migrate_legacy(config / kInstallerUserDirectory);
    }
    return migrate_legacy(std::filesystem::path(home) /
                          ("." + std::string(kInstallerUserDirectory)));
  }
#endif
  return migrate_legacy(std::filesystem::current_path() / kInstallerUserDirectory);
}

std::filesystem::path InstallerConfigPath() {
  return rex::filesystem::GetExecutableFolder() / "reeot.toml";
}

std::filesystem::path InstallerResourceRoot() {
  return (rex::filesystem::GetExecutableFolder() / "../../../res").lexically_normal();
}

std::filesystem::path InstallerFontRoot() {
  return InstallerResourceRoot() / "font";
}

InstallLayout ResolveInstallLayout(const std::filesystem::path& user_data_root) {
  InstallLayout layout;
  layout.root = user_data_root.empty() ? BuildInstallerUserRoot() : user_data_root;
  layout.game = layout.root / "game";
  layout.update = layout.root / "update";
  layout.dlc = layout.root / "dlc";
  layout.mods = layout.root / "mods";
  layout.patched = layout.root / "patched";
  layout.manifest = layout.root / "install_manifest.toml";
  return layout;
}

namespace {

bool HasFile(const std::filesystem::path& root, const char* relative,
             std::string* missing) {
  std::error_code ec;
  const auto path = root / std::filesystem::path(relative);
  if (std::filesystem::is_regular_file(path, ec)) {
    return true;
  }
  if (missing) {
    *missing = path.string();
  }
  return false;
}

bool HasPatchedBinaries(const std::filesystem::path& root, std::string* missing) {
  return HasFile(root, kPatchedEntrypointXex, missing) &&
         HasFile(root, kPatchedGameLogicDll, missing);
}

}  // namespace

std::filesystem::path ResolveRuntimeRoot(const InstallLayout& layout) {
  if (HasRequiredRuntimeFiles(layout.game, nullptr)) {
    return layout.game;
  }
  return {};
}

std::filesystem::path ResolvePatchedEntrypoint(const InstallLayout& layout) {
  if (!HasRequiredPatchedFiles(layout.patched, nullptr)) {
    return {};
  }
  return layout.patched / kPatchedEntrypointXex;
}

std::filesystem::path ResolvePatchedGameLogic(const InstallLayout& layout) {
  if (!HasRequiredPatchedFiles(layout.patched, nullptr)) {
    return {};
  }
  return layout.patched / std::filesystem::path(kPatchedGameLogicDll);
}

bool HasRequiredRuntimeFiles(const std::filesystem::path& root, std::string* missing) {
  return HasFile(root, kEntrypointXex, missing) &&
         HasFile(root, kGameLogicDll, missing);
}

bool HasRequiredPatchFiles(const std::filesystem::path& root, std::string* missing) {
  return HasFile(root, kEntrypointPatch, missing) &&
         HasFile(root, kGameLogicPatch, missing);
}

bool HasRequiredPatchedFiles(const std::filesystem::path& root, std::string* missing) {
  return HasPatchedBinaries(root, missing);
}

bool HasMountedPatchedFiles(const InstallLayout& layout, std::string* missing) {
  return HasPatchedBinaries(layout.game / "patched", missing);
}

bool EnsureInstallDirectories(const InstallLayout& layout, std::string* error) {
  std::error_code ec;
  for (const auto& path : {layout.root, layout.game, layout.update, layout.dlc,
                           layout.mods, layout.patched,
                           layout.game / "Data", layout.patched / "Data"}) {
    std::filesystem::create_directories(path, ec);
    if (ec) {
      if (error) {
        *error = "Failed to create " + path.string() + ": " + ec.message();
      }
      return false;
    }
  }
  return true;
}

}  // namespace eot::installer
