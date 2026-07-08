#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace eot::installer {

struct SourceFileInfo {
  std::string path;
  uintmax_t size = 0;
};

class SourceVfs {
 public:
  virtual ~SourceVfs() = default;

  virtual bool Exists(const std::string& path) const = 0;
  virtual bool Load(const std::string& path, std::vector<uint8_t>* out) const = 0;
  virtual uintmax_t Size(const std::string& path) const = 0;
  virtual std::vector<SourceFileInfo> ListFiles() const = 0;
  virtual std::string Name() const = 0;
};

std::unique_ptr<SourceVfs> OpenSourceVfs(const std::filesystem::path& path);
bool SourceHasFile(const std::filesystem::path& path, const std::string& relative);
uintmax_t SourceFileSize(const std::filesystem::path& path, const std::string& relative);
uintmax_t SourceTotalSize(const std::filesystem::path& path);
bool ListSourceFiles(const std::filesystem::path& path,
                     std::vector<SourceFileInfo>* files,
                     std::string* error);
bool LoadSourceFile(const std::filesystem::path& path, const std::string& relative,
                    std::vector<uint8_t>* out, std::string* error);

}  // namespace eot::installer
