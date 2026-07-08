/**
 * @file    goliath_engine/common/cvars.h
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 * @edit      Modifications Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
 *            All rights reserved.
 */
#pragma once

#include <cstdint>
#include <string>

#include <rex/cvar.h>

inline constexpr char kCvarGroup[] = "Blue Dragon";

// Names also consumed as raw strings (settings UIs, change-callback). Use these
// instead of string literals so a rename is a compile error.
inline constexpr char kCvDevmode[]           = "bd_devmode";
inline constexpr char kCvAspectRatio[]       = "bd_aspect_ratio";
inline constexpr char kCvRenderResolution[]  = "bd_render_resolution";
inline constexpr char kCvFpsLimit[]          = "bd_fps_limit";
inline constexpr char kCvVsync[]             = "bd_vsync";
inline constexpr char kCvSupersampling[]     = "bd_supersampling";
inline constexpr char kCvAnisotropy[]        = "bd_anisotropy";
inline constexpr char kCvShadowDimension[]   = "bd_shadow_dimension";
inline constexpr char kCvShadowDistance[]    = "bd_shadow_distance";
inline constexpr char kCvSkipInstaller[]     = "skip_installer";
inline constexpr char kCvEotDataRoot[]       = "eot_data_root";

REXCVAR_DECLARE(bool,    skip_installer);
REXCVAR_DECLARE(bool,    bd_devmode);
REXCVAR_DECLARE(bool,    bd_dbgprint);
REXCVAR_DECLARE(bool,    bd_mod_log);
REXCVAR_DECLARE(int32_t, bd_mod_log_keep);
REXCVAR_DECLARE(int32_t, bd_mod_log_max_mb);
REXCVAR_DECLARE(bool,    pso_precache);
REXCVAR_DECLARE(int32_t, bd_anisotropy);
REXCVAR_DECLARE(int32_t, bd_supersampling);
REXCVAR_DECLARE(int32_t, bd_shadow_dimension);
REXCVAR_DECLARE(double,  bd_shadow_distance);
REXCVAR_DECLARE(bool,    bd_bloom_mask_clamp);
REXCVAR_DECLARE(double,  bd_dof_strength);
REXCVAR_DECLARE(int32_t, bd_render_resolution);
REXCVAR_DECLARE(int32_t, bd_aspect_ratio);
REXCVAR_DECLARE(std::string, bd_language);
REXCVAR_DECLARE(int32_t, bd_fps_limit);
REXCVAR_DECLARE(bool,    bd_vsync);
REXCVAR_DECLARE(int32_t, bd_geometry_heap);
REXCVAR_DECLARE(int32_t, bd_diag_verbosity);
REXCVAR_DECLARE(int32_t, bd_force_constant_upload);
REXCVAR_DECLARE(int32_t, bd_diag_stale_constants);
