#include "VFS.hpp"

#include <Utils/Log.hpp>
#include <Utils/Profiler.hpp>

#include "Core/FileSystem.hpp"

namespace ox {
std::pair<std::string, std::string> split_path(std::string_view full_path) {
  const size_t found = full_path.find_first_of("/\\");
  return {
    std::string(full_path.substr(0, found + 1)), // virtual
    std::string(full_path.substr(found + 1)),    // rest
  };
}

void VFS::init() {}

void VFS::deinit() {}

void VFS::mount_dir(const std::string& virtual_dir, const std::string& physical_dir) {
  OX_SCOPED_ZONE;
  mapped_dirs.emplace(virtual_dir, physical_dir);
}

void VFS::unmount_dir(const std::string& virtual_dir) {
  OX_SCOPED_ZONE;
  mapped_dirs.erase(virtual_dir);
}

std::string VFS::resolve_physical_dir(const std::string& virtual_dir, const std::string& file_path) {
  OX_SCOPED_ZONE;
  if (!mapped_dirs.contains(virtual_dir)) {
    OX_LOG_ERROR("Not a mounted virtual dir: {}", virtual_dir);
    return {};
  }

  const auto physical_dir = mapped_dirs[virtual_dir];

  return fs::append_paths(physical_dir, file_path);
}

std::string VFS::get_virtual_dir(const std::string& file_path) {
  OX_SCOPED_ZONE;
  return {};
}
} // namespace ox
