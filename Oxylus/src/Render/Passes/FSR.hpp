#pragma once
#include <vuk/Future.hpp>

#include "Assets/Texture.hpp"
#include "Core/Types.hpp"

namespace vuk {
struct ImageAttachment;
struct PipelineBaseCreateInfo;
class Context;
} // namespace vuk
namespace ox {
class Camera;
class FSR {
public:
  FSR() = default;
  ~FSR() = default;

  std::unique_ptr<char[]> scratch_memory = nullptr;
  Vec2 get_jitter() const;

  void create_fs2_resources(UVec2 render_resolution, UVec2 presentation_resolution);
  void load_pipelines(vuk::Allocator& allocator, vuk::PipelineBaseCreateInfo& pipeline_ci);
  vuk::Value<vuk::ImageAttachment> dispatch(vuk::Value<vuk::ImageAttachment>& input_color_post_alpha,
                                            vuk::Value<vuk::ImageAttachment>& input_color_pre_alpha,
                                            vuk::Value<vuk::ImageAttachment>& output,
                                            vuk::Value<vuk::ImageAttachment>& depth,
                                            vuk::Value<vuk::ImageAttachment>& velocity,
                                            Camera& camera,
                                            double dt,
                                            float sharpness,
                                            uint32_t frame_index);

private:
  struct Fsr2Constants {
    int32_t renderSize[2];
    int32_t displaySize[2];
    uint32_t lumaMipDimensions[2];
    uint32_t lumaMipLevelToUse;
    uint32_t frameIndex;
    float displaySizeRcp[2];
    float jitterOffset[2];
    float deviceToViewDepth[4];
    float depthClipUVScale[2];
    float postLockStatusUVScale[2];
    float reactiveMaskDimRcp[2];
    float motionVectorScale[2];
    float downscaleFactor[2];
    float preExposure;
    float tanHalfFOV;
    float motionVectorJitterCancellation[2];
    float jitterPhaseCount;
    float lockInitialLifetime;
    float lockTickDelta;
    float deltaTime;
    float dynamicResChangeFactor;
    float lumaMipRcp;
  } fsr2_constants;

  UVec2 _render_res;
  UVec2 _present_res;

  Texture adjusted_color;
  Texture luminance_current;
  Texture luminance_history;
  Texture exposure;
  Texture previous_depth;
  Texture dilated_depth;
  Texture dilated_motion;
  Texture dilated_reactive;
  Texture disocclusion_mask;
  Texture lock_status[2];
  Texture reactive_mask;
  Texture lanczos_lut;
  Texture maximum_bias_lut;
  Texture spd_global_atomic;
  Texture output_internal[2];
};
} // namespace ox
