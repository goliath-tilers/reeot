#pragma once

#include <filesystem>
#include <string>

namespace eot::installer {

struct InstallLayout {
  std::filesystem::path root;
  std::filesystem::path game;
  std::filesystem::path update;
  std::filesystem::path dlc;
  std::filesystem::path mods;
  std::filesystem::path patched;
  std::filesystem::path manifest;
};

std::filesystem::path BuildInstallerUserRoot();
std::filesystem::path InstallerConfigPath();
std::filesystem::path InstallerResourceRoot();
std::filesystem::path InstallerFontRoot();
InstallLayout ResolveInstallLayout(const std::filesystem::path& user_data_root);
std::filesystem::path ResolveRuntimeRoot(const InstallLayout& layout);
std::filesystem::path ResolvePatchedEntrypoint(const InstallLayout& layout);
std::filesystem::path ResolvePatchedGameLogic(const InstallLayout& layout);
bool HasRequiredRuntimeFiles(const std::filesystem::path& root, std::string* missing);
bool HasRequiredPatchFiles(const std::filesystem::path& root, std::string* missing);
bool HasRequiredPatchedFiles(const std::filesystem::path& root, std::string* missing);
bool HasMountedPatchedFiles(const InstallLayout& layout, std::string* missing);
bool EnsureInstallDirectories(const InstallLayout& layout, std::string* error);

}  // namespace eot::installer
