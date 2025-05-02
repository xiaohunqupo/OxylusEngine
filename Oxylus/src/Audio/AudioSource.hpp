#pragma once

struct ma_sound;

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
  AudioSource(const AudioSource& other) = delete;
  AudioSource(AudioSource&& other) = delete;

  bool load(const std::string& path);

  void play() const;
  void pause() const;
  void un_pause() const;
  void stop() const;
  bool is_playing() const;
  void set_config(const AudioSourceConfig& config);
  void set_volume(float volume) const;
  void set_pitch(float pitch) const;
  void set_looping(bool state) const;
  void set_spatialization(bool state);
  void set_attenuation_model(AttenuationModelType type) const;
  void set_roll_off(float rollOff) const;
  void set_min_gain(float minGain) const;
  void set_max_gain(float maxGain) const;
  void set_min_distance(float minDistance) const;
  void set_max_distance(float maxDistance) const;
  void set_cone(float innerAngle,
                float outerAngle,
                float outerGain) const;
  void set_doppler_factor(float factor) const;
  void set_position(const glm::vec3& position) const;
  void set_direction(const glm::vec3& forward) const;
  void set_velocity(const glm::vec3& velocity) const;

private:
  Unique<ma_sound> _sound;
  bool _spatialization = false;
};
} // namespace ox
