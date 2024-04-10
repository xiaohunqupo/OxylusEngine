#include "SPD.hpp"

#include <vuk/Partials.hpp>
#include <vuk/RenderGraph.hpp>

#include "Core/FileSystem.hpp"
#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Utils/Log.hpp"

namespace ox {
static void SpdSetup(uint2& dispatchThreadGroupCountXY,  // CPU side: dispatch thread group count xy
                     uint2& workGroupOffset,             // GPU side: pass in as constant
                     uint2& numWorkGroupsAndMips,        // GPU side: pass in as constant
                     UVec4 rectInfo,                     // left, top, width, height
                     int32_t mips)                       // optional: if -1, calculate based on rect width and height
{
  workGroupOffset[0] = rectInfo[0] / 64;                 // rectInfo[0] = left
  workGroupOffset[1] = rectInfo[1] / 64;                 // rectInfo[1] = top

  uint endIndexX = (rectInfo[0] + rectInfo[2] - 1) / 64; // rectInfo[0] = left, rectInfo[2] = width
  uint endIndexY = (rectInfo[1] + rectInfo[3] - 1) / 64; // rectInfo[1] = top, rectInfo[3] = height

  dispatchThreadGroupCountXY[0] = endIndexX + 1 - workGroupOffset[0];
  dispatchThreadGroupCountXY[1] = endIndexY + 1 - workGroupOffset[1];

  numWorkGroupsAndMips[0] = (dispatchThreadGroupCountXY[0]) * (dispatchThreadGroupCountXY[1]);

  if (mips >= 0) {
    numWorkGroupsAndMips[1] = uint(mips);
  } else {
    // calculate based on rect width and height
    uint resolution = std::max(rectInfo[2], rectInfo[3]);
    numWorkGroupsAndMips[1] = uint((std::min(floor(log2(float(resolution))), float(12))));
  }
}

static void SpdSetup(uint2& dispatchThreadGroupCountXY, // CPU side: dispatch thread group count xy
                     uint2& workGroupOffset,            // GPU side: pass in as constant
                     uint2& numWorkGroupsAndMips,       // GPU side: pass in as constant
                     uint4 rectInfo)                    // left, top, width, height
{
  SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, -1);
}

static VkDescriptorSetLayoutBinding binding(uint binding, vuk::DescriptorType descriptor_type, uint count = 1024) {
  return {
    .binding = binding,
    .descriptorType = (VkDescriptorType)descriptor_type,
    .descriptorCount = count,
    .stageFlags = (VkShaderStageFlags)vuk::ShaderStageFlagBits::eAll,
  };
}

void SPD::init(vuk::Allocator& allocator, SPDLoad load) {
  _load = load;

  vuk::PipelineBaseCreateInfo pci = {};
  vuk::DescriptorSetLayoutCreateInfo layout_create_info = {};
  if (load == SPDLoad::Load) {
    layout_create_info.bindings = {
      binding(0, vuk::DescriptorType::eStorageImage, 13),
      binding(1, vuk::DescriptorType::eStorageImage, 1),
      binding(2, vuk::DescriptorType::eStorageBuffer, 1),
    };
    layout_create_info.index = 0;
    for (int i = 0; i < 3; i++)
      layout_create_info.flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    pci.explicit_set_layouts.emplace_back(layout_create_info);
  } else {
    layout_create_info.bindings = {
      binding(0, vuk::DescriptorType::eStorageImage, 12),
      binding(1, vuk::DescriptorType::eStorageImage, 1),
      binding(2, vuk::DescriptorType::eStorageBuffer, 1),
      binding(3, vuk::DescriptorType::eSampledImage, 1),
      binding(4, vuk::DescriptorType::eSampler, 1),
    };
    layout_create_info.index = 0;
    for (int i = 0; i < 4; i++)
      layout_create_info.flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    pci.explicit_set_layouts.emplace_back(layout_create_info);
  }

  pipeline_name = load == SPDLoad::LinearSampler ? "spd_pipeline_linear" : "spd_pipeline";
  const auto shader_name = load == SPDLoad::LinearSampler ? "FFX/SPD/SPDLinear.hlsl" : "FFX/SPD/SPD.hlsl";
  if (!allocator.get_context().is_pipeline_available(pipeline_name.c_str())) {
    pci.add_hlsl(FileSystem::read_shader_file(shader_name), FileSystem::get_shader_path(shader_name), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline(pipeline_name.c_str(), pci))
  }

  descriptor_set = allocator.get_context().create_persistent_descriptorset(allocator,
                                                                           *allocator.get_context().get_named_pipeline(pipeline_name.c_str()),
                                                                           0,
                                                                           64);
}

