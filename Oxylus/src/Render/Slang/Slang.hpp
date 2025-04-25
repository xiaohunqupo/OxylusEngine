#pragma once
#include <string>
#include <vector>
#include <vuk/runtime/vk/Pipeline.hpp>

namespace ox {
class Slang {
public:
  struct CompileInfo {
    std::string path = {};
    std::vector<std::string> entry_points = {};
    std::vector<std::pair<std::string, std::string>> definitions = {};
  };

  static void add_shader(vuk::PipelineBaseCreateInfo& pipeline_ci, const CompileInfo& compile_info);
};
} // namespace ox
