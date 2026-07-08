#include "goliath_engine/installer/installer_dialog.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

#include <ddspp.h>
#include <imgui.h>
#include <nfd.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/image_decode.h>
#include <rex/ui/immediate_drawer.h>
#include <xex_patcher.h>

#include "goliath_engine/common/cvars.h"
#include "goliath_engine/common/logging.h"
#include "goliath_engine/installer/install_constants.h"
#include "goliath_engine/installer/install_manifest.h"
#include "goliath_engine/installer/source_vfs.h"

#ifndef XXH_INLINE_ALL
#define XXH_INLINE_ALL
#endif
#include <xxhash.h>

namespace eot::installer {
namespace {

constexpr float kCanvasWidth = 1280.0f;
constexpr float kCanvasHeight = 720.0f;
constexpr float kPi = 3.14159265358979323846f;

constexpr double kScanlinesAnimationTime = 0.0;
constexpr double kScanlinesAnimationDuration = 15.0;
constexpr double kIconAnimationTime = kScanlinesAnimationTime + 10.0;
constexpr double kIconAnimationDuration = 15.0;
constexpr double kImageAnimationTime = kIconAnimationTime + kIconAnimationDuration;
constexpr double kImageAnimationDuration = 15.0;
constexpr double kTitleAnimationTime = kScanlinesAnimationDuration;
constexpr double kTitleAnimationDuration = 30.0;
constexpr double kContainerLineAnimationTime = kScanlinesAnimationDuration;
constexpr double kContainerLineAnimationDuration = 23.0;
constexpr double kContainerOuterTime =
    kScanlinesAnimationDuration + kContainerLineAnimationDuration;
constexpr double kContainerOuterDuration = 23.0;
constexpr double kContainerInnerTime =
    kScanlinesAnimationDuration + kContainerLineAnimationDuration + 8.0;
constexpr double kContainerInnerDuration = 15.0;
constexpr double kAllAnimationsFullDuration =
    kContainerInnerTime + kContainerInnerDuration;
constexpr double kQuittingExtraDuration = 60.0;

constexpr float kImageX = 161.5f;
constexpr float kImageY = 103.5f;
constexpr float kImageWidth = 512.0f;
constexpr float kImageHeight = 512.0f;
constexpr float kContainerX = 513.0f;
constexpr float kContainerY = 226.0f;
constexpr float kContainerWidth = 526.5f;
constexpr float kContainerHeight = 246.0f;
constexpr float kSideContainerWidth = kContainerWidth / 2.0f;
constexpr float kBottomXGap = 4.0f;
constexpr float kBottomYGap = 5.0f;
constexpr float kContainerButtonWidth = 250.0f;
constexpr float kContainerButtonGap = 9.0f;
constexpr float kButtonHeight = 22.0f;
constexpr float kButtonTextGap = 28.0f;
constexpr float kBorderSize = 1.0f;
constexpr float kBorderOvershoot = 36.0f;
constexpr float kGridSize = 9.0f;

enum class WizardPage {
  SelectLanguage,
  Introduction,
  SelectGameAndUpdate,
  SelectDLC,
  CheckSpace,
  Installing,
  InstallSucceeded,
  InstallFailed,
};

enum class MessageSource {
  Unknown,
  Next,
  Back,
  PickerTutorialFile,
  PickerTutorialFolder,
};

enum class Language {
  French,
  German,
  English,
  Spanish,
  Italian,
  Japanese,
};

struct SourceSelection {
  std::filesystem::path game;
  std::filesystem::path update;
  std::filesystem::path dlc;
};

ImU32 Color(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }

float Saturate(float value) { return std::clamp(value, 0.0f, 1.0f); }

float EaseInOut(float value) {
  const float t = Saturate(value);
  return 0.5f - 0.5f * std::cos(t * kPi);
}

std::string PathString(const std::filesystem::path& path) {
  return path.string();
}

std::string LowerPath(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string HexHash(uint64_t hash) {
  std::array<char, 19> text{};
  std::snprintf(text.data(), text.size(), "0x%016llx",
                static_cast<unsigned long long>(hash));
  return text.data();
}

const std::array<uint64_t, 1>* ExpectedHashesFor(const char* relative) {
  const auto path = LowerPath(relative);
  if (path == LowerPath(kEntrypointXex)) {
    return &kKnownEntrypointXexHashes;
  }
  if (path == LowerPath(kGameLogicDll)) {
    return &kKnownGameLogicDllHashes;
  }
  if (path == LowerPath(kEntrypointPatch)) {
    return &kKnownEntrypointPatchHashes;
  }
  if (path == LowerPath(kGameLogicPatch)) {
    return &kKnownGameLogicPatchHashes;
  }
  return nullptr;
}

std::string CanonicalInstallRelative(std::string relative) {
  const auto path = LowerPath(relative);
  if (path == LowerPath(kEntrypointXex)) {
    return kEntrypointXex;
  }
  if (path == LowerPath(kEntrypointPatch)) {
    return kEntrypointPatch;
  }
  if (path == LowerPath(kGameLogicDll)) {
    return kGameLogicDll;
  }
  if (path == LowerPath(kGameLogicPatch)) {
    return kGameLogicPatch;
  }
  return relative;
}

bool VerifyFileHash(const std::filesystem::path& source, const char* relative,
                    const std::vector<uint8_t>& bytes, std::string* error) {
  const auto* expected_hashes = ExpectedHashesFor(relative);
  if (!expected_hashes) {
    return true;
  }

  const uint64_t hash = XXH3_64bits(bytes.data(), bytes.size());
  if (std::find(expected_hashes->begin(), expected_hashes->end(), hash) !=
      expected_hashes->end()) {
    EOT_INFO("[installer-ui] verified {} from {} hash={}",
             relative, source.string(), HexHash(hash));
    return true;
  }

  *error = "The selected " + std::string(relative) +
           " did not match a known Edge of Time file.\n\n"
           "Please make sure all the specified files are correct and try again.";
  EOT_ERROR("[installer-ui] hash verification failed file={} source={} size={} hash={}",
            relative, source.string(), bytes.size(), HexHash(hash));
  return false;
}

bool SourceFileMatchesKnownHash(const std::filesystem::path& source,
                                const char* relative) {
  std::vector<uint8_t> bytes;
  std::string error;
  if (!LoadSourceFile(source, relative, &bytes, &error)) {
    return false;
  }

  const auto* expected_hashes = ExpectedHashesFor(relative);
  if (!expected_hashes) {
    return true;
  }

  const uint64_t hash = XXH3_64bits(bytes.data(), bytes.size());
  const bool matches =
      std::find(expected_hashes->begin(), expected_hashes->end(), hash) !=
      expected_hashes->end();
  if (!matches) {
    EOT_WARN("[installer-ui] rejected {} from {} hash={}",
             relative, source.string(), HexHash(hash));
  }
  return matches;
}

bool RegularFileMatchesHash(const std::filesystem::path& source,
                            const std::array<uint64_t, 1>& expected_hashes,
                            std::string* error) {
  std::ifstream in(source, std::ios::binary);
  if (!in) {
    if (error) {
      *error = "Unable to open " + source.string();
    }
    return false;
  }
  std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),
                             std::istreambuf_iterator<char>()};
  const uint64_t hash = XXH3_64bits(bytes.data(), bytes.size());
  if (std::find(expected_hashes.begin(), expected_hashes.end(), hash) !=
      expected_hashes.end()) {
    EOT_INFO("[installer-ui] verified package {} hash={}",
             source.string(), HexHash(hash));
    return true;
  }
  if (error) {
    *error = "The selected DLC package did not match the known Identity Crisis Suits package.";
  }
  EOT_ERROR("[installer-ui] DLC package hash verification failed file={} size={} hash={}",
            source.string(), bytes.size(), HexHash(hash));
  return false;
}

std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::vector<pid_t>& AudioPids() {
  static std::vector<pid_t> pids;
  return pids;
}

void ReapInstallerAudio() {
#if defined(__APPLE__)
  auto& pids = AudioPids();
  pids.erase(std::remove_if(pids.begin(), pids.end(),
                            [](pid_t pid) {
                              int status = 0;
                              const pid_t result = waitpid(pid, &status, WNOHANG);
                              return result == pid || result == -1;
                            }),
             pids.end());
#endif
}

void HandleAudioShutdownSignal(int signal) {
#if defined(__APPLE__)
  StopInstallerAudio();
  ::signal(signal, SIG_DFL);
  raise(signal);
#else
  (void)signal;
#endif
}

void RegisterAudioShutdown() {
#if defined(__APPLE__)
  static bool registered = false;
  if (!registered) {
    registered = true;
    std::atexit([] { StopInstallerAudio(); });
    ::signal(SIGINT, HandleAudioShutdownSignal);
    ::signal(SIGTERM, HandleAudioShutdownSignal);
    ::signal(SIGHUP, HandleAudioShutdownSignal);
  }
#endif
}

void TrackInstallerAudio(pid_t pid) {
#if defined(__APPLE__)
  if (pid <= 0) {
    return;
  }
  RegisterAudioShutdown();
  ReapInstallerAudio();
  AudioPids().push_back(pid);
#else
  (void)pid;
#endif
}

int LaunchResourceAudio(const std::filesystem::path& relative_path) {
#if defined(__APPLE__)
  const auto path = InstallerResourceRoot() / relative_path;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    return -1;
  }
  const std::string path_string = path.string();
  const std::string parent_pid = std::to_string(getpid());
  char program[] = "/bin/sh";
  char script[] =
      "trap 'kill \"$child\" 2>/dev/null; wait \"$child\" 2>/dev/null; exit 0' "
      "TERM INT HUP EXIT; "
      "afplay \"$1\" & child=$!; "
      "while kill -0 \"$2\" 2>/dev/null && kill -0 \"$child\" 2>/dev/null; "
      "do sleep 0.2; done; "
      "kill \"$child\" 2>/dev/null; "
      "wait \"$child\" 2>/dev/null";
  char arg0[] = "reeot-audio-supervisor";
  char* argv[] = {program,
                  const_cast<char*>("-c"),
                  script,
                  arg0,
                  const_cast<char*>(path_string.c_str()),
                  const_cast<char*>(parent_pid.c_str()),
                  nullptr};
  pid_t pid = -1;
  const int result = posix_spawnp(&pid, program, nullptr, nullptr, argv, environ);
  if (result != 0) {
    EOT_WARN("[installer-ui] failed to start audio {}: {}", path.string(), result);
    return -1;
  }
  TrackInstallerAudio(pid);
  EOT_DEBUG("[installer-ui] started audio pid={} path={}", pid, path.string());
  return static_cast<int>(pid);
#else
  (void)relative_path;
  return -1;
#endif
}

