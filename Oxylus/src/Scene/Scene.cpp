#include "Scene.hpp"

#include <Core/FileSystem.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/AllowedDOFs.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <sol/state.hpp>

#include "Core/App.hpp"
#include "Memory/Stack.hpp"
#include "Physics/Physics.hpp"
#include "Physics/PhysicsMaterial.hpp"
#include "Render/RenderPipeline.hpp"
#include "Scene/ComponentWrapper.hpp"
#include "Scene/Components.hpp"
#include "SceneRenderer.hpp"
#include "Scripting/LuaManager.hpp"
#include "Utils/Timestep.hpp"

namespace ox {
void serialize_vec2(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                    const glm::vec2& vec) {
  writer.StartArray();
  writer.Double(vec.x);
  writer.Double(vec.y);
  writer.EndArray();
}

void serialize_vec3(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                    const glm::vec3& vec) {
  writer.StartArray();
  writer.Double(vec.x);
  writer.Double(vec.y);
  writer.Double(vec.z);
  writer.EndArray();
}

void serialize_vec4(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                    const glm::vec4& vec) {
  writer.StartArray();
  writer.Double(vec.x);
  writer.Double(vec.y);
  writer.Double(vec.z);
  writer.Double(vec.w);
  writer.EndArray();
}

auto deserialize_vec2(const rapidjson::GenericArray<true,
                                                    rapidjson::GenericValue<rapidjson::UTF8<>>>& array,
                      glm::vec2* v) -> void {
  v->x = static_cast<float>(array[0].GetDouble());
  v->y = static_cast<float>(array[1].GetDouble());
}

auto deserialize_vec3(const rapidjson::GenericArray<true,
                                                    rapidjson::GenericValue<rapidjson::UTF8<>>>& array,
                      glm::vec3* v) -> void {
  v->x = static_cast<float>(array[0].GetDouble());
  v->y = static_cast<float>(array[1].GetDouble());
  v->z = static_cast<float>(array[2].GetDouble());
}

auto deserialize_vec4(const rapidjson::GenericArray<true,
                                                    rapidjson::GenericValue<rapidjson::UTF8<>>>& array,
                      glm::vec4* v) -> void {
  v->x = static_cast<float>(array[0].GetDouble());
  v->y = static_cast<float>(array[1].GetDouble());
  v->z = static_cast<float>(array[2].GetDouble());
  v->w = static_cast<float>(array[3].GetDouble());
}

Scene::Scene() { init("Untitled"); }

Scene::Scene(const std::string& name) { init(name); }

Scene::~Scene() {
  const auto* lua_manager = App::get_system<LuaManager>(EngineSystems::LuaManager);
  lua_manager->get_state()->collect_gc();

  if (running)
    on_runtime_stop();
}

auto Scene::init(this Scene& self,
                 const std::string& name) -> void {
  OX_SCOPED_ZONE;

  self.scene_name = name;

  // Renderer
  self.scene_renderer = create_unique<SceneRenderer>(&self);
  self.scene_renderer->init();
}

auto Scene::create_entity(const std::string& name) const -> flecs::entity {
  OX_SCOPED_ZONE;

  flecs::entity e = flecs::entity::null();
  if (name.empty()) {
    memory::ScopedStack stack;

    e = world.entity();
    e.set_name(stack.format_char("Entity {}", e.raw_id()));
  } else {
    e = world.entity(name.c_str());
  }

  return e.add<TransformComponent>().add<LayerComponent>();
}

auto Scene::on_runtime_start() -> void {
  OX_SCOPED_ZONE;

  running = true;

  physics_frame_accumulator = 0.0f;

  // Physics
  {
    OX_SCOPED_ZONE_N("Physics Start");
    body_activation_listener_3d = new Physics3DBodyActivationListener();
    contact_listener_3d = new Physics3DContactListener(this);
    const auto physics_system = App::get_system<Physics>(EngineSystems::Physics)->get_physics_system();
    physics_system->SetBodyActivationListener(body_activation_listener_3d);
    physics_system->SetContactListener(contact_listener_3d);

    // Rigidbodies
    world.query_builder<const TransformComponent, RigidbodyComponent>().build().each(
        [this](flecs::entity e, const TransformComponent& tc, RigidbodyComponent& rb) {
      rb.previous_translation = rb.translation = tc.position;
      rb.previous_rotation = rb.rotation = tc.rotation;
      create_rigidbody(e, tc, rb);
    });

    // Characters
    world.query_builder<const TransformComponent, CharacterControllerComponent>().build().each(
        [this](const TransformComponent& tc, CharacterControllerComponent& ch) { create_character_controller(tc, ch); });

    physics_system->OptimizeBroadPhase();
  }

  // Scripting
  {
    OX_SCOPED_ZONE_N("LuaScripting/on_init");
    world.query_builder<const LuaScriptComponent>().build().each([this](const flecs::entity& e, const LuaScriptComponent& lsc) {
      for (const auto& script : lsc.lua_systems) {
        script->reload();
        script->on_init(this, e);
      }
    });
  }

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_init");
    world.query_builder<const CPPScriptComponent>().build().each([this](const flecs::entity& e, const CPPScriptComponent& csc) {
      for (const auto& script : csc.systems) {
        script->on_init(this, e);
      }
    });
  }
}

