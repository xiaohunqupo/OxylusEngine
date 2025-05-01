#pragma once

#include "Asset/SpriteMaterial.hpp"
#include "Asset/TilemapSerializer.hpp"
#include "Audio/AudioListener.hpp"
#include "Audio/AudioSource.hpp"
#include "Core/App.hpp"
#include "Core/SystemManager.hpp"
#include "Render/ParticleSystem.hpp"
#include "Render/Utils/RectPacker.hpp"
#include "Scripting/LuaSystem.hpp"

namespace JPH {
class Character;
}

namespace ox {
class Texture;

struct LayerComponent {
  uint16 layer = 1;
};

struct TransformComponent {
  static constexpr auto in_place_delete = true;

  glm::vec3 position = glm::vec3(0);
  glm::vec3 rotation = glm::vec3(0); // Stored in radians
  glm::vec3 scale = glm::vec3(1);

  TransformComponent() = default;
  TransformComponent(const glm::vec3& translation) : position(translation) {}

  TransformComponent(const glm::mat4& transform_matrix) {
    OX_SCOPED_ZONE;
    math::decompose_transform(transform_matrix, position, rotation, scale);
  }

  void set_from_matrix(const glm::mat4& transform_matrix) {
    OX_SCOPED_ZONE;
    math::decompose_transform(transform_matrix, position, rotation, scale);
  }

  glm::mat4 get_local_transform() const {
    return glm::translate(glm::mat4(1.0f), position) * glm::toMat4(glm::quat(rotation)) * glm::scale(glm::mat4(1.0f), scale);
  }
};

// Rendering
struct MeshComponent {
  static constexpr auto in_place_delete = true; // pointer stability

  Shared<Mesh> mesh_base = nullptr;
  bool cast_shadows = true;
  bool stationary = false;

  // non-serialized data
  uint32_t mesh_id = Asset::INVALID_ID;
  std::vector<Shared<PBRMaterial>> materials = {};
  glm::mat4 transform = glm::mat4{1};
  // std::vector<entt::entity> child_entities = {}; // filled at load
  std::vector<glm::mat4> child_transforms = {}; // filled at submit
  AABB aabb = {};
  bool dirty = false;

  MeshComponent() = default;

  explicit MeshComponent(const Shared<Mesh>& mesh) : mesh_base(mesh) {
    materials = mesh_base->_materials;
    mesh_id = mesh->get_id();
    dirty = true;
  }
};

struct SpriteComponent {
  Shared<SpriteMaterial> material = nullptr;
  uint32 layer = 0;

  bool sort_y = true;
  bool flip_x = false;

  // non-serialized data
  glm::mat4 transform = {};
  AABB rect = {};

  // set if an animation is controlling this sprite
  std::optional<glm::vec2> current_uv_offset = std::nullopt;

  SpriteComponent() {
    material = create_shared<SpriteMaterial>();
    material->create();
  }

  glm::vec3 get_position() const { return glm::vec3(transform[3]); }
  glm::vec2 get_size() const { return {glm::length(glm::vec3(transform[0])), glm::length(glm::vec3(transform[1]))}; }
};

struct SpriteAnimationComponent {
  uint32 num_frames = 0;
  bool loop = true;
  bool inverted = false;
  uint32 fps = 0;
  uint32 columns = 1;
  glm::vec2 frame_size = {};

  // non-serialized data
  float current_time = 0.f;

  void reset() { current_time = 0.f; }

  void set_frame_size(const Texture* sprite) {
    if (num_frames > 0) {
      const auto horizontal = sprite->get_extent().width / num_frames;
      const auto vertical = sprite->get_extent().height;

      frame_size = {horizontal, vertical};

      reset();
    }
  }

  void set_num_frames(uint32 value) {
    num_frames = value;
    reset();
  }

  void set_fps(uint32 value) {
    fps = value;
    reset();
  }

  void set_columns(uint32 value) {
    columns = value;
    reset();
  }
};

struct TilemapComponent {
  std::string path = {};
  ankerl::unordered_dense::map<std::string, Shared<SpriteMaterial>> layers = {};
  glm::ivec2 tilemap_size = {64, 64};