std::string Truncate(std::string text, size_t max_chars) {
  if (text.size() <= max_chars) {
    return text;
  }
  return text.substr(0, max_chars / 2) + "..." +
         text.substr(text.size() - (max_chars / 2));
}

std::filesystem::path SourceRootFor(const std::filesystem::path& selected) {
  std::error_code ec;
  if (std::filesystem::is_directory(selected, ec)) {
    return selected;
  }
  return selected.parent_path();
}

std::vector<std::filesystem::path> SourceCandidates(const std::filesystem::path& selected) {
  std::vector<std::filesystem::path> candidates;
  candidates.push_back(selected);

  std::error_code ec;
  if (std::filesystem::is_regular_file(selected, ec)) {
    candidates.push_back(selected.parent_path());
  } else if (std::filesystem::is_directory(selected, ec)) {
    for (const auto& entry : std::filesystem::directory_iterator(selected, ec)) {
      if (entry.is_regular_file(ec) || entry.is_directory(ec)) {
        candidates.push_back(entry.path());
      }
    }
  }

  return candidates;
}

bool HasFile(const std::filesystem::path& root, const char* relative) {
  return SourceHasFile(root, relative) && SourceFileMatchesKnownHash(root, relative);
}

bool HasGameFiles(const std::filesystem::path& root) {
  return HasFile(root, kEntrypointXex) && HasFile(root, kGameLogicDll);
}

bool HasPatchFiles(const std::filesystem::path& root) {
  return HasFile(root, kEntrypointPatch) && HasFile(root, kGameLogicPatch);
}

bool HasDlcFiles(const std::filesystem::path& root) {
  std::error_code ec;
  if (std::filesystem::is_regular_file(root, ec)) {
    std::string error;
    return root.filename() == kDlcPackageFileName &&
           RegularFileMatchesHash(root, kKnownDlcPackageHashes, &error) &&
           SourceTotalSize(root) > 0;
  }

  if (std::filesystem::is_directory(root, ec)) {
    return SourceTotalSize(root) > 0;
  }
  return false;
}

std::filesystem::path FindGameRoot(const std::filesystem::path& selected) {
  for (const auto& root : SourceCandidates(selected)) {
    if (HasGameFiles(root)) {
      return root;
    }
  }
  return {};
}

std::filesystem::path FindPatchRoot(const std::filesystem::path& selected) {
  for (const auto& root : SourceCandidates(selected)) {
    if (HasPatchFiles(root)) {
      return root;
    }
    const auto update = root / "Update";
    if (HasPatchFiles(update)) {
      return update;
    }
  }
  return {};
}

std::filesystem::path FindDlcRoot(const std::filesystem::path& selected) {
  for (const auto& root : SourceCandidates(selected)) {
    if (HasDlcFiles(root)) {
      return root;
    }
  }
  return {};
}

bool CopyOne(const std::filesystem::path& source, const char* relative,
             const std::filesystem::path& target_root, std::string* error) {
  const auto target = target_root / std::filesystem::path(relative);
  std::error_code ec;
  std::filesystem::create_directories(target.parent_path(), ec);
  if (ec) {
    *error = "Failed to create directory: " + target.parent_path().string();
    return false;
  }

  std::vector<uint8_t> bytes;
  if (!LoadSourceFile(source, relative, &bytes, error)) {
    return false;
  }
  if (!VerifyFileHash(source, relative, bytes, error)) {
    return false;
  }

  std::ofstream out(target, std::ios::binary);
  if (!out) {
    *error = "Failed to create " + target.string();
    return false;
  }
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!out) {
    *error = "Failed to write " + target.string();
    return false;
  }
  EOT_INFO("[installer-ui] copied {} from {} to {} ({} bytes)",
           relative, source.string(), target.string(), bytes.size());
  return true;
}

bool CreateRelativeDirectorySymlink(const std::filesystem::path& target,
                                    const std::filesystem::path& link,
                                    std::string* error) {
  std::error_code ec;
  std::filesystem::remove_all(link, ec);
  if (ec) {
    *error = "Failed to clean symlink target " + link.string() + ": " + ec.message();
    return false;
  }
  std::filesystem::create_directories(link.parent_path(), ec);
  if (ec) {
    *error = "Failed to create symlink parent " + link.parent_path().string() +
             ": " + ec.message();
    return false;
  }
  const auto relative_target = std::filesystem::relative(target, link.parent_path(), ec);
  if (ec) {
    *error = "Failed to compute relative symlink target: " + ec.message();
    return false;
  }
  std::filesystem::create_directory_symlink(relative_target, link, ec);
  if (ec) {
    *error = "Failed to create symlink " + link.string() + " -> " +
             relative_target.string() + ": " + ec.message();
    return false;
  }
  EOT_INFO("[installer-ui] created symlink {} -> {}", link.string(),
           relative_target.string());
  return true;
}

