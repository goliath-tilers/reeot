// reeot - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include <imgui.h>
#include <rex/rex_app.h>
#include <rex/system.h>
#include <rex/ui/imgui_dialog.h>

#include "goliath_engine/common/cvars.h"
#include "goliath_engine/common/logging.h"
#include "goliath_engine/installer/install_constants.h"
#include "goliath_engine/installer/installer_dialog.h"
#include "goliath_engine/installer/installer.h"

class ReeotApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<ReeotApp>(new ReeotApp(ctx, "reeot",
        PPCImageConfig));
  }

  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)> resume) override {
    (void)resume;
    const auto config_path = eot::installer::InstallerConfigPath();
    EOT_INFO("[app] finalizing paths. config={}", config_path.string());
    auto startup = eot::installer::PrepareStartupPaths(defaults, config_path);
    if (!startup.ready) {
      EOT_WARN("[app] startup blocked by installer: {}", startup.message);
      if (imgui_drawer_) {
        eot::installer::ShowInstallerDialog(imgui_drawer_, immediate_drawer(), defaults,
                                            config_path, std::move(startup),
                                            std::move(resume));
      } else {
        EOT_ERROR("[app] ImGui drawer unavailable; falling back to native message box");
        rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error,
                                  startup.message);
      }
      return std::nullopt;
    }
    EOT_INFO("[app] startup paths ready. game={} update={} user={}",
             startup.paths.game_data_root.string(),
             startup.paths.update_data_root.string(),
             startup.paths.user_data_root.string());
    return std::move(startup.paths);
  }

  void OnLoadXexImage(std::string& xex_image) override {
    const auto root = std::filesystem::path(
        rex::cvar::GetFlagByName(kCvEotDataRoot));
    if (!root.empty()) {
      const auto mounted_patched = root / "patched" /
          eot::installer::kPatchedEntrypointXex;
      const auto local_patched = root / eot::installer::kPatchedEntrypointXex;
      std::error_code ec;
      if (std::filesystem::is_regular_file(mounted_patched, ec)) {
        xex_image = eot::installer::kMountedPatchedEntrypointXex;
        EOT_INFO("[app] loading patched XEX image: {}", xex_image);
        return;
      }
      ec.clear();
      if (std::filesystem::is_regular_file(local_patched, ec)) {
        xex_image = eot::installer::kMountedLocalPatchedEntrypointXex;
        EOT_INFO("[app] loading patched XEX image: {}", xex_image);
        return;
      }
    }

    xex_image = eot::installer::kMountedEntrypointXex;
    EOT_INFO("[app] loading default XEX image: {}", xex_image);
  }

  // Override virtual hooks for customization:
  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
      EOT_INFO("[app] gpu_plugin was empty; using xenos so installer UI can render");
    }
  }
  // void OnPostSetup() override {}
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    EOT_INFO("[app] ImGui drawer created for installer dialogs");
    imgui_drawer_ = drawer;
  }
  void OnShutdown() override {
    eot::installer::StopInstallerAudio();
  }
  void OnConfigureFonts(ImFontAtlas* atlas) override {
    const auto font_root = eot::installer::InstallerFontRoot();
    auto font_path = font_root / eot::installer::kSeuratFontFile;
    const bool using_seurat = std::filesystem::is_regular_file(font_path);
    if (!using_seurat) {
      font_path = font_root / eot::installer::kFallbackFontFile;
    }
    if (std::filesystem::is_regular_file(font_path)) {
      ImFontConfig config;
      config.OversampleH = config.OversampleV = 1;
      config.PixelSnapH = true;
      config.MergeMode = false;
      config.Name[0] = '\0';
      std::snprintf(config.Name, sizeof(config.Name), "%s",
                    using_seurat ? eot::installer::kSeuratFontFile
                                 : eot::installer::kFallbackFontFile);
      atlas->Clear();
      atlas->AddFontFromFileTTF(font_path.string().c_str(), 20.0f, &config,
                                atlas->GetGlyphRangesDefault());
      EOT_INFO("[app] configured installer font: {}{}",
               font_path.string(), using_seurat ? "" : " (fallback)");
    } else {
      EOT_WARN("[app] installer font not found under {}; using ReXGlue default font",
               font_root.string());
    }
  }
  // void OnConfigurePaths(rex::PathConfig& paths) override {}

 private:
  rex::ui::ImGuiDrawer* imgui_drawer_ = nullptr;
};
