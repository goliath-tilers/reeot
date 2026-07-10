#include "goliath_engine/installer/installer.h"

#include <filesystem>
#include <string>
#include <system_error>

#include <rex/cvar.h>

#include "goliath_engine/common/cvars.h"
#include "goliath_engine/common/logging.h"
#include "goliath_engine/installer/install_constants.h"
#include "goliath_engine/installer/install_manifest.h"

namespace eot::installer {
namespace {

bool IsRegularFile(const std::filesystem::path& path) {
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec);
}

std::filesystem::path NormalPath(const std::filesystem::path& path) {
  std::error_code ec;
  auto canonical = std::filesystem::weakly_canonical(path, ec);
  return ec ? path.lexically_normal() : canonical;
}

bool SamePath(const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
  return NormalPath(lhs) == NormalPath(rhs);
}

bool IsManagedRuntimeRoot(const std::filesystem::path& root,
                          const InstallLayout& layout) {
  return SamePath(root, layout.game) || SamePath(root, layout.patched);
}

bool HasCompletedManagedManifest(const InstallLayout& layout, std::string* message) {
  InstallManifest manifest;
  if (!LoadInstallManifest(layout.manifest, &manifest)) {
    if (message) {
      *message = "Install manifest is missing.";
    }
    return false;
  }
  if (!manifest.installed) {
    if (message) {
      *message = "Install manifest exists but is not marked installed.";
    }
    return false;
  }
  return true;
}

std::filesystem::path LocatePatchRoot(const std::filesystem::path& runtime_root,
                                      std::string* missing) {
  if (HasRequiredPatchFiles(runtime_root, missing)) {
    return runtime_root;
  }

  const auto update_root = runtime_root / "Update";
  if (HasRequiredPatchFiles(update_root, missing)) {
    return update_root;
  }

  const auto sibling_update_root = runtime_root.parent_path() / "update";
  if (HasRequiredPatchFiles(sibling_update_root, missing)) {
    return sibling_update_root;
  }

  return {};
}

void MarkInstallerSkipped(const std::filesystem::path& config_path) {
  if (!REXCVAR_GET(skip_installer)) {
    EOT_INFO("[installer] enabling skip_installer");
    REXCVAR_SET(skip_installer, true);
    if (!config_path.empty()) {
      EOT_INFO("[installer] saving config after validation: {}", config_path.string());
      rex::cvar::SaveConfig(config_path);
    }
  }
}

std::filesystem::path ResolveConfigRelativePath(std::filesystem::path path,
                                                const std::filesystem::path& config_dir) {
  if (path.empty() || path.is_absolute()) {
    return path;
  }
  return (config_dir / path).lexically_normal();
}

ValidationResult ValidateConfiguredRoot(const std::filesystem::path& config_dir) {
  auto root = ResolveConfigRelativePath(
      std::filesystem::path(rex::cvar::GetFlagByName(kCvEotDataRoot)), config_dir);
  if (root.empty()) {
    EOT_INFO("[installer] eot_data_root is empty");
    return {false, {}, {}, "eot_data_root is empty."};
  }

  EOT_INFO("[installer] validating eot_data_root: {}", root.string());
  const auto managed_layout = ResolveInstallLayout(BuildInstallerUserRoot());
  const bool is_managed_runtime_root = IsManagedRuntimeRoot(root, managed_layout);
  if (is_managed_runtime_root) {
    std::string manifest_error;
    if (!HasCompletedManagedManifest(managed_layout, &manifest_error)) {
      EOT_WARN("[installer] configured managed root is incomplete: {}", manifest_error);
      return {false, {}, {}, manifest_error};
    }
    if (SamePath(root, managed_layout.patched)) {
      EOT_INFO("[installer] migrating managed eot_data_root from patched to game root");
      root = managed_layout.game;
    }
    std::string missing;
    if (!HasRequiredPatchedFiles(managed_layout.patched, &missing)) {
      EOT_WARN("[installer] configured managed root missing patched runtime file: {}", missing);
      return {false, {}, {}, "Missing patched runtime file: " + missing};
    }
    if (!HasMountedPatchedFiles(managed_layout, &missing)) {
      EOT_WARN("[installer] configured managed root missing mounted patched runtime file: {}",
               missing);
      return {false, {}, {}, "Missing mounted patched runtime file: " + missing};
    }
  }

  std::string missing;
  if (!HasRequiredRuntimeFiles(root, &missing)) {
    EOT_WARN("[installer] configured root missing runtime file: {}", missing);
    return {
        false, {}, {},
        "eot_data_root is missing required game file: " + missing,
    };
  }

  EOT_INFO("[installer] configured root valid. runtime={}", root.string());
  return {true, root, is_managed_runtime_root ? managed_layout.update : std::filesystem::path{},
          "eot_data_root is ready."};
}

}  // namespace

