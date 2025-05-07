#pragma once

namespace ox {

enum class MeshID : u64 { Invalid = std::numeric_limits<u64>::max() };

class Mesh {
public:
  Mesh() = default;
  ~Mesh() = default;

  u32 v = 0;
};

} // namespace ox