auto Scene::on_runtime_stop() -> void {
  OX_SCOPED_ZONE;

  running = false;

  // Physics
  {
    const auto physics = App::get_system<Physics>(EngineSystems::Physics);
    world.query_builder<RigidbodyComponent>().build().each([physics](const flecs::entity& e, const RigidbodyComponent& rb) {
      if (rb.runtime_body) {
        JPH::BodyInterface& body_interface = physics->get_physics_system()->GetBodyInterface();
        const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);
        body_interface.RemoveBody(body->GetID());
        body_interface.DestroyBody(body->GetID());
      }
    });
    world.query_builder<CharacterControllerComponent>().build().each([physics](const flecs::entity& e, CharacterControllerComponent& ch) {
      if (ch.character) {
        JPH::BodyInterface& body_interface = physics->get_physics_system()->GetBodyInterface();
        body_interface.RemoveBody(ch.character->GetBodyID());
        ch.character = nullptr;
      }
    });

    delete body_activation_listener_3d;
    delete contact_listener_3d;
    body_activation_listener_3d = nullptr;
    contact_listener_3d = nullptr;
  }

  // Scripting
  {
    OX_SCOPED_ZONE_N("LuaScripting/on_release");
    world.query_builder<const LuaScriptComponent>().build().each([this](const flecs::entity& e, const LuaScriptComponent& lsc) {
      for (const auto& script : lsc.lua_systems) {
        script->on_release(this, e);
      }
    });
  }

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_release");
    world.query_builder<const CPPScriptComponent>().build().each([this](const flecs::entity& e, const CPPScriptComponent& csc) {
      for (const auto& script : csc.systems) {
        script->on_release(this, e);
      }
    });
  }
}

auto Scene::copy(const Shared<Scene>& src_scene) -> Shared<Scene> {
  OX_SCOPED_ZONE;
  Shared<Scene> new_scene = create_shared<Scene>();

  OX_LOG_ERROR("TODO: Scene::copy");

  return new_scene;
}

auto Scene::get_world_transform(const flecs::entity entity) const -> glm::mat4 {
  const auto* tc = entity.get<TransformComponent>();
  const auto parent = entity.parent();
  const glm::mat4 parent_transform = parent != flecs::entity::null() ? get_world_transform(parent) : glm::mat4(1.0f);
  return parent_transform * glm::translate(glm::mat4(1.0f), tc->position) * glm::toMat4(glm::quat(tc->rotation)) *
         glm::scale(glm::mat4(1.0f), tc->scale);
}

auto Scene::get_local_transform(flecs::entity entity) const -> glm::mat4 {
  const auto* tc = entity.get<TransformComponent>();
  return glm::translate(glm::mat4(1.0f), tc->position) * glm::toMat4(glm::quat(tc->rotation)) * glm::scale(glm::mat4(1.0f), tc->scale);
}

