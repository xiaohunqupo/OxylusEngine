#include "Camera.hpp"

#include "Core/App.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
Camera::Camera(glm::vec3 position) {
  _position = position;
  _yaw = glm::radians(-90.f);
  update();
}

void Camera::update() {
  jitter_prev = jitter;
  previous_matrices.projection_matrix = matrices.projection_matrix;
  previous_matrices.view_matrix = matrices.view_matrix;
  update_view_matrix();
  update_projection_matrix();
}

void Camera::update(const glm::vec3& pos, const glm::vec3& rotation) {
  OX_SCOPED_ZONE;
  set_position(pos);
  set_pitch(rotation.x);
  set_yaw(rotation.y);
  set_tilt(rotation.z);
  update();
}

void Camera::update_view_matrix() {
  OX_SCOPED_ZONE;
  const float cos_yaw = glm::cos(_yaw);
  const float sin_yaw = glm::sin(_yaw);
  const float cos_pitch = glm::cos(_pitch);
  const float sin_pitch = glm::sin(_pitch);

  _forward.x = cos_yaw * cos_pitch;
  _forward.y = sin_pitch;
  _forward.z = sin_yaw * cos_pitch;

  _forward = glm::normalize(_forward);
  _right = glm::normalize(glm::cross(_forward, {_tilt, 1, _tilt}));
  _up = glm::normalize(glm::cross(_right, _forward));

  matrices.view_matrix = glm::lookAt(_position, _position + _forward, _up);

  const auto extent = App::get()->get_swapchain_extent();
  if (extent.x != 0)
    _aspect = extent.x / extent.y;
  else
    _aspect = 1.0f;
}

void Camera::update_projection_matrix() {
  if (_projection == Projection::Perspective) {
    matrices.projection_matrix = glm::perspective(glm::radians(_fov), _aspect, far_clip, near_clip); // reversed-z
  } else {
    matrices.projection_matrix = glm::ortho(-_aspect * _zoom, _aspect * _zoom, -_zoom, _zoom, 100.0f,
                                            -100.0f);                                                // reversed-z
  }
  matrices.projection_matrix[1][1] *= -1.0f;
}

Frustum Camera::get_frustum() {
  const float half_v_side = get_far() * tanf(glm::radians(_fov) * .5f);
  const float half_h_side = half_v_side * get_aspect();
  const glm::vec3 forward_far = get_far() * _forward;

  Frustum frustum = {
    .top_face = {_position, cross(_right, forward_far - _up * half_v_side)},
    .bottom_face = {_position, cross(forward_far + _up * half_v_side, _right)},
    .right_face = {_position, cross(forward_far - _right * half_h_side, _up)},
    .left_face = {_position, cross(_up, forward_far + _right * half_h_side)},
    .far_face = {_position + forward_far, -_forward},
    .near_face = {_position + get_near() * _forward, _forward},
  };

  frustum.init();

  return frustum;
}

RayCast Camera::get_screen_ray(const glm::vec2& screen_pos) const {
  const glm::mat4 view_proj_inverse = inverse(get_projection_matrix() * get_view_matrix());

  const auto extent = App::get()->get_swapchain_extent();

  float screen_x = screen_pos.x / extent.x;
  float screen_y = screen_pos.y / extent.y;

  screen_x = 2.0f * screen_x - 1.0f;
  screen_y = 2.0f * screen_y - 1.0f;

  glm::vec4 n = view_proj_inverse * glm::vec4(screen_x, screen_y, 0.0f, 1.0f);
  n /= n.w;

  glm::vec4 f = view_proj_inverse * glm::vec4(screen_x, screen_y, 1.0f, 1.0f);
  f /= f.w;

  return {glm::vec3(n), glm::normalize(glm::vec3(f) - glm::vec3(n))};
}
} // namespace ox
