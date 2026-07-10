#include "goliath_engine/installer/install_manifest.h"

#include <fstream>
#include <string_view>

namespace eot::installer {
namespace {

bool LineIsTrue(std::string_view line, std::string_view key) {
  auto pos = line.find('=');
  if (pos == std::string_view::npos) {
    return false;
  }
  auto lhs = line.substr(0, pos);
  auto rhs = line.substr(pos + 1);
  auto trim = [](std::string_view s) {
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
      return std::string_view{};
    }
    const auto last = s.find_last_not_of(" \t");
    return s.substr(first, last - first + 1);
  };
  return trim(lhs) == key && trim(rhs) == "true";
}

std::string Quoted(std::string value) {
  std::string out = "\"";
  for (char c : value) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

}  // namespace

bool LoadInstallManifest(const std::filesystem::path& path,
                         InstallManifest* manifest) {
  if (!manifest) {
    return false;
  }
  *manifest = {};
  std::ifstream in(path);
  if (!in) {
    return false;
  }

  std::string line;
  while (std::getline(in, line)) {
    if (LineIsTrue(line, "installed")) {
      manifest->installed = true;
    }
  }
  return true;
}

bool SaveInstallManifest(const std::filesystem::path& path,
                         const InstallManifest& manifest,
                         std::string* error) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    if (error) {
      *error = "Failed to create manifest directory: " + ec.message();
    }
    return false;
  }

  const auto tmp = path.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary);
    if (!out) {
      if (error) {
        *error = "Failed to open install manifest for writing: " + tmp;
      }
      return false;
    }
    out << "# ReEoT install manifest\n";
    out << "installed = " << (manifest.installed ? "true" : "false") << "\n";
    out << "runtime_root = " << Quoted(manifest.runtime_root) << "\n";
    out << "note = " << Quoted(manifest.note) << "\n";
  }

  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    if (error) {
      *error = "Failed to commit install manifest: " + ec.message();
    }
    std::filesystem::remove(tmp);
    return false;
  }
  return true;
}

}  // namespace eot::installer
