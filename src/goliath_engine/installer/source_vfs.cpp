#include "goliath_engine/installer/source_vfs.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <stack>
#include <system_error>

#include <rex/types.h>

#include "goliath_engine/common/logging.h"

namespace eot::installer {
namespace {

std::string Lower(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

uint16_t ReadLE16(const uint8_t* p) {
  rex::le<rex::u16> value;
  std::memcpy(&value, p, sizeof(value));
  return value;
}

uint32_t ReadLE24(const uint8_t* p) {
  return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16));
}

uint32_t ReadLE32(const uint8_t* p) {
  rex::le<rex::u32> value;
  std::memcpy(&value, p, sizeof(value));
  return value;
}

uint16_t ReadBE16(const uint8_t* p) {
  rex::be_u16 value;
  std::memcpy(&value, p, sizeof(value));
  return value;
}

uint32_t ReadBE32(const uint8_t* p) {
  rex::be_u32 value;
  std::memcpy(&value, p, sizeof(value));
  return value;
}

uint64_t ReadBE64(const uint8_t* p) {
  rex::be_u64 value;
  std::memcpy(&value, p, sizeof(value));
  return value;
}

class FileReader {
 public:
  explicit FileReader(std::filesystem::path path) : path_(std::move(path)) {
    stream_.open(path_, std::ios::binary);
    if (!stream_) {
      return;
    }
    stream_.seekg(0, std::ios::end);
    size_ = static_cast<uint64_t>(stream_.tellg());
  }

  bool IsOpen() const { return stream_.is_open(); }
  uint64_t Size() const { return size_; }
  const std::filesystem::path& path() const { return path_; }

  bool Read(uint64_t offset, void* out, size_t size) const {
    if (!stream_ || offset + size > size_) {
      return false;
    }
    stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    stream_.read(static_cast<char*>(out), static_cast<std::streamsize>(size));
    return !stream_.bad() && static_cast<size_t>(stream_.gcount()) == size;
  }

  bool ReadVector(uint64_t offset, size_t size, std::vector<uint8_t>* out) const {
    out->resize(size);
    return Read(offset, out->data(), size);
  }

 private:
  std::filesystem::path path_;
  mutable std::ifstream stream_;
  uint64_t size_ = 0;
};

class DirectorySourceVfs final : public SourceVfs {
 public:
  explicit DirectorySourceVfs(std::filesystem::path root) : root_(std::move(root)) {
    name_ = root_.filename().string();
  }

  bool Exists(const std::string& path) const override {
    std::error_code ec;
    return std::filesystem::is_regular_file(Resolve(path), ec);
  }

  bool Load(const std::string& path, std::vector<uint8_t>* out) const override {
    const auto file_path = Resolve(path);
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
      return false;
    }
    out->assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
  }

  uintmax_t Size(const std::string& path) const override {
    std::error_code ec;
    const auto size = std::filesystem::file_size(Resolve(path), ec);
    return ec ? 0 : size;
  }

  std::vector<SourceFileInfo> ListFiles() const override {
    std::vector<SourceFileInfo> files;
    std::error_code ec;
    if (!std::filesystem::is_directory(root_, ec)) {
      return files;
    }
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root_, ec)) {
      if (ec) {
        break;
      }
      if (!entry.is_regular_file(ec)) {
        continue;
      }
      auto relative = std::filesystem::relative(entry.path(), root_, ec);
      if (ec) {
        continue;
      }
      const auto size = entry.file_size(ec);
      if (ec) {
        continue;
      }
      files.push_back({relative.generic_string(), size});
    }
    return files;
  }

  std::string Name() const override { return name_; }

 private:
  std::filesystem::path Resolve(const std::string& path) const {
    std::filesystem::path rel;
    for (const auto& part : std::filesystem::path(path)) {
      if (part == "/" || part == "." || part == "..") {
        continue;
      }
      rel /= part;
    }
    auto direct = root_ / rel;
    if (std::filesystem::exists(direct)) {
      return direct;
    }

    std::filesystem::path cur = root_;
    for (const auto& part : rel) {
      const auto wanted = Lower(part.string());
      bool found = false;
      std::error_code ec;
      for (const auto& entry : std::filesystem::directory_iterator(cur, ec)) {
        if (Lower(entry.path().filename().string()) == wanted) {
          cur = entry.path();
          found = true;
          break;
        }
      }
      if (!found) {
        return direct;
      }
    }
    return cur;
  }

  std::filesystem::path root_;
  std::string name_;
};

