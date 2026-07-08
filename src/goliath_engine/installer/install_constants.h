#pragma once

#include <array>
#include <cstdint>

namespace eot::installer {

inline constexpr char kInstallerUserDirectory[] = "EdgeOfTimeRecomp";
inline constexpr char kLegacyInstallerUserDirectory[] = "EdgeOfTimeRecompiled";

inline constexpr char kEntrypointXex[] = "Default.xex";
inline constexpr char kGameLogicDll[] = "Data/GameLogic.dll";
inline constexpr char kEntrypointPatch[] = "Default.xexp";
inline constexpr char kGameLogicPatch[] = "Data/GameLogic.dllp";
inline constexpr char kPatchedEntrypointXex[] = "Default_Patched.xex";
inline constexpr char kPatchedGameLogicDll[] = "Data/GameLogic_Patched.dll";

inline constexpr char kMountedPatchedEntrypointXex[] =
    "game:\\patched\\Default_Patched.xex";
inline constexpr char kMountedLocalPatchedEntrypointXex[] =
    "game:\\Default_Patched.xex";
inline constexpr char kMountedEntrypointXex[] = "game:\\Default.xex";

inline constexpr char kDlcPackageFileName[] =
    "564E216B8B02133E5F0FFB3D2E3CD412CBA2D0FF41";
inline constexpr char kDlcDisplayName[] = "IDENTITY CRISIS SUITS";
inline constexpr char kTitleId[] = "415608B2";
inline constexpr char kMarketplaceContentType[] = "00000002";
inline constexpr char kCommonContentXuid[] = "0000000000000000";

inline constexpr char kSeuratFontFile[] = "FOT-SeuratPro-M.otf";
inline constexpr char kFallbackFontFile[] = "Roboto-Medium.ttf";

inline constexpr std::array<uint64_t, 1> kKnownEntrypointXexHashes = {
    0xb8e9fb69b9d04b8bULL,
};
inline constexpr std::array<uint64_t, 1> kKnownGameLogicDllHashes = {
    0xb60c9189331fa25bULL,
};
inline constexpr std::array<uint64_t, 1> kKnownEntrypointPatchHashes = {
    0x03600b84a464aeb0ULL,
};
inline constexpr std::array<uint64_t, 1> kKnownGameLogicPatchHashes = {
    0xbc098335b78a4958ULL,
};
inline constexpr std::array<uint64_t, 1> kKnownDlcPackageHashes = {
    0x8813256b16fcc0a7ULL,
};

}  // namespace eot::installer
