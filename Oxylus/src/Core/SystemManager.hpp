#pragma once

#include "Core/ESystem.hpp"
#include "System.hpp"

namespace ox {
class SystemManager : public ESystem {
public:
  SystemManager() = default;

  auto init() -> std::expected<void, std::string> override { return {}; }
  auto deinit() -> std::expected<void, std::string> override { return {}; }

  template <typename T, typename... Args>
  Shared<System> register_system(Args&&... args) {
    auto hash_code = typeid(T).hash_code();
    if (system_registry.contains(hash_code))
      system_registry.erase(hash_code);

    Shared<T> system = create_shared<T>(std::forward<Args>(args)...);
    system->hash_code = hash_code;
    system_registry.emplace(hash_code, std::make_pair(typeid(T).name(), std::move(system)));
    return system_registry[hash_code].second;
  }

  template <typename T>
  void unregister_system() {
    const auto hash_code = typeid(T).hash_code();

    if (system_registry.contains(hash_code)) {
      system_registry.erase(hash_code);
    }
  }

  Shared<System> get_system(size_t hash) {
    if (system_registry.contains(hash)) {
      return system_registry[hash].second;
    }

    return nullptr;
  }

  template <typename T>
  bool has_system() {
    const auto hash_code = typeid(T).hash_code();
    return system_registry.contains(hash_code);
  }

  ankerl::unordered_dense::map<size_t, std::pair<const char*, Shared<System>>> system_registry = {};
};
} // namespace ox