class MapSourceVfs : public SourceVfs {
 protected:
  struct Entry {
    uint64_t offset = 0;
    uint64_t size = 0;
    uint32_t block = 0;
    uint32_t block_count = 0;
  };

  const Entry* Find(const std::string& path) const {
    auto it = files_.find(Lower(path));
    return it == files_.end() ? nullptr : &it->second;
  }

  void AddFile(std::string path, Entry entry) {
    listed_files_.push_back({path, entry.size});
    files_[Lower(path)] = entry;
  }

  std::vector<SourceFileInfo> ListMapFiles() const { return listed_files_; }

  std::map<std::string, Entry> files_;
  std::vector<SourceFileInfo> listed_files_;
};

class IsoSourceVfs final : public MapSourceVfs {
 public:
  explicit IsoSourceVfs(std::filesystem::path path) : file_(std::move(path)) {
    name_ = file_.path().filename().string();
    if (!file_.IsOpen()) {
      return;
    }
    Parse();
  }

  bool ok() const { return ok_; }
  bool Exists(const std::string& path) const override { return Find(path) != nullptr; }

  bool Load(const std::string& path, std::vector<uint8_t>* out) const override {
    const Entry* entry = Find(path);
    return entry && file_.ReadVector(entry->offset, static_cast<size_t>(entry->size), out);
  }

  uintmax_t Size(const std::string& path) const override {
    const Entry* entry = Find(path);
    return entry ? entry->size : 0;
  }

  std::vector<SourceFileInfo> ListFiles() const override { return ListMapFiles(); }

  std::string Name() const override { return name_; }

 private:
  void Parse() {
    static constexpr uint64_t kSectorSize = 2048;
    static constexpr std::array<uint64_t, 5> kOffsets = {
        0x00000000ULL, 0x0000FB20ULL, 0x00020600ULL, 0x02080000ULL, 0x0FD90000ULL};
    static constexpr const char* kMagic = "MICROSOFT*XBOX*MEDIA";

    uint64_t game_offset = 0;
    bool magic_found = false;
    std::array<char, 20> magic{};
    for (uint64_t offset : kOffsets) {
      const uint64_t file_offset = offset + 32 * kSectorSize;
      if (file_.Read(file_offset, magic.data(), std::strlen(kMagic)) &&
          std::memcmp(magic.data(), kMagic, std::strlen(kMagic)) == 0) {
        game_offset = offset;
        magic_found = true;
      }
    }
    if (!magic_found) {
      return;
    }

    std::array<uint8_t, 8> root_info{};
    if (!file_.Read(game_offset + 32 * kSectorSize + 20, root_info.data(), root_info.size())) {
      return;
    }
    const uint32_t root_sector = ReadLE32(root_info.data());
    const uint32_t root_size = ReadLE32(root_info.data() + 4);
    if (root_size < 13 || root_size > 32 * 1024 * 1024) {
      return;
    }

    struct Step {
      std::string base;
      uint64_t node_offset = 0;
      uint32_t entry_offset = 0;
    };
    std::stack<Step> stack;
    stack.push({"", game_offset + root_sector * kSectorSize, 0});

    while (!stack.empty()) {
      Step step = stack.top();
      stack.pop();

      std::array<uint8_t, 270> buf{};
      const uint64_t info_offset = step.node_offset + step.entry_offset;
      if (!file_.Read(info_offset, buf.data(), 14)) {
        return;
      }
      const uint16_t node_l = ReadLE16(buf.data());
      const uint16_t node_r = ReadLE16(buf.data() + 2);
      const uint32_t sector = ReadLE32(buf.data() + 4);
      const uint32_t length = ReadLE32(buf.data() + 8);
      const uint8_t attributes = buf[12];
      const uint8_t name_length = buf[13];
      if (name_length >= 255 || !file_.Read(info_offset + 14, buf.data(), name_length)) {
        return;
      }
      std::string file_name(reinterpret_cast<char*>(buf.data()), name_length);
      if (node_l) {
        stack.push({step.base, step.node_offset, static_cast<uint32_t>(node_l * 4)});
      }
      if (node_r) {
        stack.push({step.base, step.node_offset, static_cast<uint32_t>(node_r * 4)});
      }
      const std::string full_name = step.base + file_name;
      if (attributes & 0x10) {
        if (length > 0) {
          stack.push({full_name + "/", game_offset + sector * kSectorSize, 0});
        }
      } else {
        AddFile(full_name, {game_offset + sector * kSectorSize, length, 0, 0});
      }
    }
    ok_ = !files_.empty();
    EOT_INFO("[installer-source] opened ISO {} files={}", name_, files_.size());
  }

