#pragma once

namespace ox {
class UUID {
public:
  UUID();

  UUID(uint64_t uuid);

  operator uint64_t() const { return _uuid; }

private:
  uint64_t _uuid;
};
}

template <>
struct ankerl::unordered_dense::hash<ox::UUID> {
  size_t operator()(const ox::UUID& uuid) const noexcept {
    return uuid;
  }
};