bool CopySourceTree(const std::filesystem::path& source,
                    const std::filesystem::path& target_root,
                    std::string* error) {
  std::vector<SourceFileInfo> files;
  if (!ListSourceFiles(source, &files, error)) {
    return false;
  }
  if (files.empty()) {
    *error = "No files were found in " + source.string();
    return false;
  }

  EOT_INFO("[installer-ui] copying source tree source={} target={} files={} bytes={}",
           source.string(), target_root.string(), files.size(), SourceTotalSize(source));
  for (const auto& file : files) {
    const auto relative = CanonicalInstallRelative(file.path);
    if (!CopyOne(source, relative.c_str(), target_root, error)) {
      return false;
    }
  }
  return true;
}

bool CopyDlcSource(const std::filesystem::path& source,
                   const InstallLayout& layout,
                   std::string* error) {
  if (source.empty()) {
    EOT_INFO("[installer-ui] no DLC source selected; skipping DLC install");
    return true;
  }

  const auto package_root =
      layout.root / kCommonContentXuid / kTitleId / kMarketplaceContentType /
      kDlcPackageFileName;
  std::error_code ec;
  std::filesystem::remove_all(package_root, ec);
  if (ec) {
    *error = "Failed to clean DLC directory: " + ec.message();
    return false;
  }

  std::string hash_error;
  if (std::filesystem::is_regular_file(source, ec) &&
      !RegularFileMatchesHash(source, kKnownDlcPackageHashes, &hash_error)) {
    *error = hash_error;
    return false;
  }

  if (!CopySourceTree(source, package_root, error)) {
    return false;
  }

  std::filesystem::create_directories(layout.dlc, ec);
  if (ec) {
    *error = "Failed to create DLC directory: " + ec.message();
    return false;
  }
  if (!CreateRelativeDirectorySymlink(package_root,
                                      layout.dlc / kDlcPackageFileName,
                                      error)) {
    return false;
  }
  if (!CreateRelativeDirectorySymlink(layout.dlc, layout.game / "DLC", error)) {
    return false;
  }

  EOT_INFO("[installer-ui] installed DLC source={} target={}",
           source.string(), package_root.string());
  return true;
}

bool PreparePatchedDirectory(const std::filesystem::path& game_root,
                             const std::filesystem::path& target_root,
                             std::string* error) {
  std::error_code ec;
  std::filesystem::remove_all(target_root, ec);
  if (ec) {
    *error = "Failed to clean patched directory: " + ec.message();
    return false;
  }
  std::filesystem::create_directories(target_root, ec);
  if (ec) {
    *error = "Failed to create patched directory: " + ec.message();
    return false;
  }

  std::filesystem::create_directories(target_root / "Data", ec);
  if (ec) {
    *error = "Failed to create patched Data directory: " + ec.message();
    return false;
  }

  const auto game_link = game_root / "patched";
  std::filesystem::remove_all(game_link, ec);
  if (ec) {
    *error = "Failed to clean game patched link: " + ec.message();
    return false;
  }
  std::filesystem::create_directory_symlink("../patched", game_link, ec);
  if (ec) {
    *error = "Failed to create game patched symlink: " + ec.message();
    return false;
  }
  EOT_INFO("[installer-ui] created game patched symlink {} -> ../patched",
           game_link.string());

  EOT_INFO("[installer-ui] prepared patched binary directory {}", target_root.string());
  return true;
}

std::string PatcherResultText(XexPatcher::Result result) {
  switch (result) {
    case XexPatcher::Result::Success:
      return "Success";
    case XexPatcher::Result::FileOpenFailed:
      return "FileOpenFailed";
    case XexPatcher::Result::FileWriteFailed:
      return "FileWriteFailed";
    case XexPatcher::Result::XexFileUnsupported:
      return "XexFileUnsupported";
    case XexPatcher::Result::XexFileInvalid:
      return "XexFileInvalid";
    case XexPatcher::Result::PatchFileInvalid:
      return "PatchFileInvalid";
    case XexPatcher::Result::PatchIncompatible:
      return "PatchIncompatible";
    case XexPatcher::Result::PatchFailed:
      return "PatchFailed";
    case XexPatcher::Result::PatchUnsupported:
      return "PatchUnsupported";
  }
  return "Unknown";
}

bool ApplyPatchFile(const std::filesystem::path& base,
                    const std::filesystem::path& patch,
                    const std::filesystem::path& patched,
                    std::string* error) {
  std::error_code ec;
  std::filesystem::create_directories(patched.parent_path(), ec);
  if (ec) {
    *error = "Failed to create patched directory: " + ec.message();
    return false;
  }
  std::filesystem::remove(patched, ec);
  ec.clear();

  EOT_INFO("[installer-ui] applying patch base={} patch={} output={}",
           base.string(), patch.string(), patched.string());
  const auto result = XexPatcher::apply(base, patch, patched);
  if (result != XexPatcher::Result::Success) {
    *error = "Patch process failed for " + base.filename().string() +
             " using " + patch.filename().string() + ": " +
             PatcherResultText(result);
    EOT_ERROR("[installer-ui] patch failed base={} patch={} result={}",
              base.string(), patch.string(), PatcherResultText(result));
    return false;
  }
  EOT_INFO("[installer-ui] patch succeeded output={}", patched.string());
  return true;
}

bool CopyInstallSources(const SourceSelection& selection, const InstallLayout& layout,
                        std::string* error) {
  if (selection.game.empty() || selection.update.empty()) {
    *error = "Some of the files that have\nbeen provided are not valid.\n\nPlease make sure all the\nspecified files are correct\nand try again.";
    return false;
  }

  std::error_code ec;
  std::filesystem::create_directories(layout.game, ec);
  std::filesystem::create_directories(layout.update, ec);
  if (ec) {
    *error = "Failed to create install directories: " + ec.message();
    return false;
  }

  if (!CopySourceTree(selection.game, layout.game, error) ||
      !CopyOne(selection.update, kGameLogicPatch, layout.update, error) ||
      !CopyOne(selection.update, kEntrypointPatch, layout.update, error) ||
      !CopyDlcSource(selection.dlc, layout, error) ||
      !PreparePatchedDirectory(layout.game, layout.patched, error)) {
    return false;
  }

  return ApplyPatchFile(layout.game / kEntrypointXex,
                        layout.update / kEntrypointPatch,
                        layout.patched / kPatchedEntrypointXex,
                        error) &&
         ApplyPatchFile(layout.game / kGameLogicDll,
                        layout.update / kGameLogicPatch,
                        layout.patched / kPatchedGameLogicDll,
                        error);
}

std::vector<std::filesystem::path> RunNativePicker(bool folder_mode) {
  std::vector<std::filesystem::path> paths;

  const nfdpathset_t* path_set = nullptr;
  nfdresult_t result = folder_mode ? NFD_PickFolderMultipleN(&path_set, nullptr)
                                   : NFD_OpenDialogMultipleN(&path_set, nullptr, 0, nullptr);
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR) {
      EOT_ERROR("[installer-ui] native picker failed: {}", NFD_GetError());
    } else {
      EOT_INFO("[installer-ui] native picker canceled");
    }
    return paths;
  }

  nfdpathsetsize_t count = 0;
  if (NFD_PathSet_GetCount(path_set, &count) == NFD_OKAY) {
    for (nfdpathsetsize_t i = 0; i < count; ++i) {
      nfdnchar_t* path = nullptr;
      if (NFD_PathSet_GetPathN(path_set, i, &path) == NFD_OKAY && path) {
        paths.emplace_back(path);
        EOT_INFO("[installer-ui] native picker returned path={}", std::filesystem::path(path).string());
        NFD_PathSet_FreePathN(path);
      }
    }
  }
  NFD_PathSet_Free(path_set);
  return paths;
}

struct TextureAsset {
  std::unique_ptr<rex::ui::ImmediateTexture> texture;
  uint32_t width = 0;
  uint32_t height = 0;

  explicit operator bool() const { return texture != nullptr; }
};