auto Scene::on_contact_added(const JPH::Body& body1,
                             const JPH::Body& body2,
                             const JPH::ContactManifold& manifold,
                             const JPH::ContactSettings& settings) -> void {
  OX_SCOPED_ZONE;

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_contact_added");
    world.query_builder<const CPPScriptComponent>().build().each(
        [this, &body1, &body2, &manifold, &settings](const flecs::entity& e, const CPPScriptComponent& csc) {
      for (const auto& script : csc.systems) {
        script->on_contact_added(this, e, body1, body2, manifold, settings);
      }
    });
  }

  {
    OX_SCOPED_ZONE_N("LuaScripting/on_contact_added");
    world.query_builder<const LuaScriptComponent>().build().each(
        [this, &body1, &body2, &manifold, &settings](const flecs::entity& e, const LuaScriptComponent& lsc) {
      for (const auto& script : lsc.lua_systems) {
        script->on_contact_added(this, e, body1, body2, manifold, settings);
      }
    });
  }
}

auto Scene::on_contact_persisted(const JPH::Body& body1,
                                 const JPH::Body& body2,
                                 const JPH::ContactManifold& manifold,
                                 const JPH::ContactSettings& settings) -> void {
  OX_SCOPED_ZONE;

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_contact_persisted");
    world.query_builder<const CPPScriptComponent>().build().each(
        [this, &body1, &body2, &manifold, &settings](const flecs::entity& e, const CPPScriptComponent& csc) {
      for (const auto& script : csc.systems) {
        script->on_contact_added(this, e, body1, body2, manifold, settings);
      }
    });
  }

  {
    OX_SCOPED_ZONE_N("LuaScripting/on_contact_persisted");
    world.query_builder<const LuaScriptComponent>().build().each(
        [this, &body1, &body2, &manifold, &settings](const flecs::entity& e, const LuaScriptComponent& lsc) {
      for (const auto& script : lsc.lua_systems) {
        script->on_contact_persisted(this, e, body1, body2, manifold, settings);
      }
    });
  }
}

