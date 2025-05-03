#include "AudioSource.hpp"

#include "Audio/AudioEngine.hpp"
#include "Core/App.hpp"

namespace ox {
AudioSource::~AudioSource() {
  ma_sound_uninit(&_sound);
}

bool AudioSource::load(const std::string& path) {
  auto* engine = App::get_system<AudioEngine>(EngineSystems::AudioEngine)->get_engine();
  const ma_result result = ma_sound_init_from_file(engine, path.c_str(), MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, &_sound);
  if (result != MA_SUCCESS) {
    OX_LOG_ERROR("Failed to load sound: {}", path);
    return false;
  }

  return true;
}

void AudioSource::play() {
  ma_sound_seek_to_pcm_frame(&_sound, 0);
  ma_sound_start(&_sound);
}

void AudioSource::pause() { ma_sound_stop(&_sound); }

void AudioSource::un_pause() { ma_sound_start(&_sound); }

void AudioSource::stop() {
  ma_sound_stop(&_sound);
  ma_sound_seek_to_pcm_frame(&_sound, 0);
}

bool AudioSource::is_playing() { return ma_sound_is_playing(&_sound); }

static ma_attenuation_model GetAttenuationModel(const AttenuationModelType model) {
  switch (model) {
    case AttenuationModelType::None       : return ma_attenuation_model_none;
    case AttenuationModelType::Inverse    : return ma_attenuation_model_inverse;
    case AttenuationModelType::Linear     : return ma_attenuation_model_linear;
    case AttenuationModelType::Exponential: return ma_attenuation_model_exponential;
  }

  return ma_attenuation_model_none;
}

void AudioSource::set_config(const AudioSourceConfig& config) {
  ma_sound* sound = &_sound;
  ma_sound_set_volume(sound, config.volume_multiplier);
  ma_sound_set_pitch(sound, config.pitch_multiplier);
  ma_sound_set_looping(sound, config.looping);

  if (_spatialization != config.spatialization) {
    _spatialization = config.spatialization;
    ma_sound_set_spatialization_enabled(sound, config.spatialization);
  }

  if (config.spatialization) {
    ma_sound_set_attenuation_model(sound, GetAttenuationModel(config.attenuation_model));
    ma_sound_set_rolloff(sound, config.roll_off);
    ma_sound_set_min_gain(sound, config.min_gain);
    ma_sound_set_max_gain(sound, config.max_gain);
    ma_sound_set_min_distance(sound, config.min_distance);
    ma_sound_set_max_distance(sound, config.max_distance);

    ma_sound_set_cone(sound, config.cone_inner_angle, config.cone_outer_angle, config.cone_outer_gain);
    ma_sound_set_doppler_factor(sound, glm::max(config.doppler_factor, 0.0f));
  } else {
    ma_sound_set_attenuation_model(sound, ma_attenuation_model_none);
  }
}

void AudioSource::set_volume(float volume) { ma_sound_set_volume(&_sound, volume); }

void AudioSource::set_pitch(float pitch) { ma_sound_set_pitch(&_sound, pitch); }

void AudioSource::set_looping(const bool state) { ma_sound_set_looping(&_sound, state); }

void AudioSource::set_spatialization(const bool state) {
  _spatialization = state;
  ma_sound_set_spatialization_enabled(&_sound, state);
}

void AudioSource::set_attenuation_model(const AttenuationModelType type) {
  if (_spatialization)
    ma_sound_set_attenuation_model(&_sound, GetAttenuationModel(type));
  else
    ma_sound_set_attenuation_model(&_sound, GetAttenuationModel(AttenuationModelType::None));
}

void AudioSource::set_roll_off(const float rollOff) { ma_sound_set_rolloff(&_sound, rollOff); }

void AudioSource::set_min_gain(const float minGain) { ma_sound_set_min_gain(&_sound, minGain); }

void AudioSource::set_max_gain(const float maxGain) { ma_sound_set_max_gain(&_sound, maxGain); }

void AudioSource::set_min_distance(const float minDistance) { ma_sound_set_min_distance(&_sound, minDistance); }

void AudioSource::set_max_distance(const float maxDistance) { ma_sound_set_max_distance(&_sound, maxDistance); }

void AudioSource::set_cone(const float innerAngle,
                           const float outerAngle,
                           const float outerGain) {
  ma_sound_set_cone(&_sound, innerAngle, outerAngle, outerGain);
}

void AudioSource::set_doppler_factor(const float factor) { ma_sound_set_doppler_factor(&_sound, glm::max(factor, 0.0f)); }

void AudioSource::set_position(const glm::vec3& position) { ma_sound_set_position(&_sound, position.x, position.y, position.z); }

void AudioSource::set_direction(const glm::vec3& forward) { ma_sound_set_direction(&_sound, forward.x, forward.y, forward.z); }

void AudioSource::set_velocity(const glm::vec3& velocity) { ma_sound_set_velocity(&_sound, velocity.x, velocity.y, velocity.z); }
} // namespace ox