  TilemapComponent() {}

  void load(const std::string& _path) {
    path = _path;
    TilemapSerializer serializer(this);
    serializer.deserialize(path);
  }
};

struct CameraComponent {
  enum class Projection {
    Perspective = 0,
    Orthographic = 1,
  } projection = Projection::Perspective;

  float32 fov = 60.0f;
  float32 aspect = 16.0f / 9.0f;
  float far_clip = 1000.f;
  float near_clip = 0.01f;

  float yaw = -1.5708f; // - 90
  float pitch = 0.0f;
  float tilt = 0.0f;
  float zoom = 1.0f;

  // --- non-serialized data ---
  glm::vec2 jitter = {};
  glm::vec2 jitter_prev = {};

  struct Matrices {
    glm::mat4 view_matrix = {};
    glm::mat4 projection_matrix = {};
  };

  Matrices matrices = {};
  Matrices matrices_prev = {};

  glm::vec3 position = {};
  glm::vec3 forward = {};
  glm::vec3 up = {};
  glm::vec3 right = {};
  // ------

  glm::mat4 get_projection_matrix() const { return matrices.projection_matrix; }
  glm::mat4 get_inv_projection_matrix() const { return glm::inverse(matrices.projection_matrix); }
  glm::mat4 get_view_matrix() const { return matrices.view_matrix; }
  glm::mat4 get_inv_view_matrix() const { return glm::inverse(matrices.view_matrix); }
  glm::mat4 get_inverse_projection_view() const { return glm::inverse(matrices.projection_matrix * matrices.view_matrix); }

  glm::mat4 get_previous_projection_matrix() const { return matrices_prev.projection_matrix; }
  glm::mat4 get_previous_inv_projection_matrix() const { return glm::inverse(matrices_prev.projection_matrix); }
  glm::mat4 get_previous_view_matrix() const { return matrices_prev.view_matrix; }
  glm::mat4 get_previous_inv_view_matrix() const { return glm::inverse(matrices_prev.view_matrix); }
  glm::mat4 get_previous_inverse_projection_view() const { return glm::inverse(matrices_prev.projection_matrix * matrices_prev.view_matrix); }
};

struct ParticleSystemComponent {
  Shared<ParticleSystem> system = nullptr;

  ParticleSystemComponent() : system(create_shared<ParticleSystem>()) {}
};

struct LightComponent {
  enum LightType { Directional = 0, Point, Spot };

  LightType type = Point;
  bool color_temperature_mode = false;
  uint32_t temperature = 6570;
  glm::vec3 color = glm::vec3(1.0f);
  float intensity = 1.0f;

  float range = 1.0f;
  float radius = 0.025f;
  float length = 0;
  float outer_cone_angle = glm::pi<float>() / 4.0f;
  float inner_cone_angle = 0;

  bool cast_shadows = true;
  uint32_t shadow_map_res = 0;
  std::vector<float> cascade_distances = {8, 80, 800};

  // non-serialized data
  glm::vec3 position = {};
  glm::vec3 rotation = {};
  glm::vec3 direction = {};
  RectPacker::Rect shadow_rect = {};
};

struct PostProcessProbe {
  bool vignette_enabled = false;
  float vignette_intensity = 0.25f;

  bool film_grain_enabled = false;
  float film_grain_intensity = 0.2f;

  bool chromatic_aberration_enabled = false;
  float chromatic_aberration_intensity = 0.5f;

  bool sharpen_enabled = false;
  float sharpen_intensity = 0.5f;
};

// Physics
struct RigidbodyComponent {
  enum class BodyType { Static = 0, Kinematic, Dynamic };
  enum class AllowedDOFs : uint32 {
    None = 0b000000,         ///< No degrees of freedom are allowed. Note that this is not valid and will crash. Use a static body instead.
    All = 0b111111,          ///< All degrees of freedom are allowed
    TranslationX = 0b000001, ///< Body can move in world space X axis
    TranslationY = 0b000010, ///< Body can move in world space Y axis
    TranslationZ = 0b000100, ///< Body can move in world space Z axis
    RotationX = 0b001000,    ///< Body can rotate around world space X axis
    RotationY = 0b010000,    ///< Body can rotate around world space Y axis
    RotationZ = 0b100000,    ///< Body can rotate around world space Z axis
    Plane2D = TranslationX | TranslationY | RotationZ, ///< Body can only move in X and Y axis and rotate around Z axis
  };

