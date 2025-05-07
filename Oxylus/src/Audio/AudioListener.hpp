#pragma once

namespace ox {
struct AudioListenerConfig {
  float cone_inner_angle = glm::radians(360.0f);
  float cone_outer_angle = glm::radians(360.0f);
  float cone_outer_gain = 0.0f;
};

class AudioListener {
public:
  AudioListener() = default;

  void set_config(const AudioListenerConfig& config) const;
  void set_position(const glm::vec3& position) const;
  void set_direction(const glm::vec3& forward) const;
  void set_velocity(const glm::vec3& velocity) const;

private:
  u32 m_ListenerIndex = 0;
};
} // namespace ox
