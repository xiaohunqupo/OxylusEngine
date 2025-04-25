#pragma once

#include <dylib.hpp>

#include "Core/ESystem.hpp"

namespace ox {
class ModuleInterface;

struct Module {
  Unique<dylib> lib;
  ModuleInterface* interface;
  std::string path;
};

class ModuleRegistry : public ESystem {
public:
  void init() override;
  void deinit() override;

  Module* add_lib(const std::string& name, std::string_view path);
  Module* get_lib(const std::string& name);
  void remove_lib(const std::string& name);
  void clear();

private:
  ankerl::unordered_dense::map<std::string, Unique<Module>> libs = {};
  std::vector<std::string> copied_file_paths = {};
};
}