ValidationResult ValidateInstall(const InstallLayout& layout) {
  EOT_INFO("[installer] validating install layout root={}", layout.root.string());
  InstallManifest manifest;
  const bool has_manifest = LoadInstallManifest(layout.manifest, &manifest);
  EOT_DEBUG("[installer] manifest path={} present={} installed={}",
            layout.manifest.string(), has_manifest, manifest.installed);

  std::string manifest_error;
  if (!HasCompletedManagedManifest(layout, &manifest_error)) {
    EOT_WARN("[installer] install layout manifest invalid: {}", manifest_error);
    return {false, {}, {}, manifest_error};
  }

  std::string missing;
  auto runtime_root = ResolveRuntimeRoot(layout);
  if (runtime_root.empty()) {
    EOT_WARN("[installer] install layout missing runtime files. game={} patched={}",
             layout.game.string(), layout.patched.string());
    return {
        false,
        {},
        {},
        "Missing runtime files. Expected " + std::string(kEntrypointXex) +
            " and " + kGameLogicDll + " under " + layout.game.string() + ".",
    };
  }

  if (!HasRequiredPatchedFiles(layout.patched, &missing)) {
    EOT_WARN("[installer] install layout missing patched runtime file: {}", missing);
    return {false, {}, {}, "Missing patched runtime file: " + missing};
  }
  if (!HasMountedPatchedFiles(layout, &missing)) {
    EOT_WARN("[installer] install layout missing mounted patched runtime file: {}", missing);
    return {false, {}, {}, "Missing mounted patched runtime file: " + missing};
  }

  if (!HasRequiredPatchFiles(layout.update, &missing)) {
    EOT_WARN("[installer] install layout missing patch file: {}", missing);
    return {false, {}, {}, "Missing title update patch file: " + missing};
  }

  EOT_INFO("[installer] install layout valid. runtime={} patched={} update={}",
           runtime_root.string(), layout.patched.string(), layout.update.string());
  return {true, runtime_root, layout.update, "Install is ready."};
}

StartupResult PrepareStartupPaths(rex::PathConfig defaults,
                                  const std::filesystem::path& config_path) {
  const auto active_config = config_path;
  EOT_INFO("[installer] preparing startup paths. config={}", active_config.string());
  if (IsRegularFile(active_config)) {
    EOT_INFO("[installer] loading config: {}", active_config.string());
    rex::cvar::LoadConfig(active_config);
  } else {
    EOT_INFO("[installer] config not found yet: {}", active_config.string());
  }

  auto configured = ValidateConfiguredRoot(active_config.parent_path());
  if (configured.valid) {
    const auto managed_layout = ResolveInstallLayout(BuildInstallerUserRoot());
    rex::cvar::SetFlagByName(kCvEotDataRoot, configured.runtime_root.string());
    defaults.game_data_root = configured.runtime_root;
    defaults.update_data_root = configured.patch_root;
    if (IsManagedRuntimeRoot(configured.runtime_root, managed_layout)) {
      defaults.user_data_root = managed_layout.root;
    }
    if (!REXCVAR_GET(skip_installer)) {
      MarkInstallerSkipped(active_config);
    } else {
      rex::cvar::SaveConfig(active_config);
    }
    return {std::move(defaults), true, false, {}, configured.message};
  }

  defaults.user_data_root = BuildInstallerUserRoot();

  auto layout = ResolveInstallLayout(defaults.user_data_root);
  std::string error;
  if (!EnsureInstallDirectories(layout, &error)) {
    EOT_ERROR("[installer] failed to create install directories: {}", error);
  } else {
    EOT_INFO("[installer] install directories ready. root={}", layout.root.string());
  }

  // The installer target layout is still useful for first-run installs, but
  // runtime startup is cvar-driven: a completed install should set
  // eot_data_root to the chosen patched/game root and then enable
  // skip_installer.
  auto installed = ValidateInstall(layout);
  if (installed.valid) {
    rex::cvar::SetFlagByName(kCvEotDataRoot, installed.runtime_root.string());
    defaults.game_data_root = installed.runtime_root;
    defaults.update_data_root = installed.patch_root;
    defaults.user_data_root = layout.root;
    MarkInstallerSkipped(active_config);
    rex::cvar::SaveConfig(active_config);
    return {std::move(defaults), true, false, layout, installed.message};
  }

  defaults.game_data_root.clear();
  defaults.update_data_root = layout.update;
  EOT_WARN("[installer] startup blocked for installer. configured='{}' installed='{}'",
           configured.message, installed.message);
  return {
      std::move(defaults),
      false,
      false,
      layout,
      "ReEoT assets were not found through eot_data_root. " +
          configured.message + " Installer target is " + layout.root.string() +
          " (game/, update/, patched/). " + installed.message,
  };
}

}  // namespace eot::installer