auto Scene::create_rigidbody(flecs::entity entity,
                             const TransformComponent& transform,
                             RigidbodyComponent& component) -> void {
  OX_SCOPED_ZONE;
  if (!running)
    return;

  // TODO: We should get rid of 'new' usages and use JPH::Ref<> instead.

  auto physics = App::get_system<Physics>(EngineSystems::Physics);

  auto& body_interface = physics->get_body_interface();
  if (component.runtime_body) {
    body_interface.DestroyBody(static_cast<JPH::Body*>(component.runtime_body)->GetID());
    component.runtime_body = nullptr;
  }

  JPH::MutableCompoundShapeSettings compound_shape_settings;
  float max_scale_component = glm::max(glm::max(transform.scale.x, transform.scale.y), transform.scale.z);

  const auto entity_name = std::string(entity.name());

  if (const auto* bc = entity.get<BoxColliderComponent>()) {
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), bc->friction, bc->restitution);

    glm::vec3 scale = bc->size;
    JPH::BoxShapeSettings shape_settings({glm::abs(scale.x), glm::abs(scale.y), glm::abs(scale.z)}, 0.05f, mat);
    shape_settings.SetDensity(glm::max(0.001f, bc->density));

    compound_shape_settings.AddShape({bc->offset.x, bc->offset.y, bc->offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (const auto* sc = entity.get<SphereColliderComponent>()) {
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), sc->friction, sc->restitution);

    float radius = 2.0f * sc->radius * max_scale_component;
    JPH::SphereShapeSettings shape_settings(glm::max(0.01f, radius), mat);
    shape_settings.SetDensity(glm::max(0.001f, sc->density));

    compound_shape_settings.AddShape({sc->offset.x, sc->offset.y, sc->offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (const auto* cc = entity.get<CapsuleColliderComponent>()) {
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), cc->friction, cc->restitution);

    float radius = 2.0f * cc->radius * max_scale_component;
    JPH::CapsuleShapeSettings shape_settings(glm::max(0.01f, cc->height) * 0.5f, glm::max(0.01f, radius), mat);
    shape_settings.SetDensity(glm::max(0.001f, cc->density));

    compound_shape_settings.AddShape({cc->offset.x, cc->offset.y, cc->offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (const auto* tcc = entity.get<TaperedCapsuleColliderComponent>()) {
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), tcc->friction, tcc->restitution);

    float top_radius = 2.0f * tcc->top_radius * max_scale_component;
    float bottom_radius = 2.0f * tcc->bottom_radius * max_scale_component;
    JPH::TaperedCapsuleShapeSettings shape_settings(glm::max(0.01f, tcc->height) * 0.5f,
                                                    glm::max(0.01f, top_radius),
                                                    glm::max(0.01f, bottom_radius),
                                                    mat);
    shape_settings.SetDensity(glm::max(0.001f, tcc->density));

    compound_shape_settings.AddShape({tcc->offset.x, tcc->offset.y, tcc->offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (const auto* cc = entity.get<CylinderColliderComponent>()) {
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), cc->friction, cc->restitution);

    float radius = 2.0f * cc->radius * max_scale_component;
    JPH::CylinderShapeSettings shape_settings(glm::max(0.01f, cc->height) * 0.5f, glm::max(0.01f, radius), 0.05f, mat);
    shape_settings.SetDensity(glm::max(0.001f, cc->density));

    compound_shape_settings.AddShape({cc->offset.x, cc->offset.y, cc->offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (const auto* mc = entity.get<MeshColliderComponent>()) {
    if (const auto* mesh_component = entity.get<MeshComponent>()) {
      const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), mc->friction, mc->restitution);

      // TODO: We should only get the vertices and indices for this particular MeshComponent using MeshComponent::node_index

      auto vertices = mesh_component->mesh_base->_vertices;
      const auto& indices = mesh_component->mesh_base->_indices;

      // scale vertices
      const auto world_transform = get_world_transform(entity);
      for (auto& vert : vertices) {
        glm::vec4 scaled_pos = world_transform * glm::vec4(vert.position, 1.0);
        vert.position = glm::vec3(scaled_pos);
      }

      const uint32_t vertex_count = static_cast<uint32_t>(vertices.size());
      const uint32_t index_count = static_cast<uint32_t>(indices.size());
      const uint32_t triangle_count = vertex_count / 3;

      JPH::VertexList vertex_list;
      vertex_list.resize(vertex_count);
      for (uint32_t i = 0; i < vertex_count; ++i)
        vertex_list[i] = JPH::Float3(vertices[i].position.x, vertices[i].position.y, vertices[i].position.z);

      JPH::IndexedTriangleList indexedTriangleList;
      indexedTriangleList.resize(index_count * 2);

      for (uint32_t i = 0; i < triangle_count; ++i) {
        indexedTriangleList[i * 2 + 0].mIdx[0] = indices[i * 3 + 0];
        indexedTriangleList[i * 2 + 0].mIdx[1] = indices[i * 3 + 1];
        indexedTriangleList[i * 2 + 0].mIdx[2] = indices[i * 3 + 2];

        indexedTriangleList[i * 2 + 1].mIdx[2] = indices[i * 3 + 0];
        indexedTriangleList[i * 2 + 1].mIdx[1] = indices[i * 3 + 1];
        indexedTriangleList[i * 2 + 1].mIdx[0] = indices[i * 3 + 2];
      }

      JPH::PhysicsMaterialList material_list = {};
      material_list.emplace_back(mat);
      JPH::MeshShapeSettings shape_settings(vertex_list, indexedTriangleList, material_list);
      compound_shape_settings.AddShape({mc->offset.x, mc->offset.y, mc->offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
    }
  }

  // Body
  auto rotation = glm::quat(transform.rotation);

  uint8_t layer_index = 1; // Default Layer
  if (auto layer_component = entity.get<LayerComponent>()) {
    const auto collision_mask_it = physics->layer_collision_mask.find(layer_component->layer);
    if (collision_mask_it != physics->layer_collision_mask.end())
      layer_index = collision_mask_it->second.index;
  }

  JPH::BodyCreationSettings body_settings(compound_shape_settings.Create().Get(),
                                          {transform.position.x, transform.position.y, transform.position.z},
                                          {rotation.x, rotation.y, rotation.z, rotation.w},
                                          static_cast<JPH::EMotionType>(component.type),
                                          layer_index);

  JPH::MassProperties mass_properties;
  mass_properties.mMass = glm::max(0.01f, component.mass);
  body_settings.mMassPropertiesOverride = mass_properties;
  body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
  body_settings.mAllowSleeping = component.allow_sleep;
  body_settings.mLinearDamping = glm::max(0.0f, component.linear_drag);
  body_settings.mAngularDamping = glm::max(0.0f, component.angular_drag);
  body_settings.mMotionQuality = component.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
  body_settings.mGravityFactor = component.gravity_scale;
  body_settings.mAllowedDOFs = static_cast<JPH::EAllowedDOFs>(component.allowed_dofs);

  body_settings.mIsSensor = component.is_sensor;

  JPH::Body* body = body_interface.CreateBody(body_settings);

  JPH::EActivation activation =
      component.awake && component.type != RigidbodyComponent::BodyType::Static ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
  body_interface.AddBody(body->GetID(), activation);

  body->SetUserData((uint64)entity);

  component.runtime_body = body;
}

void Scene::create_character_controller(const TransformComponent& transform,
                                        CharacterControllerComponent& component) const {
  OX_SCOPED_ZONE;
  if (!running)
    return;

  const auto physics = App::get_system<Physics>(EngineSystems::Physics);

  const auto position = JPH::Vec3(transform.position.x, transform.position.y, transform.position.z);
  const auto capsule_shape =
      JPH::RotatedTranslatedShapeSettings(JPH::Vec3(0, 0.5f * component.character_height_standing + component.character_radius_standing, 0),
                                          JPH::Quat::sIdentity(),
                                          new JPH::CapsuleShape(0.5f * component.character_height_standing, component.character_radius_standing))
          .Create()
          .Get();

  // Create character
  const Shared<JPH::CharacterSettings> settings = create_shared<JPH::CharacterSettings>();
  settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
  settings->mLayer = PhysicsLayers::MOVING;
  settings->mShape = capsule_shape;
  settings->mFriction = 0.0f;                                                     // For now this is not set.
  settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(),
                                           -component.character_radius_standing); // Accept contacts that touch the lower sphere of the capsule
  component.character = create_shared<JPH::Character>(settings.get(), position, JPH::Quat::sIdentity(), 0, physics->get_physics_system());
  component.character->AddToPhysicsSystem(JPH::EActivation::Activate);
}

auto entity_to_json(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                    flecs::entity root) -> void {
  const auto q = root.world() //
                     .query_builder()
                     .with(flecs::ChildOf, root)
                     .build();

  q.each([&](flecs::entity e) {
    writer.StartObject();
    writer.String("name");
    writer.String(e.name());

    std::vector<ECS::ComponentWrapper> components = {};
    writer.String("tags");
    writer.StartArray();
    e.each([&](flecs::id component_id) {
      if (!component_id.is_entity()) {
        return;
      }

      ECS::ComponentWrapper component(e, component_id);
      if (!component.has_component()) {
        writer.String(component.path.c_str());
      } else {
        components.emplace_back(e, component_id);
      }
    });
    writer.EndArray();

    writer.String("components");
    writer.StartArray();
    for (auto& component : components) {
      writer.StartObject();
      writer.String("name");
      writer.String(component.name.data());
      component.for_each([&](usize&, std::string_view member_name, ECS::ComponentWrapper::Member& member) {
        writer.String(member_name.data());
        std::visit(ox::match{
                       [](const auto&) {},
                       [&](float32* v) { writer.Double(*v); },
                       [&](int32* v) { writer.Int(*v); },
                       [&](uint32* v) { writer.Uint(*v); },
                       [&](int64* v) { writer.Int64(*v); },
                       [&](uint64* v) { writer.Uint64(*v); },
                       [&](glm::vec2* v) { serialize_vec2(writer, *v); },
                       [&](glm::vec3* v) { serialize_vec3(writer, *v); },
                       [&](glm::vec4* v) { serialize_vec4(writer, *v); },
                       [&](glm::quat* v) { serialize_vec4(writer, glm::vec4{v->x, v->y, v->z, v->w}); },
                       [&](glm::mat4* v) {}, // do nothing
                       [&](std::string* v) { writer.String(v->c_str()); },
                       // [&](UUID* v) { member_json = v->str().c_str(); }, TODO:
                   },
                   member);
      });
      writer.EndObject();
    }
    writer.EndArray();

    writer.String("children");
    writer.StartArray();
    entity_to_json(writer, e);
    writer.EndArray();

    writer.EndObject();
  });
}

auto Scene::save_to_file(this Scene& self,
                         std::string path) -> bool {
  OX_SCOPED_ZONE;
  rapidjson::StringBuffer sb;

  rapidjson::PrettyWriter writer(sb);
  writer.StartObject(); // root

  writer.String("name");
  writer.String(self.scene_name.c_str());

  writer.String("entities");
  writer.StartArray(); // entities
  entity_to_json(writer, self.root);
  writer.EndArray();   // entities

  writer.EndObject();  // root

  std::ofstream filestream(path);
  filestream << sb.GetString();

  OX_LOG_INFO("Saved scene {0}.", self.scene_name);

  return true;
}

auto json_to_entity(Scene& self,
                    flecs::entity root,
                    const rapidjson::GenericValue<rapidjson::UTF8<>>& json,
                    std::vector<UUID>& requested_assets) -> bool {
  OX_SCOPED_ZONE;
  memory::ScopedStack stack;

  const auto& world = self.world;

  const auto entity_name = json["name"].GetString();
  if (entity_name == nullptr) {
    OX_LOG_ERROR("Entities must have names!");
    return false;
  }

  auto e = self.create_entity(std::string(entity_name));
  e.child_of(root);

  const auto entity_tags_json = json["tags"].GetArray();
  for (const auto& entity_tag : entity_tags_json) {
    auto tag = world.component(stack.null_terminate(entity_tag.GetString()).data());
    e.add(tag);
  }

  const auto components_json = json["components"].GetArray();
  for (const auto& component_json : components_json) {
    const auto component_name = component_json["name"].GetString();
    if (component_name == nullptr) {
      OX_LOG_ERROR("Entity '{}' has corrupt components JSON array.", e.name().c_str());
      return false;
    }

    auto component_id = world.lookup(component_name);
    if (!component_id) {
      OX_LOG_ERROR("Entity '{}' has invalid component named '{}'!", e.name().c_str(), component_name);
      return false;
    }

    // OX_CHECK_EQ(self.get_entity_db().is_component_known(component_id));
    e.add(component_id);
    ECS::ComponentWrapper component(e, component_id);
    component.for_each([&](usize&, std::string_view member_name, ECS::ComponentWrapper::Member& member) {
      const auto& member_json = component_json[member_name.data()];

      auto match_result = ox::match{
          [](const auto&) {},
          [&](float32* v) { *v = member_json.GetFloat(); },
          [&](int32* v) { *v = member_json.GetInt(); },
          [&](uint32* v) { *v = member_json.GetUint(); },
          [&](int64* v) { *v = member_json.GetInt64(); },
          [&](uint64* v) { *v = member_json.GetUint64(); },
          [&](glm::vec2* v) { deserialize_vec2(member_json.GetArray(), v); },
          [&](glm::vec3* v) { deserialize_vec3(member_json.GetArray(), v); },
          [&](glm::vec4* v) { deserialize_vec4(member_json.GetArray(), v); },
          [&](glm::quat* v) { deserialize_vec4(member_json.GetArray(), reinterpret_cast<glm::vec4*>(v)); },
          // [&](glm::mat4 *v) {json_to_mat(member_json.value(), *v); },
          [&](std::string* v) { *v = member_json.GetString(); },
          [&](UUID* v) {
        // *v = UUID::from_string(member_json.GetString()).value();
        // requested_assets.push_back(*v);
      },
      };

      std::visit(match_result, member);
    });

    e.modified(component_id);
  }

  const auto children_json = json["children"].GetArray();
  for (const auto& children : children_json) {
    if (!json_to_entity(self, e, children, requested_assets)) {
      return false;
    }
  }

  return true;
}

auto Scene::load_from_file(this Scene& self,
                           const std::string& path) -> bool {
  OX_SCOPED_ZONE;
  const auto content = fs::read_file(path);
  if (content.empty()) {
    OX_LOG_ERROR("Failed to read/open file {}!", path);
    return false;
  }

  rapidjson::Document doc;
  doc.Parse(content.data());

  const rapidjson::ParseResult parse_result = doc.Parse(content.c_str());

  if (doc.HasParseError()) {
    OX_LOG_ERROR("Json parser error for: {0} {1}", path, rapidjson::GetParseError_En(parse_result.Code()));
    return false;
  }

  self.scene_name = doc["name"].GetString();

  const auto entities_array = doc["entities"].GetArray();

  std::vector<UUID> requested_assets = {};
  for (auto& entity_json : entities_array) {
    if (!json_to_entity(self, self.root, entity_json, requested_assets)) {
      return false;
    }
  }

  OX_LOG_TRACE("Loading scene {} with {} assets...", self.scene_name, requested_assets.size());
  // for (const auto& uuid : requested_assets) {
  // auto* app = App::get();
  // if (uuid && app.asset_man.get_asset(uuid)) {
  // app.asset_man.load_asset(uuid);
  // }
  // }

  return true;
}

void Scene::on_runtime_update(const Timestep& delta_time) {
  OX_SCOPED_ZONE;

  // TODO: maybe bindings should be done once at entity creation...
  const auto cpp_scripts_query = world.query_builder<const CPPScriptComponent>().build();
  {
    OX_SCOPED_ZONE_N("CPPScripting/binding");
    cpp_scripts_query.each([this](const flecs::entity& e, const CPPScriptComponent& c) {
      for (const auto& system : c.systems) {
        system->bind_globals(this, e);
      }
    });
  }

  const auto lua_scripts_query = world.query_builder<const LuaScriptComponent>().build();
  {
    OX_SCOPED_ZONE_N("LuaScripting/binding");
    lua_scripts_query.each([this, delta_time](const flecs::entity& e, const LuaScriptComponent& c) {
      for (const auto& script : c.lua_systems) {
        script->bind_globals(this, e, delta_time);
      }
    });
  }

  scene_renderer->update(delta_time);

  update_physics(delta_time);

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_update");
    cpp_scripts_query.each([delta_time](const flecs::entity& e, const CPPScriptComponent& c) {
      for (const auto& system : c.systems) {
        system->on_update(delta_time);
      }
    });
  }

  {
    OX_SCOPED_ZONE_N("LuaScripting/on_update");
    lua_scripts_query.each([delta_time](const flecs::entity& e, const LuaScriptComponent& c) {
      for (const auto& script : c.lua_systems) {
        script->on_update(delta_time);
      }
    });
  }

  // Audio
  {
    OX_SCOPED_ZONE_N("Audio Systems");
    world.query_builder<const TransformComponent, AudioListenerComponent>().build().each(
        [this](const flecs::entity& e, const TransformComponent& tc, AudioListenerComponent& ac) {
      ac.listener = create_shared<AudioListener>();
      if (ac.active) {
        const glm::mat4 inverted = glm::inverse(get_world_transform(e));
        const glm::vec3 forward = normalize(glm::vec3(inverted[2]));
        ac.listener->set_config(ac.config);
        ac.listener->set_position(tc.position);
        ac.listener->set_direction(-forward);
      }
    });

    world.query_builder<const TransformComponent, AudioSourceComponent>().build().each(
        [this](const flecs::entity& e, const TransformComponent& tc, const AudioSourceComponent& ac) {
      if (ac.source) {
        const glm::mat4 inverted = glm::inverse(get_world_transform(e));
        const glm::vec3 forward = normalize(glm::vec3(inverted[2]));
        ac.source->set_config(ac.config);
        ac.source->set_position(tc.position);
        ac.source->set_direction(forward);
        if (ac.config.play_on_awake)
          ac.source->play();
      }
    });
  }
}

void Scene::on_render(const vuk::Extent3D extent,
                      const vuk::Format format) {
  OX_SCOPED_ZONE;

  {
    OX_SCOPED_ZONE_N("CPPScripting/on_render");
    world.query_builder<const CPPScriptComponent>().build().each([extent, format](const CPPScriptComponent& c) {
      for (const auto& system : c.systems) {
        system->on_render(extent, format);
      }
    });
  }

  {
    OX_SCOPED_ZONE_N("LuaScripting/on_render");
    world.query_builder<const LuaScriptComponent>().build().each([extent, format](const LuaScriptComponent& c) {
      for (const auto& script : c.lua_systems) {
        script->on_render(extent, format);
      }
    });
  }
}

void Scene::on_editor_update(const Timestep& delta_time,
                             const CameraComponent& camera) const {
  OX_SCOPED_ZONE;
  scene_renderer->get_render_pipeline()->submit_camera(camera);
  scene_renderer->update(delta_time);
}

auto Scene::update_physics(const Timestep& delta_time) -> void {
  OX_SCOPED_ZONE;
  // Minimum stable value is 16.0
  constexpr float physics_step_rate = 50.0f;
  constexpr float physics_ts = 1.0f / physics_step_rate;

  bool stepped = false;
  physics_frame_accumulator += static_cast<float>(delta_time.get_seconds());

  auto* physics = App::get_system<Physics>(EngineSystems::Physics);

  while (physics_frame_accumulator >= physics_ts) {
    physics->step(physics_ts);

    {
      OX_SCOPED_ZONE_N("CPPScripting/on_fixed_update");
      world.query_builder<const CPPScriptComponent>().build().each([](const CPPScriptComponent& c) {
        for (const auto& system : c.systems) {
          system->on_fixed_update(physics_ts);
        }
      });
    }

    {
      OX_SCOPED_ZONE_N("LuaScripting/on_fixed_update");
      world.query_builder<const LuaScriptComponent>().build().each([](const LuaScriptComponent& c) {
        for (const auto& system : c.lua_systems) {
          system->on_fixed_update(physics_ts);
        }
      });
    }

    physics_frame_accumulator -= physics_ts;
    stepped = true;
  }

  const float interpolation_factor = physics_frame_accumulator / physics_ts;

  world.query_builder<TransformComponent, RigidbodyComponent>().build().each(
      [physics, stepped, interpolation_factor](TransformComponent& tc, RigidbodyComponent& rb) {
    if (!rb.runtime_body)
      return;

    const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);
    const auto& body_interface = physics->get_physics_system()->GetBodyInterface();

    if (!body_interface.IsActive(body->GetID()))
      return;

    if (rb.interpolation) {
      if (stepped) {
        const JPH::Vec3 position = body->GetPosition();
        const JPH::Vec3 rotation = body->GetRotation().GetEulerAngles();

        rb.previous_translation = rb.translation;
        rb.previous_rotation = rb.rotation;
        rb.translation = {position.GetX(), position.GetY(), position.GetZ()};
        rb.rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
      }

      tc.position = glm::lerp(rb.previous_translation, rb.translation, interpolation_factor);
      tc.rotation = glm::eulerAngles(glm::slerp(rb.previous_rotation, rb.rotation, interpolation_factor));
    } else {
      const JPH::Vec3 position = body->GetPosition();
      const JPH::Vec3 rotation = body->GetRotation().GetEulerAngles();

      rb.previous_translation = rb.translation;
      rb.previous_rotation = rb.rotation;
      rb.translation = {position.GetX(), position.GetY(), position.GetZ()};
      rb.rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
      tc.position = rb.translation;
      tc.rotation = glm::eulerAngles(rb.rotation);
    }
  });

  // Character
  {
    world.query_builder<TransformComponent, CharacterControllerComponent>().build().each(
        [stepped, interpolation_factor](TransformComponent& tc, CharacterControllerComponent& ch) {
      ch.character->PostSimulation(ch.collision_tolerance);
      if (ch.interpolation) {
        if (stepped) {
          const JPH::Vec3 position = ch.character->GetPosition();
          const JPH::Vec3 rotation = ch.character->GetRotation().GetEulerAngles();

          ch.previous_translation = ch.translation;
          ch.previous_rotation = ch.rotation;
          ch.translation = {position.GetX(), position.GetY(), position.GetZ()};
          ch.rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
        }

        tc.position = glm::lerp(ch.previous_translation, ch.translation, interpolation_factor);
        tc.rotation = glm::eulerAngles(glm::slerp(ch.previous_rotation, ch.rotation, interpolation_factor));
      } else {
        const JPH::Vec3 position = ch.character->GetPosition();
        const JPH::Vec3 rotation = ch.character->GetRotation().GetEulerAngles();

        ch.previous_translation = ch.translation;
        ch.previous_rotation = ch.rotation;
        ch.translation = {position.GetX(), position.GetY(), position.GetZ()};
        ch.rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
        tc.position = ch.translation;
        tc.rotation = glm::eulerAngles(ch.rotation);
      }
    });
  }
}
} // namespace ox
