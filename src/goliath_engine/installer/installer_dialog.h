#pragma once

#include <functional>
#include <filesystem>
#include <string>

#include <rex/rex_app.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/immediate_drawer.h>

#include "goliath_engine/installer/installer.h"

namespace eot::installer {

void ShowInstallerDialog(rex::ui::ImGuiDrawer* drawer,
                         rex::ui::ImmediateDrawer* immediate_drawer,
                         rex::PathConfig defaults,
                         std::filesystem::path config_path,
                         StartupResult initial_result,
                         std::function<void(rex::PathConfig)> resume);

void StopInstallerAudio();

}  // namespace eot::installer
