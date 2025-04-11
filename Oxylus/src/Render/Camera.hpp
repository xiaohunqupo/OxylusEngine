#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Frustum.hpp"

#include "Physics/RayCast.hpp"

namespace ox {
class Camera {
public:
  enum class Projection {
    Perspective = 0,
    Orthographic = 1,
  };

  Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f));

  void update();
  void update(const glm::vec3& pos, const glm::vec3& rotation);

  void set_projection(Projection projection) { _projection = projection; }
  Projection get_projection() const { return _projection; }

  glm::mat4 get_projection_matrix() const { return matrices.projection_matrix; }
  glm::mat4 get_inv_projection_matrix() const { return glm::inverse(matrices.projection_matrix); }
  glm::mat4 get_view_matrix() const { return matrices.view_matrix; }
  glm::mat4 get_inv_view_matrix() const { return glm::inverse(matrices.view_matrix); }
  glm::mat4 get_inverse_projection_view() const { return glm::inverse(matrices.projection_matrix * matrices.view_matrix); }

  glm::mat4 get_previous_projection_matrix() const { return previous_matrices.projection_matrix; }
  glm::mat4 get_previous_inv_projection_matrix() const { return glm::inverse(previous_matrices.projection_matrix); }
  glm::mat4 get_previous_view_matrix() const { return previous_matrices.view_matrix; }
  glm::mat4 get_previous_inv_view_matrix() const { return glm::inverse(previous_matrices.view_matrix); }
  glm::mat4 get_previous_inverse_projection_view() const { return glm::inverse(previous_matrices.projection_matrix * previous_matrices.view_matrix); }

  const glm::vec3& get_position() const { return _position; }
  void set_position(const glm::vec3 pos) { _position = pos; }

  float get_yaw() const { return _yaw; }
  void set_yaw(const float value) { _yaw = value; }

  float get_pitch() const { return _pitch; }
  void set_pitch(const float value) { _pitch = value; }

  float get_tilt() const { return _tilt; }
  void set_tilt(const float value) { _tilt = value; }

  float get_near() const { return near_clip; }
  void set_near(float new_near) { near_clip = new_near; }

  float get_far() const { return far_clip; }
  void set_far(float new_far) { far_clip = new_far; }

  float get_fov() const { return _fov; }
  void set_fov(float fov) { _fov = fov; }

  float get_aspect() const { return _aspect; }

  glm::vec3 get_forward() const { return _forward; }
  glm::vec3 get_right() const { return _right; }

  glm::vec2 get_jitter() const { return jitter; }
  glm::vec2 get_previous_jitter() const { return jitter_prev; }
  void set_jitter(const glm::vec2 value) { jitter = value; }

  void set_zoom(const float zoom) { _zoom = zoom; }
  float get_zoom() const { return _zoom; }

  void update_view_matrix();
  void update_projection_matrix();
  Frustum get_frustum();
  RayCast get_screen_ray(const glm::vec2& screen_pos) const;

private:
  Projection _projection = Projection::Perspective;
  float _fov = 60.0f;
  float _aspect = 1.0f;
  float far_clip = 1000.0f;
  float near_clip = 0.01f;
  glm::vec2 jitter = {};
  glm::vec2 jitter_prev = {};

  uint32_t aspect_ratio_w = 1;
  uint32_t aspect_ratio_h = 1;

  struct Matrices {
    glm::mat4 view_matrix = {};
    glm::mat4 projection_matrix = {};
  };

  Matrices matrices = {};
  Matrices previous_matrices = {};

  glm::vec3 _position = {};
  glm::vec3 _forward = {};
  glm::vec3 _right = {};
  glm::vec3 _up = {};
  float _yaw = -1.5708f; // -90
  float _pitch = 0;
  float _tilt = 0;
  float _zoom = 1;
};
} // namespace ox
