#include "Slang.hpp"

#include "Core/App.hpp"
#include "Core/FileSystem.hpp"

namespace ox {
void Slang::add_shader(vuk::PipelineBaseCreateInfo& pipeline_ci, const CompileInfo& compile_info) {
  OX_SCOPED_ZONE;

  auto& ctx = App::get()->get_vkcontext();

  const auto root_directory = fs::get_directory(compile_info.path);
  const auto module_name = fs::get_file_name(compile_info.path);

  auto slang_session = ctx.shader_compiler.new_session({.definitions = pipeline_ci.defines, .root_directory = root_directory});

  auto slang_module = slang_session->load_module({
    .path = compile_info.path,
    .module_name = module_name,
  });

  for (auto& v : compile_info.entry_points) {
    auto entry_point = slang_module->get_entry_point(v);
    if (!entry_point.has_value()) {
      OX_LOG_ERROR("Shader stage '{}' is not found for shader module '{}'", v, module_name);
      return;
    }

    pipeline_ci.add_spirv(entry_point->ir, module_name, v);
  }
}
} // namespace ox
