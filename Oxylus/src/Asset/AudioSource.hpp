#pragma once
#include <miniaudio.h>

namespace ox {
enum class AttenuationModelType { None = 0, Inverse, Linear, Exponential };

struct AudioSourceConfig {
  float volume_multiplier = 1.0f;
  float pitch_multiplier = 1.0f;
  bool play_on_awake = true;
  bool looping = false;

  bool spatialization = false;
  AttenuationModelType attenuation_model = AttenuationModelType::Inverse;
  float roll_off = 1.0f;
  float min_gain = 0.0f;
  float max_gain = 1.0f;
  float min_distance = 0.3f;
  float max_distance = 1000.0f;

  float cone_inner_angle = glm::radians(360.0f);
  float cone_outer_angle = glm::radians(360.0f);
  float cone_outer_gain = 0.0f;

  float doppler_factor = 1.0f;
};

enum class AudioID : uint64 { Invalid = std::numeric_limits<uint64>::max() };
class AudioSource {
public:
  AudioSource() = default;
  ~AudioSource();

  bool load(const std::string& path);

  void play();
  void pause();
  void un_pause();
  void stop();
  bool is_playing();
  void set_config(const AudioSourceConfig& config);
  void set_volume(float volume);
  void set_pitch(float pitch);
  void set_looping(bool state);
  void set_spatialization(bool state);
  void set_attenuation_model(AttenuationModelType type);
  void set_roll_off(float rollOff);
  void set_min_gain(float minGain);
  void set_max_gain(float maxGain);
  void set_min_distance(float minDistance);
  void set_max_distance(float maxDistance);
  void set_cone(float innerAngle,
                float outerAngle,
                float outerGain);
  void set_doppler_factor(float factor);
  void set_position(const glm::vec3& position);
  void set_direction(const glm::vec3& forward);
  void set_velocity(const glm::vec3& velocity);

private:
  ma_sound _sound;
  bool _spatialization = false;
};
} // namespace ox
