/**
 * @file      goliath_engine/common/cvars.cpp
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 * @edit      Modifications Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
 *            All rights reserved.
 */
#include "goliath_engine/common/cvars.h"

#include <charconv>
#include <string_view>

REXCVAR_DEFINE_BOOL(
    skip_installer, false, "Installer",
    "Skip the ReEoT installer gate once a valid install has been prepared. "
    "The installer enables this after validation so normal launches go "
    "straight to the game.");

REXCVAR_DEFINE_BOOL(bd_devmode, false, kCvarGroup,
                    "Enable developer mode: debug menu boot, Mindows config "
                    "overlay (F11 to toggle), and keyboard bridge for debug "
                    "input. Off by default in retail.");
REXCVAR_DEFINE_BOOL(bd_dbgprint, false, kCvarGroup,
                    "Print DbgPrint output to host log");

REXCVAR_DEFINE_BOOL(bd_mod_log, false, kCvarGroup,
                    "Log file accesses to mods/file_access_summary.csv plus "
                    "per-session detail logs under mods/mod_access_logs/");
REXCVAR_DEFINE_INT32(bd_mod_log_keep, 10, kCvarGroup,
                     "Previous-session detail logs to retain at boot (the "
                     "active session writes one more)");
REXCVAR_DEFINE_INT32(bd_mod_log_max_mb, 64, kCvarGroup,
                     "Per-session detail log size cap in MB (0 = unlimited)");

REXCVAR_DEFINE_BOOL(
    pso_precache, true, kCvarGroup,
    "Master switch for the load-time PSO precache (background compiler pool + "
    "load-time predictor + boot-time residual replay). Off falls back to lazy "
    "draw-time PSO compilation.");

REXCVAR_DEFINE_INT32(
    bd_anisotropy, 16, kCvarGroup,
    "Anisotropic texture filtering")
    .range(0, 16);

REXCVAR_DEFINE_INT32(
    bd_supersampling, 2, kCvarGroup,
    "3D scene supersampling (anti-aliasing) factor, power of two. The scene "
    "renders at the output resolution x this factor, then resolves down as a "
    "clean integer downsample. 1 = no SSAA, 2/4 = progressively cleaner edges "
    "at GPU cost. Scene = output x factor, so a high factor on a high output "
    "resolution is large. Only 1/2/4 accepted. Applies live (the scene surface "
    "is recreated each frame).")
    .range(1, 4)
    .validator([](std::string_view v) {
      int n = 0;
      auto r = std::from_chars(v.data(), v.data() + v.size(), n);
      return r.ec == std::errc() && (n == 1 || n == 2 || n == 4);
    });

REXCVAR_DEFINE_INT32(
    bd_shadow_dimension, 4096, kCvarGroup,
    "Sun shadow-map resolution in pixels (square, power of two). Sets BOTH the "
    "render depth surface (Visual__UnitShadow__vf03) and its resolved "
    "sampleable copy (Visual__ShadowMapInfo__vf08) to this dimension so the "
    "resolve is 1:1 and the higher resolution reaches the receiver (sharper, "
    "less shadow-edge shimmer). 1024 = X360 native; 4096 default. Only "
    "512/1024/2048/4096/8192 accepted. Requires restart. The Settings Shadow "
    "Quality step sets this together with bd_shadow_distance to hold texel "
    "density as coverage grows.")
    .range(512, 8192)
    .validator([](std::string_view v) {
      int n = 0;
      auto r = std::from_chars(v.data(), v.data() + v.size(), n);
      return r.ec == std::errc() &&
             (n == 512 || n == 1024 || n == 2048 || n == 4096 || n == 8192);
    })
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_DOUBLE(
    bd_shadow_distance, 2.0, kCvarGroup,
    "Sun shadow draw distance: multiplier on the light-frustum coverage scale "
    "built in Visual__ShadowMapInfo__vf10. BD's dynamic coverage scale "
    "saturates at a half-extent of ~512 world units, so casters beyond that "
    "fall outside the shadow map and their shadows pop in as the camera nears. "
    "Both frustum modes converge on one projection call, so one multiplier "
    "widens both. The receiver PCF tap stride is recompiled to scale by "
    "1/distance (SharedConstants.shadowPcfScale) so the wider frustum keeps a "
    "constant world-space penumbra instead of combing the shadow edge; pair "
    "with bd_shadow_dimension (Settings Shadow Quality couples them) to hold "
    "texel density. 1.0 = X360 native. Applies live (density/kernel need the "
    "restart-gated dimension to land).")
    .range(1.0, 4.0);

REXCVAR_DEFINE_BOOL(
    bd_bloom_mask_clamp, true, kCvarGroup,
    "Clamp the bloom bright-mask shaders (bd_pe_ps_brightpass / "
    "bd_pe_ps_ms_bright) to [0,1] at the export, restoring the LDR saturation "
    "the X360 EDRAM tile applied before the bloom blur. Unclamped, maps with a "
    "high BLOOM BriMulti (wc01 ships 50) push mask values of ~59 into the FP16 "
    "chain and the gaussian blur spreads them into giant white blobs. false = "
    "faithful FP16 (blobby). Read once at shader link; needs restart.");