class InstallerDialog final : public rex::ui::ImGuiDialog {
 public:
  InstallerDialog(rex::ui::ImGuiDrawer* drawer,
                  rex::ui::ImmediateDrawer* immediate_drawer,
                  rex::PathConfig defaults,
                  std::filesystem::path config_path, StartupResult initial_result,
                  std::function<void(rex::PathConfig)> resume)
      : rex::ui::ImGuiDialog(drawer),
        immediate_drawer_(immediate_drawer),
        defaults_(std::move(defaults)),
        config_path_(std::move(config_path)),
        result_(std::move(initial_result)),
        resume_(std::move(resume)) {
    if (NFD_Init() == NFD_OKAY) {
      nfd_ready_ = true;
    } else {
      EOT_ERROR("[installer-ui] NFD_Init failed: {}", NFD_GetError());
    }
  }

  ~InstallerDialog() override {
    StopAudio();
  }

 protected:
  void OnClose() override {
    StopAudio();
    if (nfd_ready_) {
      NFD_Quit();
      nfd_ready_ = false;
    }
    EOT_INFO("[installer-ui] closed installer");
  }

  void OnDraw(ImGuiIO& io) override {
    if (!has_appeared_) {
      has_appeared_ = true;
      appear_time_ = ImGui::GetTime();
      LoadTextures();
      PlayMusic();
      EOT_INFO("[installer-ui] showing Unleashed-style wizard target={}",
               result_.install_layout.root.string());
    }

    UpdateCanvas(io.DisplaySize);
    HandleKeyboard();

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##reeot_installer_wizard", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);

    draw_ = ImGui::GetWindowDrawList();
    DrawBackground();
    DrawLeftImage();
    DrawScanlineBars();
    DrawDescriptionContainer();
    DrawPageControls();
    DrawInstallingProgress();
    DrawNavigationButton();
    DrawBorders();
    DrawButtonGuide();
    DrawMessagePrompt();

    if (is_disappearing_ && ImGui::GetTime() >
                                disappear_time_ + kAllAnimationsFullDuration / 60.0 +
                                    (is_quitting_ ? kQuittingExtraDuration / 60.0 : 0.0)) {
      if (is_quitting_) {
        Close();
      } else {
        CompleteAndResume();
      }
    }

    ImGui::End();
  }

 private:
  void UpdateCanvas(ImVec2 display) {
    scale_ = std::min(display.x / kCanvasWidth, display.y / kCanvasHeight);
    if (scale_ <= 0.0f || !std::isfinite(scale_)) {
      scale_ = 1.0f;
    }
    offset_ = {(display.x - kCanvasWidth * scale_) * 0.5f,
               (display.y - kCanvasHeight * scale_) * 0.5f};
  }

  ImVec2 P(float x, float y) const {
    return {offset_.x + x * scale_, offset_.y + y * scale_};
  }
  float S(float value) const { return value * scale_; }

  float Motion(double offset_frames, double duration_frames) const {
    const double elapsed_frames = (ImGui::GetTime() - appear_time_) * 60.0;
    return EaseInOut(static_cast<float>((elapsed_frames - offset_frames) /
                                        duration_frames));
  }

  void HandleKeyboard() {
    if (!message_.empty()) {
      return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      Back();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
      Next();
    }
  }

  const char* HeaderText() const {
    return page_ == WizardPage::Installing ? "INSTALLING" : "INSTALLER";
  }

  const char* PageText() const {
    switch (page_) {
      case WizardPage::SelectLanguage:
        return "Please select a language.";
      case WizardPage::Introduction:
        return "Welcome to\nEdge of Time Recompiled!\n\nYou'll need an Xbox 360 copy\nof Spider-Man: Edge of Time in order to proceed with the installation.";
      case WizardPage::SelectGameAndUpdate:
        return "Add the sources for the game and its title update.";
      case WizardPage::SelectDLC:
        return "Add the source for the optional\nIdentity Crisis Suits DLC.";
      case WizardPage::CheckSpace:
        return "The content will be installed to the program's folder.\n\n";
      case WizardPage::Installing:
        return "Please wait while the content is being installed...";
      case WizardPage::InstallSucceeded:
        return "Installation complete!\nThis project is brought to you by:";
      case WizardPage::InstallFailed:
        return "Installation failed.\n\nError: ";
    }
    return "";
  }

  void DrawBackground() {
    draw_->AddRectFilled(P(0.0f, 0.0f), P(kCanvasWidth, kCanvasHeight), Color(0, 0, 0));
  }

  void DrawLeftImage() {
    const float alpha = Motion(kImageAnimationTime, kImageAnimationDuration);
    const ImVec2 min = P(kImageX, kImageY);
    const ImVec2 max = P(kImageX + kImageWidth, kImageY + kImageHeight);
    draw_->AddRectFilled(min, max, Color(3, 7, 4, static_cast<int>(255 * alpha)));

    const int texture_index = TextureIndex();
    if (install_textures_[texture_index]) {
      draw_->AddImage(reinterpret_cast<ImTextureID>(install_textures_[texture_index].texture.get()),
                      min, max, {0.0f, 0.0f}, {1.0f, 1.0f},
                      Color(255, 255, 255, static_cast<int>(255 * alpha)));
      return;
    }

    const ImU32 colors[] = {
        Color(39, 124, 45, 180), Color(36, 90, 130, 180), Color(130, 104, 30, 180),
        Color(86, 53, 135, 180), Color(33, 116, 90, 180), Color(135, 55, 38, 180),
        Color(108, 108, 108, 180), Color(55, 80, 143, 180),
    };
    draw_->AddRectFilledMultiColor(min, max, colors[texture_index], Color(0, 0, 0, 210),
                                   Color(0, 0, 0, 240), colors[(texture_index + 2) % 8]);
    DrawGrid(min, max, Color(203, 255, 0, static_cast<int>(38 * alpha)), S(16.0f));

    const ImVec2 center = {min.x + S(256.0f), min.y + S(256.0f)};
    const float pulse = 0.65f + 0.35f * std::sin(static_cast<float>(ImGui::GetTime() * 4.0));
    for (int i = 0; i < 10; ++i) {
      const float angle = (static_cast<float>(i) / 10.0f +
                           static_cast<float>(ImGui::GetTime()) * 0.035f) *
                          kPi * 2.0f;
      draw_->AddLine(center,
                     {center.x + std::cos(angle) * S(230.0f),
                      center.y + std::sin(angle) * S(230.0f)},
                     Color(203, 255, 0, static_cast<int>(24 * alpha)), S(2.0f));
    }
    draw_->AddCircle(center, S(80.0f + pulse * 14.0f), Color(203, 255, 0, 80), 96,
                     S(3.0f));
    DrawOutlinedText({center.x - S(45.0f), center.y - S(19.0f)}, "EOT",
                     Color(255, 195, 0, static_cast<int>(255 * alpha)), 2.7f);
  }

  void LoadTextures() {
    if (!immediate_drawer_) {
      EOT_WARN("[installer-ui] immediate drawer unavailable; installer images disabled");
      return;
    }
    for (size_t i = 0; i < install_textures_.size(); ++i) {
      char name[64];
      std::snprintf(name, sizeof(name), "images/installer/raw/install_%03zu.png", i + 1);
      install_textures_[i] = LoadPngTexture(name);
      if (!install_textures_[i]) {
        EOT_WARN("[installer-ui] failed to load installer texture {}", name);
      }
    }
    ProbeDds("images/installer/install_001.dds");
  }

  TextureAsset LoadPngTexture(const std::filesystem::path& relative_path) {
    TextureAsset asset;
    const auto path = InstallerResourceRoot() / relative_path;
    const auto bytes = ReadBytes(path);
    if (bytes.empty()) {
      return asset;
    }
    int width = 0;
    int height = 0;
    auto rgba = rex::ui::DecodeImageRGBA(bytes.data(), bytes.size(), width, height);
    if (rgba.empty() || width <= 0 || height <= 0) {
      return asset;
    }
    asset.texture = immediate_drawer_->CreateTexture(
        static_cast<uint32_t>(width), static_cast<uint32_t>(height),
        rex::ui::ImmediateTextureFilter::kLinear, false, rgba.data());
    if (asset.texture) {
      asset.width = static_cast<uint32_t>(width);
      asset.height = static_cast<uint32_t>(height);
    }
    return asset;
  }