  FileReader file_;
  std::string name_;
  bool ok_ = false;
};

class StfsSourceVfs final : public MapSourceVfs {
 public:
  explicit StfsSourceVfs(std::filesystem::path path) : file_(std::move(path)) {
    name_ = file_.path().filename().string();
    if (file_.IsOpen()) {
      Parse();
    }
  }

  bool ok() const { return ok_; }
  bool Exists(const std::string& path) const override { return Find(path) != nullptr; }

  bool Load(const std::string& path, std::vector<uint8_t>* out) const override {
    const Entry* entry = Find(path);
    if (!entry) {
      return false;
    }
    out->resize(static_cast<size_t>(entry->size));
    uint64_t dst = 0;
    uint64_t remaining = entry->size;
    uint32_t block = entry->block;
    for (uint32_t i = 0; i < entry->block_count && block != kEndOfChain && remaining > 0; ++i) {
      const uint64_t chunk = std::min<uint64_t>(kBlockSize, remaining);
      if (!file_.Read(BlockOffset(block), out->data() + dst, static_cast<size_t>(chunk))) {
        return false;
      }
      block = NextBlock(block);
      dst += chunk;
      remaining -= chunk;
    }
    return remaining == 0;
  }

  uintmax_t Size(const std::string& path) const override {
    const Entry* entry = Find(path);
    return entry ? entry->size : 0;
  }

  std::vector<SourceFileInfo> ListFiles() const override { return ListMapFiles(); }

  std::string Name() const override { return name_; }

  static bool Check(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      return false;
    }
    std::array<uint8_t, 4> magic{};
    file.read(reinterpret_cast<char*>(magic.data()), magic.size());
    const uint32_t value = ReadBE32(magic.data());
    return value == 0x434F4E20 || value == 0x50495253 || value == 0x4C495645;
  }

 private:
  static constexpr uint32_t kBlockSize = 0x1000;
  static constexpr uint32_t kEndOfChain = 0xFFFFFF;
  static constexpr uint32_t kBlocksPerHashLevel0 = 170;
  static constexpr uint32_t kBlocksPerHashLevel1 = 28900;
  static constexpr uint32_t kBlocksPerHashLevel2 = 4913000;

  uint64_t BlockOffset(uint64_t block_index) const {
    uint64_t block = block_index;
    block += ((block_index + kBlocksPerHashLevel0) / kBlocksPerHashLevel0);
    if (block_index >= kBlocksPerHashLevel0) {
      block += ((block_index + kBlocksPerHashLevel1) / kBlocksPerHashLevel1);
      if (block_index >= kBlocksPerHashLevel1) {
        block += ((block_index + kBlocksPerHashLevel2) / kBlocksPerHashLevel2);
      }
    }
    return base_offset_ + (block << 12);
  }

  uint32_t HashBlockNumber(uint32_t block_index) const {
    if (block_index < kBlocksPerHashLevel0) {
      return 0;
    }
    uint32_t block =
        (block_index / kBlocksPerHashLevel0) * (kBlocksPerHashLevel0 + 1);
    block += ((block_index / kBlocksPerHashLevel1) + 1);
    return block_index < kBlocksPerHashLevel1 ? block : block + 1;
  }

  uint32_t NextBlock(uint32_t block_index) const {
    const uint64_t hash_offset = base_offset_ + (static_cast<uint64_t>(HashBlockNumber(block_index)) << 12);
    std::array<uint8_t, 0x18> entry{};
    if (!file_.Read(hash_offset + (block_index % kBlocksPerHashLevel0) * 0x18,
                    entry.data(), entry.size())) {
      return kEndOfChain;
    }
    return ReadBE32(entry.data() + 0x14) & 0xFFFFFF;
  }

  void Parse() {
    std::array<uint8_t, 0x3B9> header{};
    if (!file_.Read(0, header.data(), header.size())) {
      return;
    }
    const uint32_t magic = ReadBE32(header.data());
    if (magic != 0x434F4E20 && magic != 0x50495253 && magic != 0x4C495645) {
      return;
    }
    const uint32_t header_size = ReadBE32(header.data() + 0x340);
    const uint32_t volume_type = ReadBE32(header.data() + 0x3A9);
    if (volume_type != 0) {
      EOT_WARN("[installer-source] SVOD packages are not supported yet: {}", name_);
      return;
    }

    const uint64_t descriptor_offset = 0x379;
    if (header[descriptor_offset] != 0x24 || (header[descriptor_offset + 2] & 1) == 0) {
      return;
    }
    const uint16_t file_table_block_count = ReadLE16(header.data() + descriptor_offset + 3);
    uint32_t table_block = ReadLE24(header.data() + descriptor_offset + 5);
    base_offset_ = ((header_size + kBlockSize - 1) / kBlockSize) * kBlockSize;

    uint32_t entry_count = 0;
    std::map<uint32_t, std::string> directory_names;
    for (uint32_t table_i = 0; table_i < file_table_block_count; ++table_i) {
      const uint64_t block_offset = BlockOffset(table_block);
      for (uint32_t entry_i = 0; entry_i < 0x40; ++entry_i) {
        std::array<uint8_t, 0x40> entry{};
        if (!file_.Read(block_offset + entry_i * 0x40, entry.data(), entry.size())) {
          return;
        }
        if (entry[0] == 0) {
          break;
        }
        const uint8_t name_length = entry[0x28] & 0x3F;
        const bool is_directory = (entry[0x28] & 0x80) != 0;
        std::string file_name(reinterpret_cast<char*>(entry.data()), name_length);
        const uint16_t dir_index = ReadBE16(entry.data() + 0x32);
        const std::string base = directory_names[dir_index];
        if (is_directory) {
          directory_names[entry_count++] = base + file_name + "/";
          continue;
        }
        const uint32_t allocated_blocks = ReadLE24(entry.data() + 0x2C);
        const uint32_t start_block = ReadLE24(entry.data() + 0x2F);
        const uint32_t length = ReadBE32(entry.data() + 0x34);
        AddFile(base + file_name, {0, length, start_block, allocated_blocks});
        entry_count++;
      }
      table_block = NextBlock(table_block);
      if (table_block == kEndOfChain) {
        break;
      }
    }
    ok_ = !files_.empty();
    EOT_INFO("[installer-source] opened STFS {} files={}", name_, files_.size());
  }

  FileReader file_;
  std::string name_;
  uint64_t base_offset_ = 0;
  bool ok_ = false;
};

}  // namespace