REXCVAR_DEFINE_DOUBLE(
    bd_dof_strength, 1.0, kCvarGroup,
    "Scale for the depth-of-field blur. mcl_dof_filter_vf00 derives the DOF "
    "circle-of-confusion from composite pixel-shader constant c27.y (blur "
    "scale); the naive full-image blur pyramid it drives bleeds the in-focus "
    "foreground into the blurred sky, ringing character/geometry silhouettes "
    "with a dark halo against clear sky. This multiplies c27.y on the DOF "
    "setup flush, shrinking the blur radius (the shader squares c27.y and "
    "scales by 711, so the visible blur falls off faster than the value). 1.0 "
    "= faithful, ~0.5 = softer halo, 0 = DOF off. Read live each flush.");

REXCVAR_DEFINE_INT32(
    bd_render_resolution, 0, kCvarGroup,
    "Internal render resolution. 0 = Display: BD renders its whole pipeline at "
    "the output (window / fullscreen) resolution so the present is 1:1. 1 = "
    "720p: BD's native 1280x720 composite, upscaled to the output. Requires "
    "restart (BD builds its composite chain once at init).")
    .range(0, 1)
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_INT32(
    bd_aspect_ratio, 0, kCvarGroup,
    "Output aspect ratio. 0 = 16:9 (native; letterbox/pillarbox bars on a "
    "non-16:9 display), 1 = 4:3, 2 = Stretch (fill the output, may distort). "
    "Applies live.")
    .range(0, 2);

REXCVAR_DEFINE_STRING(
    bd_language, "auto", kCvarGroup,
    "UI/voice language, from the game's Language Set (bd_boot.ini). 'auto' "
    "follows the Windows display language. Otherwise: us English, jp Japanese, "
    "de German, fr French, es Spanish, it Italian, kr Korean, tw Chinese "
    "(Traditional), cn Chinese (Simplified), po Portuguese. Which languages are "
    "shown depends on the installed assets (bd_boot.ini [Language]). Read once "
    "at guest boot, so a change takes effect after restart.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_INT32(
    bd_fps_limit, 60, kCvarGroup,
    "Frame rate cap; 0 = unlimited. Above 30 (or unlimited) the fixed 30Hz "
    "simulation is decoupled from rendering and the camera and animated objects "
    "are interpolated, so the game renders smoothly above 30fps at correct speed. "
    "30 or below = native coupled behavior.");

REXCVAR_DEFINE_BOOL(
    bd_vsync, true, kCvarGroup, "Vertical sync");

REXCVAR_DEFINE_INT32(
    bd_geometry_heap, 0, kCvarGroup,
    "Heap placement for guest geometry mirrors (VB/IB). 0 = auto (default): "
    "load-time-static geometry (physical arenas, quadlist IB) in device-local "
    "GPU_UPLOAD when supported, per-unlock dynamic buffers in UPLOAD sysmem - "
    "avoids both AMD floors (GPU re-reading static geometry over PCIe every "
    "pass, and the CPU byte-swapping into write-combined VRAM every unlock). "
    "1 = force UPLOAD for everything; 2 = force GPU_UPLOAD for everything "
    "(A/B diagnostics). Applies to buffers created after the change: restart "
    "to fully apply.")
    .range(0, 2);

REXCVAR_DEFINE_INT32(
    bd_diag_verbosity, 0, kCvarGroup,
    "Render diagnostic logging verbosity. 0 = silent (default); 1 = "
    "resolve/draw fallback and dropped-draw diagnostics (the rate-limited "
    "[resolve-diag]/[draw-diag] messages); 2 = also per-frame telemetry such as "
    "[occlusion] readback counts for tuning light coronas. Live-tunable.")
    .range(0, 2);

REXCVAR_DEFINE_INT32(
    bd_force_constant_upload, 0, kCvarGroup,
    "Diagnostic: force shader-constant re-upload every draw, bypassing the "
    "dirty-flag gate. Bit0 (1) = VS float constants, bit1 (2) = PS float "
    "constants, 3 = both; 0 = off (default). Isolates stale-CBV bugs where BD "
    "writes a constant inline (e.g. a bone/world matrix) without a hooked "
    "Set*ShaderConstantF, so reblue keeps a previous draw's constants. If a draw "
    "renders correctly only with this on, an inline constant write is being "
    "missed. Live-tunable.")
    .range(0, 3);

REXCVAR_DEFINE_INT32(
    bd_diag_stale_constants, 0, kCvarGroup,
    "Diagnostic (read-only): watch the WHOLE VS float-constant block (all 256 "
    "registers, device+0x700) for inline writes that bypass the dirty flag. On "
    "each draw that reuses the bound VS constant CBV, compare the guest's live "
    "block against what was last uploaded; a mismatch means BD wrote a constant "
    "without a hooked SetVertexShaderConstantF, so the draw renders with stale "
    "(previous draw's) constants - the random geometry-offset bug. Logs the "
    "first differing register (c20-c23 = the world matrix), its before/after "
    "values and the VS hash (rate-limited). ~4KB compare per reuse draw, "
    "cache-hot (~0.5-1% worst case). Does NOT alter rendering. 1 = on "
    "(default); 0 = off. Live-tunable.")
    .range(0, 1);