  AllowedDOFs allowed_dofs = AllowedDOFs::All;
  BodyType type = BodyType::Dynamic;
  float mass = 1.0f;
  float linear_drag = 0.0f;
  float angular_drag = 0.05f;
  float gravity_scale = 1.0f;
  bool allow_sleep = true;
  bool awake = true;
  bool continuous = false;
  bool interpolation = false;

  bool is_sensor = false;

  // Stored as JPH::Body
  void* runtime_body = nullptr;

  // For interpolation/extrapolation
  glm::vec3 previous_translation = glm::vec3(0.0f);
  glm::quat previous_rotation = glm::vec3(0.0f);
  glm::vec3 translation = glm::vec3(0.0f);
  glm::quat rotation = glm::vec3(0.0f);

  JPH::Body* get_body() const { return static_cast<JPH::Body*>(runtime_body); }
};

struct BoxColliderComponent {
  glm::vec3 size = {0.5f, 0.5f, 0.5f};
  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct SphereColliderComponent {
  float radius = 0.5f;
  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct CapsuleColliderComponent {
  float height = 1.0f;
  float radius = 0.5f;
  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct TaperedCapsuleColliderComponent {
  float height = 1.0f;
  float top_radius = 0.5f;
  float bottom_radius = 0.5f;
  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct CylinderColliderComponent {
  float height = 1.0f;
  float radius = 0.5f;
  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct MeshColliderComponent {
  glm::vec3 offset = {0.0f, 0.0f, 0.0f};
  float friction = 0.5f;
  float restitution = 0.0f;
};

struct CharacterControllerComponent {
  Shared<JPH::Character> character = nullptr;

  // Size
  float character_height_standing = 1.35f;
  float character_radius_standing = 0.3f;
  float character_height_crouching = 0.8f;
  float character_radius_crouching = 0.3f;

  // Movement
  struct MovementSettings {
    float max_speed;
    float acceleration;
    float deceleration;

    MovementSettings(const float maxSpeed, float accel, float decel) : max_speed(maxSpeed), acceleration(accel), deceleration(decel) {}
  };

  bool interpolation = true;

  bool control_movement_during_jump = true;
  float jump_force = 8.0f;
  bool auto_bunny_hop = true;
  float air_control = 0.3f;
  MovementSettings ground_settings = MovementSettings(7, 14, 10);
  MovementSettings air_settings = MovementSettings(7, 2, 2);
  MovementSettings strafe_settings = MovementSettings(0.0f, 50, 50);

  float friction = 6.0f;
  float gravity = 20;
  float collision_tolerance = 0.05f;

  // For interpolation/extrapolation
  glm::vec3 previous_translation = glm::vec3(0.0f);
  glm::quat previous_rotation = glm::vec3(0.0f);
  glm::vec3 translation = glm::vec3(0.0f);
  glm::quat rotation = glm::vec3(0.0f);

  CharacterControllerComponent() = default;
};

// Audio
struct AudioSourceComponent {
  AudioSourceConfig config;

  Shared<AudioSource> source = nullptr;
};

struct AudioListenerComponent {
  bool active = true;
  AudioListenerConfig config;

  Shared<AudioListener> listener;
};

// Scripting
struct LuaScriptComponent {
  std::vector<Shared<LuaSystem>> lua_systems = {};
};

struct CPPScriptComponent {
  std::vector<Shared<System>> systems = {};

  template <typename T>
  void add_system() {
    auto system = App::get_system<SystemManager>(EngineSystems::SystemManager)->register_system<T>();
    systems.emplace(system);
  }
};
} // namespace ox