vuk::Value<vuk::ImageAttachment> SPD::dispatch(vuk::Allocator& allocator, vuk::Value<vuk::ImageAttachment> image) {
  OX_ASSERT(image->level_count <= 13);

  std::vector<uint> global_atomics(image->layer_count);
  std::fill(global_atomics.begin(), global_atomics.end(), 0u);

  auto buff = vuk::create_cpu_buffer(allocator, std::span(global_atomics));
  const auto& global_counter_buffer = *buff.first;
  descriptor_set->update_storage_buffer(2, 0, global_counter_buffer);

  uint32_t num_uavs = image->level_count;
  if (_load == SPDLoad::LinearSampler) {
    num_uavs = image->level_count - 1;
  }

  std::vector<vuk::ImageView> views = {};
  views.resize(num_uavs);
  std::vector<vuk::ImageViewCreateInfo> cis = {};
  cis.resize(num_uavs);
  for (uint mip = 0; mip < num_uavs; mip++) {
    cis[mip] = vuk::ImageViewCreateInfo{
      .image = image->image.image,
      .viewType = vuk::ImageViewType::e2DArray,
      .format = image->format,
      .subresourceRange =
        vuk::ImageSubresourceRange{
          .aspectMask = vuk::ImageAspectFlagBits::eColor,
          .baseMipLevel = mip,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = image->layer_count
        },
      .view_usage = vuk::ImageUsageFlagBits::eTransferDst | vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment |
                    vuk::ImageUsageFlagBits::eStorage,
    };
  }
  allocator.allocate_image_views(views, cis);

  if (_load == SPDLoad::Load) {
    for (uint i = 0; i < image->level_count; i++)
      descriptor_set->update_storage_image(0, i, views[i]);
    descriptor_set->update_storage_image(1, 0, views[6]);
  }

  if (_load == SPDLoad::LinearSampler) {
    for (uint i = 0; i < image->level_count - 1; i++)
      descriptor_set->update_storage_image(0, i + 1, views[i + 1]);
    descriptor_set->update_storage_image(1, 0, views[5]);
    descriptor_set->update_sampled_image(3, 0, views[0], vuk::ImageLayout::eReadOnlyOptimal);
    vuk::SamplerCreateInfo info = {};
    info.magFilter = vuk::Filter::eLinear;
    info.minFilter = vuk::Filter::eLinear;
    info.mipmapMode = vuk::SamplerMipmapMode::eNearest;
    info.addressModeU = vuk::SamplerAddressMode::eClampToEdge;
    info.addressModeV = vuk::SamplerAddressMode::eClampToEdge;
    info.addressModeW = vuk::SamplerAddressMode::eClampToEdge;
    info.minLod = -1000;
    info.maxLod = 1000;
    info.maxAnisotropy = 1.0f;

    const auto sampler = allocator.get_context().acquire_sampler(info, allocator.get_context().get_frame_count());

    descriptor_set->update_sampler(4, 0, sampler);
  }

  descriptor_set->commit(allocator.get_context());

  auto pass = vuk::make_pass("SPD", [this](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eComputeRW) input) {
    uint2 dispatchThreadGroupCountXY;
    uint2 workGroupOffset;                                                         // needed if Left and Top are not 0,0
    uint2 numWorkGroupsAndMips;
    const uint4 rectInfo = uint4(0, 0, input->extent.width, input->extent.height); // left, top, width, height
    SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);

    command_buffer.image_barrier(input,
                                 vuk::Access::eComputeRead | vuk::Access::eComputeWrite,
                                 vuk::Access::eComputeRead | vuk::Access::eComputeWrite,
                                 _load == SPDLoad::LinearSampler ? 1 : 0,
                                 _load == SPDLoad::LinearSampler ? input->level_count - 1 : input->level_count);

    if (_load == SPDLoad::LinearSampler) {
      // TODO: set layer_count to input->layer_count but there is no way to do so with vuk
      command_buffer.image_barrier(input,
                                   vuk::Access::eComputeRead | vuk::Access::eComputeWrite,
                                   vuk::Access::eComputeRead | vuk::Access::eComputeWrite,
                                   0,
                                   1);
    }

    command_buffer.bind_compute_pipeline(pipeline_name.c_str());

    command_buffer.bind_persistent(0, *descriptor_set);

    // Bind push constants
    struct SpdConstants {
      uint mips;
      uint numWorkGroupsPerSlice;
      uint workGroupOffset[2];
    };

    struct SpdLinearSamplerConstants {
      uint mips;
      uint numWorkGroupsPerSlice;
      uint workGroupOffset[2];
      float invInputSize[2];
      float padding[2];
    };

    if (_load == SPDLoad::LinearSampler) {
      SpdLinearSamplerConstants data;
      data.numWorkGroupsPerSlice = numWorkGroupsAndMips[0];
      data.mips = numWorkGroupsAndMips[1];
      data.workGroupOffset[0] = workGroupOffset[0];
      data.workGroupOffset[1] = workGroupOffset[1];
      data.invInputSize[0] = 1.0f / (float)input->extent.width;
      data.invInputSize[1] = 1.0f / (float)input->extent.height;
      command_buffer.push_constants(vuk::ShaderStageFlagBits::eCompute, 0, &data, sizeof(SpdLinearSamplerConstants));
    } else {
      SpdConstants data;
      data.numWorkGroupsPerSlice = numWorkGroupsAndMips[0];
      data.mips = numWorkGroupsAndMips[1];
      data.workGroupOffset[0] = workGroupOffset[0];
      data.workGroupOffset[1] = workGroupOffset[1];
      command_buffer.push_constants(vuk::ShaderStageFlagBits::eCompute, 0, &data, sizeof(SpdConstants));
    }

    // should be / 64
    uint dispatchX = dispatchThreadGroupCountXY[0];
    uint dispatchY = dispatchThreadGroupCountXY[1];
    uint dispatchZ = input->layer_count; // slices

    command_buffer.dispatch(dispatchX, dispatchY, dispatchZ);

    command_buffer.image_barrier(input,
                                 vuk::Access::eNone,
                                 vuk::Access::eComputeRead | vuk::Access::eComputeWrite,
                                 _load == SPDLoad::LinearSampler ? 1 : 0,
                                 _load == SPDLoad::LinearSampler ? input->level_count - 1 : input->level_count);

    return input;
  });

  return pass(image);
}

} // namespace ox
