#include <rex/cvar.h>
#include <rex/types.h>
#include <rex/system/kernel_state.h>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <rex/ui/imgui_dialog.h>

REXCVAR_DEFINE_STRING(eot_data_root, "", "EdgeOfTime/config",
                      "Path to game asset directory. Overrides the default data root when non-empty.");
