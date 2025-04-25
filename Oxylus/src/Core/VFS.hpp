#pragma once

#include "ESystem.hpp"

namespace ox {
class VFS : public ESystem {
public:
  static constexpr auto APP_DIR = "app_dir";

  // Only used by OxylusEditor. Virtual directory registered for projects.
  static constexpr auto PROJECT_DIR = "project_dir";

  void init() override;
  void deinit() override;

  void mount_dir(const std::string& virtual_dir, const std::string& physical_dir);
  void unmount_dir(const std::string& virtual_dir);

  std::string resolve_physical_dir(const std::string& virtual_dir, const std::string& file_path);
  std::string get_virtual_dir(const std::string& file_path);

private:
  ankerl::unordered_dense::map<std::string, std::string> mapped_dirs = {};
};
} // namespace ox
