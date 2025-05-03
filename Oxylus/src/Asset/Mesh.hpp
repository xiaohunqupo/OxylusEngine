#pragma once

namespace ox {

enum class MeshID : uint64 { Invalid = std::numeric_limits<uint64>::max() };

class Mesh {
public:
  Mesh() = default;
  ~Mesh() = default;

  uint32 v = 0;
};

} // namespace ox