  void ProbeDds(const std::filesystem::path& relative_path) {
    const auto bytes = ReadBytes(InstallerResourceRoot() / relative_path);
    if (bytes.empty()) {
      return;
    }
    ddspp::Descriptor descriptor{};
    if (ddspp::decode_header(bytes.data(), descriptor) == ddspp::Success) {
      EOT_INFO("[installer-ui] DDS available {}: {}x{} format={} (compressed upload not "
               "supported by ImmediateTexture)",
               relative_path.string(), descriptor.width, descriptor.height,
               static_cast<int>(descriptor.format));
    }
  }

  int TextureIndex() const {
    static constexpr int kPageTexture[] = {0, 0, 1, 2, 3, 4, 7, 5};
    int index = kPageTexture[static_cast<int>(page_)];
    if (page_ == WizardPage::Installing) {
      index += static_cast<int>((ImGui::GetTime() - install_start_time_) / 15.0);
    }
    return index % 8;
  }

  void DrawScanlineBars() {
    const float motion = Motion(kScanlinesAnimationTime, kScanlinesAnimationDuration);
    const float height = S(105.0f) * motion;
    if (height <= 0.0f) {
      return;
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    DrawScanlineBand({0.0f, 0.0f}, {display.x, height}, motion);
    DrawScanlineBand({display.x, display.y}, {0.0f, display.y - height}, motion);

    const float title_alpha = Motion(kTitleAnimationTime, kTitleAnimationDuration);
    const float breathe =
        page_ == WizardPage::Installing
            ? 0.55f + 0.45f * std::sin(static_cast<float>(ImGui::GetTime() * 4.18))
            : 1.0f;
    DrawOutlinedText(P(288.0f, 54.5f), HeaderText(),
                     Color(255, 195, 0, static_cast<int>(255 * title_alpha * breathe)),
                     3.2f);
    DrawOutlinedText(P(256.0f, 80.0f), page_ == WizardPage::Installing ? "..." : "*",
                     Color(255, 255, 255, static_cast<int>(235 * title_alpha)), 2.0f);
  }

  void DrawScanlineBand(ImVec2 min, ImVec2 max, float alpha) {
    draw_->AddRectFilledMultiColor(min, max, Color(203, 255, 0, 0),
                                   Color(203, 255, 0, 0),
                                   Color(203, 255, 0, static_cast<int>(55 * alpha)),
                                   Color(203, 255, 0, static_cast<int>(55 * alpha)));
    const float y0 = std::min(min.y, max.y);
    const float y1 = std::max(min.y, max.y);
    for (float y = y0; y <= y1; y += S(4.0f)) {
      draw_->AddLine({0.0f, y}, {ImGui::GetIO().DisplaySize.x, y},
                     Color(203, 255, 0, static_cast<int>(35 * alpha)), S(1.0f));
    }
  }

  void DrawDescriptionContainer() {
    const ImVec2 desc_min = P(kContainerX + 0.5f, kContainerY + 0.5f);
    const ImVec2 desc_max =
        P(kContainerX + 0.5f + kContainerWidth, kContainerY + 0.5f + kContainerHeight);
    const ImVec2 side_min = {desc_max.x, desc_min.y};
    const ImVec2 side_max = {ImGui::GetIO().DisplaySize.x, desc_max.y};
    DrawContainer(desc_min, desc_max, true);
    DrawContainer(side_min, side_max, false);

    std::string text = PageText();
    if (page_ == WizardPage::CheckSpace) {
      const double required = RequiredBytes() / (1024.0 * 1024.0 * 1024.0);
      const double available = AvailableBytes() / (1024.0 * 1024.0 * 1024.0);
      char extra[160];
      std::snprintf(extra, sizeof(extra), "Required space: %2.2f GiB\nAvailable space: %2.2f GiB",
                    required, available);
      text += extra;
    } else if (page_ == WizardPage::InstallFailed) {
      text += install_error_;
    }

    DrawWrappedText(P(kContainerX + 28.0f, kContainerY + 26.0f), text,
                    S(kContainerWidth - 50.0f), 1.35f,
                    Color(255, 255, 255,
                          static_cast<int>(255 * Motion(kContainerInnerTime,
                                                        kContainerInnerDuration))));

    if (page_ == WizardPage::InstallSucceeded) {
      DrawOutlinedText(P(kContainerX + 176.0f, kContainerY + 150.0f), "hedge-dev",
                       Color(255, 255, 255), 1.35f);
      DrawWrappedText(P(kContainerX + 20.0f, kContainerY + 205.0f),
                      "Tom Clay  HyperBE32  goliath-tilers", S(kContainerWidth - 40.0f),
                      0.85f, Color(255, 255, 255, 210));
    }
  }

  void DrawContainer(ImVec2 min, ImVec2 max, bool text_area) {
    const float alpha = Motion(text_area ? kContainerInnerTime : kContainerOuterTime,
                               text_area ? kContainerInnerDuration : kContainerOuterDuration);
    draw_->AddRectFilled(min, max,
                         text_area ? Color(0, 32, 0, static_cast<int>(128 * alpha))
                                   : Color(0, 33, 0, static_cast<int>(180 * alpha)));
    DrawGrid(min, max, Color(0, 80, 0, static_cast<int>(115 * alpha)), S(kGridSize));
  }

  void DrawGrid(ImVec2 min, ImVec2 max, ImU32 color, float step) {
    for (float x = min.x; x <= max.x; x += step) {
      draw_->AddLine({x, min.y}, {x, max.y}, color, S(1.0f));
    }
    for (float y = min.y; y <= max.y; y += step) {
      draw_->AddLine({min.x, y}, {max.x, y}, color, S(1.0f));
    }
  }

  void DrawPageControls() {
    switch (page_) {
      case WizardPage::SelectLanguage:
        DrawLanguagePicker();
        break;
      case WizardPage::SelectGameAndUpdate:
        DrawSourcePickers();
        DrawSourceButton(0, 0, "GAME", !sources_.game.empty());
        DrawSourceButton(2, 0, "UPDATE", !sources_.update.empty());
        break;
      case WizardPage::SelectDLC:
        DrawSourcePickers();
        DrawDlcSources();
        break;
      default:
        break;
    }
  }

  void DrawLanguagePicker() {
    static constexpr std::array<const char*, 6> kLanguageText = {
        "FRANÇAIS", "DEUTSCH", "ENGLISH", "ESPAÑOL", "ITALIANO", "日本語"};
    for (int i = 0; i < 6; ++i) {
      const int column = i < 3 ? 0 : 2;
      const int row = i % 3;
      bool pressed = false;
      DrawButton(ButtonRect(column, row), kLanguageText[i], true, pressed, i == 2);
      DrawToggleLight(ButtonLightPos(column, row), static_cast<int>(language_) == i);
      if (pressed) {
        language_ = static_cast<Language>(i);
        PlaySound("sys_worldmap_cursor");
        EOT_INFO("[installer-ui] language selected index={}", i);
      }
    }
  }

  void DrawSourcePickers() {
    bool add_files = false;
    bool add_folder = false;
    const float y = kContainerY + kContainerHeight + kBottomYGap;
    DrawAutoWidthButton(P(kContainerX + kBottomXGap, y), "ADD FILES", add_files);
    DrawAutoWidthButton(P(kContainerX + kBottomXGap + 150.0f, y), "ADD FOLDER", add_folder);
    if (add_files) {
      ShowPickerTutorial(false);
    }
    if (add_folder) {
      ShowPickerTutorial(true);
    }
  }

  void DrawDlcSources() {
    DrawSourceButton(0, 0, kDlcDisplayName, !sources_.dlc.empty());
  }

  ImVec2 ButtonMin(int column, int row) const {
    float min_x = kContainerX + kContainerButtonGap;
    if (column == 1) {
      min_x = kContainerX + kContainerWidth / 2.0f - kContainerButtonWidth / 2.0f;
    } else if (column == 2) {
      min_x = kContainerX + kContainerWidth - kContainerButtonGap - kContainerButtonWidth;
    }
    const float minus_y = (kContainerButtonGap + kButtonHeight) * row;
    return P(min_x, kContainerY + kContainerHeight - kContainerButtonGap - kButtonHeight -
                        minus_y);
  }

  ImVec4 ButtonRect(int column, int row) const {
    const ImVec2 min = ButtonMin(column, row);
    return {min.x, min.y, min.x + S(kContainerButtonWidth), min.y + S(kButtonHeight)};
  }

  ImVec2 ButtonLightPos(int column, int row) const {
    const ImVec2 min = ButtonMin(column, row);
    return {min.x + S(14.0f), min.y + S(12.0f)};
  }

  void DrawSourceButton(int column, int row, const char* text, bool set) {
    bool ignored = false;
    DrawButton(ButtonRect(column, row), text, set, ignored, false, true);
    DrawToggleLight(ButtonLightPos(column, row), set);
  }

  void DrawInstallingProgress() {
    if (page_ != WizardPage::Installing) {
      return;
    }
    install_progress_ += std::min(install_progress_target_ - install_progress_,
                                  0.25f * ImGui::GetIO().DeltaTime);
    DrawProgressBar(install_progress_);
    if (install_finished_ && ImGui::GetTime() - install_start_time_ > 1.0) {
      page_ = install_failed_ ? WizardPage::InstallFailed : WizardPage::InstallSucceeded;
      install_end_time_ = ImGui::GetTime();
    }
  }

  void DrawProgressBar(float ratio) {
    const ImVec2 min =
        P(kContainerX + kBottomXGap + 1.0f, kContainerY + kContainerHeight + kBottomYGap);
    const ImVec2 max = P(kContainerX + kContainerWidth - kBottomXGap,
                         kContainerY + kContainerHeight + kBottomYGap + kButtonHeight);
    DrawButtonContainer(min, max, true);
    const ImVec2 inner_min = {min.x + S(5.0f), min.y + S(4.5f)};
    const ImVec2 inner_max = {max.x - S(5.0f), max.y - S(4.5f)};
    draw_->AddRectFilledMultiColor(inner_min, inner_max, Color(0, 65, 0), Color(0, 65, 0),
                                   Color(0, 32, 0), Color(0, 32, 0));
    ImVec2 slider_max = inner_max;
    slider_max.x = inner_min.x + (inner_max.x - inner_min.x) * Saturate(ratio);
    draw_->AddRectFilledMultiColor(inner_min, slider_max, Color(57, 241, 0),
                                   Color(57, 241, 0), Color(2, 106, 0),
                                   Color(2, 106, 0));
  }

  void DrawNavigationButton() {
    if (page_ == WizardPage::Installing) {
      return;
    }
    std::string text = "NEXT";
    if (page_ == WizardPage::SelectDLC && DlcSelectionsEmpty()) {
      text = "SKIP";
    } else if (page_ == WizardPage::InstallFailed) {
      text = "RETRY";
    }

    const float width = std::max(86.0f, 18.0f + text.size() * 14.0f);
    const ImVec2 min =
        P(kContainerX + kContainerWidth - width - kBottomXGap, kContainerY + kContainerHeight +
                                                               kBottomYGap);
    bool pressed = false;
    DrawButton({min.x, min.y, min.x + S(width), min.y + S(kButtonHeight)}, text.c_str(),
               NextEnabled(), pressed);
    if (pressed) {
      Next();
    }
  }

  bool NextEnabled() const {
    return !is_disappearing_ &&
           (page_ != WizardPage::SelectGameAndUpdate ||
            (!sources_.game.empty() && !sources_.update.empty()));
  }

  void DrawBorders() {
    const float border_motion =
        1.0f - Motion(kContainerLineAnimationTime, kContainerLineAnimationDuration);
    const float mid_x = offset_.x + S(kContainerX + kContainerWidth / 5.0f);
    const float min_x = std::lerp(offset_.x + S(kContainerX - kBorderSize - kBorderOvershoot),
                                  mid_x, border_motion);
    const float max_x = std::lerp(
        offset_.x + S(kContainerX + kContainerWidth + kSideContainerWidth + kBorderOvershoot),
        mid_x, border_motion);
    const float top_y = offset_.y + S(kContainerY - kBorderSize);
    const float bottom_y = offset_.y + S(kContainerY + kContainerHeight);
    draw_->AddLine({min_x, top_y}, {max_x, top_y}, Color(155, 200, 155), S(1.0f));
    draw_->AddLine({min_x, bottom_y}, {max_x, bottom_y}, Color(155, 225, 155), S(1.0f));

    const float mid_y = offset_.y + S(kContainerY + kContainerHeight / 2.0f);
    const float min_y = std::lerp(offset_.y + S(kContainerY - kBorderOvershoot), mid_y,
                                  border_motion);
    const float max_y = std::lerp(offset_.y + S(kContainerY + kContainerHeight + kBorderOvershoot),
                                  mid_y, border_motion);
    draw_->AddLine(P(kContainerX - kBorderSize, kContainerY), {P(kContainerX, 0).x, max_y},
                   Color(155, 155, 155), S(1.0f));
    draw_->AddLine({P(kContainerX + kContainerWidth, 0).x, min_y},
                   {P(kContainerX + kContainerWidth, 0).x, max_y}, Color(155, 225, 155),
                   S(1.0f));
  }

  void DrawButtonGuide() {
    const char* back = (page_ == first_page_ || page_ == WizardPage::InstallFailed) ? "QUIT"
                                                                                     : "BACK";
    if (page_ == WizardPage::Installing) {
      back = "CANCEL";
    }
    DrawOutlinedText(P(1010.0f, 650.0f), "SELECT", Color(210, 255, 130, 210), 0.9f);
    DrawOutlinedText(P(1115.0f, 650.0f), back, Color(210, 255, 130, 210), 0.9f);
  }

  void DrawMessagePrompt() {
    if (message_.empty()) {
      return;
    }
    drawing_message_prompt_ = true;
    draw_->AddRectFilled(P(0, 0), P(kCanvasWidth, kCanvasHeight), Color(0, 0, 0, 190));
    const ImVec2 min = P(420.0f, 220.0f);
    const ImVec2 max = P(860.0f, 500.0f);
    draw_->AddRectFilled(min, max, Color(0, 28, 0, 245));
    DrawGrid(min, max, Color(0, 110, 0, 80), S(kGridSize));
    draw_->AddRect(min, max, Color(155, 225, 155), 0.0f, 0, S(1.0f));
    DrawWrappedText(P(450.0f, 250.0f), message_, S(380.0f), 1.05f, Color(255, 255, 255));
    bool yes = false;
    bool no = false;
    if (message_confirmation_) {
      DrawButton({P(510, 450).x, P(510, 450).y, P(625, 472).x, P(625, 472).y}, "YES",
                 true, yes);
      DrawButton({P(650, 450).x, P(650, 450).y, P(765, 472).x, P(765, 472).y}, "NO",
                 true, no);
    } else {
      DrawButton({P(585, 450).x, P(585, 450).y, P(700, 472).x, P(700, 472).y}, "OK",
                 true, yes);
    }
    if (yes || no) {
      PlaySound(yes ? "sys_worldmap_finaldecide" : "sys_actstg_pausecansel");
      const bool accepted = yes;
      const MessageSource source = message_source_;
      EOT_INFO("[installer-ui] message dismissed accepted={} source={}",
               accepted, static_cast<int>(source));
      ClearMessage();
      if (accepted) {
        if (source == MessageSource::Back) {
          is_quitting_ = true;
          BeginDisappear();
        } else if (source == MessageSource::Next) {
          page_ = WizardPage::CheckSpace;
        } else if (source == MessageSource::PickerTutorialFile) {
          RunPicker(false);
        } else if (source == MessageSource::PickerTutorialFolder) {
          RunPicker(true);
        }
      }
    }
    drawing_message_prompt_ = false;
  }

  void DrawAutoWidthButton(ImVec2 min, const char* text, bool& pressed) {
    const float width = std::max(120.0f, 20.0f + std::strlen(text) * 11.0f);
    DrawButton({min.x, min.y, min.x + S(width), min.y + S(kButtonHeight)}, text, true,
               pressed);
  }

  void DrawButton(ImVec4 rect, const char* text, bool enabled, bool& pressed,
                  bool make_default = false, bool source_button = false) {
    pressed = false;
    const ImVec2 min = {rect.x, rect.y};
    const ImVec2 max = {rect.z, rect.w};
    ImGui::PushID(text);
    ImGui::SetCursorScreenPos(min);
    if (enabled && !source_button && (message_.empty() || drawing_message_prompt_)) {
      pressed = ImGui::InvisibleButton("##button", {max.x - min.x, max.y - min.y});
    } else {
      ImGui::Dummy({max.x - min.x, max.y - min.y});
    }
    const bool hovered = enabled && ImGui::IsItemHovered();
    if (hovered && hovered_button_ != text) {
      hovered_button_ = text;
      PlaySound("sys_worldmap_cursor");
    }
    DrawButtonContainer(min, max, hovered || make_default);
    const int alpha = enabled ? 255 : 128;
    DrawOutlinedText({min.x + (max.x - min.x) * 0.5f, min.y + S(1.0f)}, text,
                     Color(255, 255, 255, alpha), source_button ? 0.82f : 1.0f, true);
    ImGui::PopID();
  }

  void DrawButtonContainer(ImVec2 min, ImVec2 max, bool selected) {
    const int baser = selected ? 48 : 0;
    const int baseg = selected ? 32 : 0;
    draw_->AddRectFilledMultiColor(min, max, Color(baser, baseg + 130, 0, 223),
                                   Color(baser, baseg + 130, 0, 178),
                                   Color(baser, baseg + 130, 0, 223),
                                   Color(baser, baseg + 130, 0, 178));
    for (float y = min.y; y < max.y; y += S(4.0f)) {
      draw_->AddLine({min.x, y}, {max.x, y}, Color(0, 0, 0, 35), S(1.0f));
    }
  }

  void DrawToggleLight(ImVec2 center, bool enabled) {
    draw_->AddCircleFilled(center, S(6.0f),
                           enabled ? Color(203, 255, 0, 255) : Color(60, 90, 35, 140), 24);
    draw_->AddCircle(center, S(7.0f), Color(0, 0, 0, 220), 24, S(1.0f));
  }

  void DrawOutlinedText(ImVec2 pos, const char* text, ImU32 color, float font_scale,
                        bool centered = false) {
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize() * font_scale * scale_;
    ImVec2 draw_pos = pos;
    if (centered) {
      const ImVec2 text_size = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
      draw_pos.x -= text_size.x * 0.5f;
    }
    const float o = std::max(1.0f, scale_);
    draw_->AddText(font, size, {draw_pos.x - o, draw_pos.y}, Color(0, 0, 0), text);
    draw_->AddText(font, size, {draw_pos.x + o, draw_pos.y}, Color(0, 0, 0), text);
    draw_->AddText(font, size, {draw_pos.x, draw_pos.y - o}, Color(0, 0, 0), text);
    draw_->AddText(font, size, {draw_pos.x, draw_pos.y + o}, Color(0, 0, 0), text);
    draw_->AddText(font, size, draw_pos, color, text);
  }

  void DrawWrappedText(ImVec2 pos, const std::string& text, float width, float font_scale,
                       ImU32 color) {
    ImGui::PushFont(ImGui::GetFont());
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushTextWrapPos(pos.x + width);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::SetWindowFontScale(font_scale * scale_);
    ImGui::TextUnformatted(text.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
  }

  void Next() {
    if (!NextEnabled() || !message_.empty()) {
      return;
    }
    if (page_ == WizardPage::SelectDLC) {
      PlaySound("sys_worldmap_finaldecide");
      const bool skip = DlcSelectionsEmpty();
      if (!skip) {
        page_ = WizardPage::CheckSpace;
      } else if (sources_.game.empty()) {
        BeginDisappear();
      } else {
        ShowMessage("The Identity Crisis Suits DLC\nwas not selected.\n\nAre you sure you want to\nskip this step?",
                    true, MessageSource::Next);
      }
      return;
    }
    if (page_ == WizardPage::CheckSpace) {
      PlaySound("sys_worldmap_finaldecide");
      StartInstall();
      return;
    }
    if (page_ == WizardPage::InstallSucceeded) {
      BeginDisappear();
      return;
    }
    if (page_ == WizardPage::InstallFailed) {
      page_ = first_page_;
      return;
    }
    page_ = static_cast<WizardPage>(static_cast<int>(page_) + 1);
    PlaySound("sys_worldmap_finaldecide");
  }

  void Back() {
    if (!message_.empty() || page_ == WizardPage::InstallSucceeded) {
      return;
    }
    if (page_ == WizardPage::Installing) {
      PlaySound("sys_actstg_pausecansel");
      ShowMessage("Are you sure you want to cancel the installation?", true,
                  MessageSource::Back);
      return;
    }
    if (page_ == first_page_ || page_ == WizardPage::InstallFailed) {
      PlaySound("sys_actstg_pausecansel");
      ShowMessage("Are you sure you want to quit?", true, MessageSource::Back);
      return;
    }
    page_ = static_cast<WizardPage>(static_cast<int>(page_) - 1);
    PlaySound("sys_actstg_pausecansel");
  }

  void ShowPickerTutorial(bool folder) {
    EOT_INFO("[installer-ui] showing picker tutorial mode={}", folder ? "folder" : "file");
    ShowMessage(folder ? "Select a folder that contains the\nunmodified files that have been\nextracted from the game.\n\nThese files can be obtained from\nyour Xbox 360 hard drive by\nfollowing the instructions on\nthe GitHub page.\n\nFor choosing a digital dump,\nuse the \"Add Files\" option instead."
                       : "Select a digital dump with\ncontent from the game.\n\nThese files can be obtained from\nyour Xbox 360 hard drive by\nfollowing the instructions on\nthe GitHub page.\n\nFor choosing a folder with extracted\nand unmodified game files, use\nthe \"Add Folder\" option instead.",
                false, folder ? MessageSource::PickerTutorialFolder
                              : MessageSource::PickerTutorialFile);
  }

  void ShowMessage(std::string text, bool confirmation, MessageSource source) {
    message_ = std::move(text);
    message_confirmation_ = confirmation;
    message_source_ = source;
  }

  void ClearMessage() {
    message_.clear();
    message_confirmation_ = false;
    message_source_ = MessageSource::Unknown;
  }

  void RunPicker(bool folder) {
    EOT_INFO("[installer-ui] opening {} picker", folder ? "folder" : "file");
    if (!nfd_ready_) {
      ShowMessage("The native file picker is not available.", false,
                  MessageSource::Unknown);
      return;
    }
    auto paths = RunNativePicker(folder);
    if (paths.empty()) {
      EOT_INFO("[installer-ui] picker returned no paths");
      return;
    }

    std::vector<std::filesystem::path> failed;
    for (const auto& path : paths) {
      if (page_ == WizardPage::SelectGameAndUpdate) {
        if (auto game = FindGameRoot(path); !game.empty()) {
          sources_.game = game;
          EOT_INFO("[installer-ui] selected game source={}", game.string());
        } else if (auto update = FindPatchRoot(path); !update.empty()) {
          sources_.update = update;
          EOT_INFO("[installer-ui] selected update source={}", update.string());
        } else {
          failed.push_back(path);
        }
      } else if (page_ == WizardPage::SelectDLC) {
        if (auto dlc = FindDlcRoot(path); !dlc.empty()) {
          sources_.dlc = dlc;
          EOT_INFO("[installer-ui] selected DLC source={}", dlc.string());
        } else {
          failed.push_back(path);
        }
      }
    }

    if (!failed.empty()) {
      std::stringstream ss;
      ss << "The following selected files are invalid:";
      for (size_t i = 0; i < std::min<size_t>(failed.size(), 5); ++i) {
        ss << "\n\n- " << Truncate(failed[i].filename().string(), 32);
      }
      if (failed.size() > 5) {
        ss << "\n- [...]";
      }
      ShowMessage(ss.str(), false, MessageSource::Unknown);
    }
  }

  bool DlcSelectionsEmpty() const {
    return sources_.dlc.empty();
  }

  uintmax_t RequiredBytes() const {
    if (cached_game_size_source_ != sources_.game) {
      cached_game_size_source_ = sources_.game;
      cached_game_size_ = sources_.game.empty() ? 0 : SourceTotalSize(sources_.game);
    }
    if (cached_update_size_source_ != sources_.update) {
      cached_update_size_source_ = sources_.update;
      cached_update_size_ = sources_.update.empty()
                                ? 0
                                : SourceFileSize(sources_.update, kEntrypointPatch) +
                                      SourceFileSize(sources_.update, kGameLogicPatch);
    }
    static constexpr uintmax_t kPatchScratchBytes = 512ull * 1024ull * 1024ull;
    return cached_game_size_ + cached_update_size_ + kPatchScratchBytes +
           SourceFileSize(sources_.game, kEntrypointXex) +
           SourceFileSize(sources_.game, kGameLogicDll) +
           (sources_.dlc.empty() ? 0 : SourceTotalSize(sources_.dlc));
  }

  uintmax_t AvailableBytes() const {
    std::error_code ec;
    const auto info = std::filesystem::space(result_.install_layout.root, ec);
    return ec ? 0 : info.available;
  }

  void StartInstall() {
    page_ = WizardPage::Installing;
    PlaySound("sys_actstg_pausedecide");
    install_start_time_ = ImGui::GetTime();
    install_progress_ = 0.0f;
    install_progress_target_ = 0.25f;
    install_finished_ = false;
    install_failed_ = false;
    install_error_.clear();

    std::string error;
    EOT_INFO("[installer-ui] installing sources game={} update={} dlc={} target={}",
             sources_.game.string(), sources_.update.string(), sources_.dlc.string(),
             result_.install_layout.root.string());
    install_progress_target_ = 0.65f;
    if (!CopyInstallSources(sources_, result_.install_layout, &error)) {
      install_failed_ = true;
      install_error_ = error;
      EOT_ERROR("[installer-ui] install failed: {}", error);
    } else {
      InstallManifest manifest;
      manifest.installed = true;
      manifest.runtime_root = result_.install_layout.game.string();
      manifest.note = "Installed by the ReEoT first-run installer.";
      if (!SaveInstallManifest(result_.install_layout.manifest, manifest, &error)) {
        install_failed_ = true;
        install_error_ = error;
        EOT_ERROR("[installer-ui] manifest write failed: {}", error);
      }
    }

    if (!install_failed_) {
      rex::cvar::SetFlagByName(kCvEotDataRoot, result_.install_layout.game.string());
      REXCVAR_SET(skip_installer, true);
      rex::cvar::SaveConfig(config_path_);
      result_ = PrepareStartupPaths(defaults_, config_path_);
      if (!result_.ready) {
        install_failed_ = true;
        install_error_ = result_.message;
        EOT_ERROR("[installer-ui] install validation failed: {}", result_.message);
      }
    }
    install_progress_target_ = 1.0f;
    install_finished_ = true;
  }

  void BeginDisappear() {
    is_disappearing_ = true;
    disappear_time_ = ImGui::GetTime();
    PlaySound("sys_actstg_pausewinclose");
  }

  void PlayMusic() {
    if (music_started_) {
      return;
    }
    music_started_ = true;
    TrackAudio(LaunchResourceAudio("music/raw/installer.wav"));
  }

  void PlaySound(const char* name) {
    if (!name || !*name) {
      return;
    }
    if (std::strcmp(name, "sys_worldmap_cursor") == 0) {
      const double now = ImGui::GetTime();
      if (now - last_cursor_sound_time_ < 0.08) {
        return;
      }
      last_cursor_sound_time_ = now;
    }
    TrackAudio(LaunchResourceAudio(std::filesystem::path("sounds/raw") /
                                   (std::string(name) + ".wav")));
  }

  void TrackAudio(int pid) {
    (void)pid;
  }

  void StopAudio() {
    StopInstallerAudio();
  }

  void CompleteAndResume() {
    if (result_.ready) {
      auto paths = result_.paths;
      Close();
      if (resume_) {
        resume_(std::move(paths));
      }
    } else {
      Close();
    }
  }

  rex::ui::ImmediateDrawer* immediate_drawer_ = nullptr;
  rex::PathConfig defaults_;
  std::filesystem::path config_path_;
  StartupResult result_;
  std::function<void(rex::PathConfig)> resume_;
  ImDrawList* draw_ = nullptr;
  ImVec2 offset_ = {};
  float scale_ = 1.0f;
  double appear_time_ = 0.0;
  double disappear_time_ = 0.0;
  double install_start_time_ = 0.0;
  double install_end_time_ = 0.0;
  float install_progress_ = 0.0f;
  float install_progress_target_ = 0.0f;
  bool install_finished_ = false;
  bool install_failed_ = false;
  bool has_appeared_ = false;
  bool is_disappearing_ = false;
  bool is_quitting_ = false;
  bool nfd_ready_ = false;
  bool music_started_ = false;
  bool drawing_message_prompt_ = false;
  double last_cursor_sound_time_ = 0.0;
  WizardPage first_page_ = WizardPage::SelectLanguage;
  WizardPage page_ = WizardPage::SelectLanguage;
  Language language_ = Language::English;
  SourceSelection sources_;
  mutable std::filesystem::path cached_game_size_source_;
  mutable std::filesystem::path cached_update_size_source_;
  mutable uintmax_t cached_game_size_ = 0;
  mutable uintmax_t cached_update_size_ = 0;
  std::string install_error_;
  std::string message_;
  std::string hovered_button_;
  MessageSource message_source_ = MessageSource::Unknown;
  bool message_confirmation_ = false;
  std::array<TextureAsset, 8> install_textures_;
};

}  // namespace

void ShowInstallerDialog(rex::ui::ImGuiDrawer* drawer,
                         rex::ui::ImmediateDrawer* immediate_drawer,
                         rex::PathConfig defaults,
                         std::filesystem::path config_path,
                         StartupResult initial_result,
                         std::function<void(rex::PathConfig)> resume) {
  if (!drawer) {
    EOT_ERROR("[installer-ui] cannot show installer without ImGui drawer");
    return;
  }
  new InstallerDialog(drawer, immediate_drawer, std::move(defaults),
                      std::move(config_path), std::move(initial_result),
                      std::move(resume));
}

void StopInstallerAudio() {
#if defined(__APPLE__)
  ReapInstallerAudio();
  auto& pids = AudioPids();
  if (pids.empty()) {
    return;
  }

  for (pid_t pid : pids) {
    EOT_DEBUG("[installer-ui] stopping audio pid={}", pid);
    kill(pid, SIGTERM);
  }

  for (int attempt = 0; attempt < 20; ++attempt) {
    ReapInstallerAudio();
    if (pids.empty()) {
      return;
    }
    usleep(10000);
  }

  for (pid_t pid : pids) {
    EOT_WARN("[installer-ui] force stopping audio pid={}", pid);
    kill(pid, SIGKILL);
  }

  for (pid_t pid : pids) {
    int status = 0;
    waitpid(pid, &status, 0);
  }
  pids.clear();
#endif
}

}  // namespace eot::installer