std::unique_ptr<SourceVfs> OpenSourceVfs(const std::filesystem::path& path) {
  std::error_code ec;
  if (std::filesystem::is_directory(path, ec)) {
    return std::make_unique<DirectorySourceVfs>(path);
  }
  if (StfsSourceVfs::Check(path)) {
    auto stfs = std::make_unique<StfsSourceVfs>(path);
    if (stfs->ok()) {
      return stfs;
    }
  }
  if (Lower(path.extension().string()) == ".iso") {
    auto iso = std::make_unique<IsoSourceVfs>(path);
    if (iso->ok()) {
      return iso;
    }
  }
  return {};
}

bool SourceHasFile(const std::filesystem::path& path, const std::string& relative) {
  auto vfs = OpenSourceVfs(path);
  return vfs && vfs->Exists(relative);
}

uintmax_t SourceFileSize(const std::filesystem::path& path, const std::string& relative) {
  auto vfs = OpenSourceVfs(path);
  return vfs ? vfs->Size(relative) : 0;
}

uintmax_t SourceTotalSize(const std::filesystem::path& path) {
  auto vfs = OpenSourceVfs(path);
  if (!vfs) {
    return 0;
  }
  uintmax_t total = 0;
  for (const auto& file : vfs->ListFiles()) {
    total += file.size;
  }
  return total;
}

bool ListSourceFiles(const std::filesystem::path& path,
                     std::vector<SourceFileInfo>* files,
                     std::string* error) {
  auto vfs = OpenSourceVfs(path);
  if (!vfs) {
    if (error) {
      *error = "Unable to open source: " + path.string();
    }
    return false;
  }
  *files = vfs->ListFiles();
  return true;
}

bool LoadSourceFile(const std::filesystem::path& path, const std::string& relative,
                    std::vector<uint8_t>* out, std::string* error) {
  auto vfs = OpenSourceVfs(path);
  if (!vfs) {
    if (error) {
      *error = "Unable to open source: " + path.string();
    }
    return false;
  }
  if (!vfs->Load(relative, out)) {
    if (error) {
      *error = "Unable to read " + relative + " from " + path.string();
    }
    return false;
  }
  return true;
}

}  // namespace eot::installer
