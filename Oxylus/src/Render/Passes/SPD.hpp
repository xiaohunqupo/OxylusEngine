#pragma once

#include <array>
#include "vuk/CommandBuffer.hpp"
#include "vuk/Future.hpp"

#include "Core/Types.hpp"

namespace ox {
class SPD {
public:
  enum class SPDLoad : uint {
    Load,
    LinearSampler,
  };

  SPD() = default;
  ~SPD() = default;

  void init(vuk::Allocator& allocator, SPDLoad load = SPDLoad::Load);

  vuk::Value<vuk::ImageAttachment> dispatch(vuk::Allocator& allocator, vuk::Value<vuk::ImageAttachment> image);

private:
  static constexpr auto SPD_MAX_MIP_LEVELS = 13;
  SPDLoad _load;
  vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set;
  std::string pipeline_name;
};
} // namespace ox
