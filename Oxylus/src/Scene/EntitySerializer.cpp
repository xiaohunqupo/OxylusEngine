#include "EntitySerializer.hpp"

#include <filesystem>

#include "Core/Base.hpp"
#include "Core/FileSystem.hpp"
#include "Core/Systems/SystemManager.hpp"
#include "Scene.hpp"
#include "Scene/Components.hpp"

#include "Assets/AssetManager.hpp"

#include "Core/App.hpp"

#include "Core/VFS.hpp"
#include "Entity.hpp"
#include "Utils/Archive.hpp"

#include "Utils/Log.hpp"

namespace ox {
template <typename Component, typename WriterFunc>
void serialize_component(const std::string_view name,
                         entt::registry& registry,
                         entt::entity entity,
                         rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                         WriterFunc serializeFunc) {
  if (!registry.all_of<Component>(entity))
    return;

  writer.String(name.data());
  writer.StartArray();
  for (auto&& [e, tc] : registry.view<Component>().each()) {
    writer.StartObject();
    serializeFunc(writer, tc);
    writer.EndObject();
  }
  writer.EndArray();
}

void serialize_vec2(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const glm::vec2& vec) {
  writer.StartArray();
  writer.Double(vec.x);
  writer.Double(vec.y);
  writer.EndArray();
}

void serialize_vec3(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const glm::vec3& vec) {
  writer.StartArray();
  writer.Double(vec.x);
  writer.Double(vec.y);
  writer.Double(vec.z);
  writer.EndArray();
}

void serialize_vec4(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const glm::vec4& vec) {
  writer.StartArray();
  writer.Double(vec.x);
  writer.Double(vec.y);
  writer.Double(vec.z);
  writer.Double(vec.w);
  writer.EndArray();
}

glm::vec2 deserialize_vec2(const rapidjson::GenericArray<true, rapidjson::GenericValue<rapidjson::UTF8<>>>& array) {
  return glm::vec2{
    array[0].GetDouble(),
    array[1].GetDouble(),
  };
}

glm::vec3 deserialize_vec3(const rapidjson::GenericArray<true, rapidjson::GenericValue<rapidjson::UTF8<>>>& array) {
  return glm::vec3{
    array[0].GetDouble(),
    array[1].GetDouble(),
    array[2].GetDouble(),
  };
}

glm::vec4 deserialize_vec4(const rapidjson::GenericArray<true, rapidjson::GenericValue<rapidjson::UTF8<>>>& array) {
  return glm::vec4{
    array[0].GetDouble(),
    array[1].GetDouble(),
    array[2].GetDouble(),
    array[3].GetDouble(),
  };
}

void EntitySerializer::serialize_entity(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, Scene* scene, Entity entity) {
  OX_SCOPED_ZONE;

  writer.StartObject(); // top

  // uuid
  writer.String("uuid");
  writer.Uint64(eutil::get_uuid(scene->registry, entity));

  serialize_component<TagComponent>("TagComponent",
                                    scene->registry,
                                    entity,
                                    writer,
                                    [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, const TagComponent& c) {
    wr.String("tag");
    wr.String(c.tag.c_str());

    wr.String("enabled");
    wr.Bool(c.enabled);
  });

  serialize_component<RelationshipComponent>("RelationshipComponent",
                                             scene->registry,
                                             entity,
                                             writer,
                                             [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, const RelationshipComponent& c) {
    wr.String("parent");
    wr.Uint64(c.parent);

    wr.String("childs");
    wr.StartArray();
    for (auto child : c.children) {
      wr.Uint64(child);
    }
    wr.EndArray();
  });

  serialize_component<TransformComponent>("TransformComponent",
                                          scene->registry,
                                          entity,
                                          writer,
                                          [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, const TransformComponent& c) {
    wr.String("position");
    serialize_vec3(wr, c.position);

    wr.String("rotation");
    serialize_vec3(wr, c.rotation);

    wr.String("scale");
    serialize_vec3(wr, c.scale);
  });

  serialize_component<MeshComponent>("MeshComponent",
                                     scene->registry,
                                     entity,
                                     writer,
                                     [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, MeshComponent& c) {
    wr.String("mesh_path");
    wr.String(c.mesh_base->get_path().c_str());

    wr.String("stationary");
    wr.Bool(c.stationary);

    wr.String("cast_shadows");
    wr.Bool(c.cast_shadows);
  });

  serialize_component<LightComponent>("LightComponent",
                                      scene->registry,
                                      entity,
                                      writer,
                                      [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, LightComponent& c) {
    wr.String("type");
    wr.Int(c.type);

    wr.String("color_temperature_mode");
    wr.Bool(c.color_temperature_mode);

    wr.String("temperature");
    wr.Uint(c.temperature);

    wr.String("color");
    serialize_vec3(wr, c.color);

    wr.String("intensity");
    wr.Double(c.intensity);

    wr.String("range");
    wr.Double(c.range);

    wr.String("radius");
    wr.Double(c.radius);

    wr.String("length");
    wr.Double(c.length);

    wr.String("outer_cone_angle");
    wr.Double(c.outer_cone_angle);

    wr.String("inner_cone_angle");
    wr.Double(c.inner_cone_angle);

    wr.String("cast_shadows");
    wr.Bool(c.cast_shadows);

    wr.String("shadow_map_res");
    wr.Uint(c.shadow_map_res);
  });

  serialize_component<PostProcessProbe>("PostProcessProbe",
                                        scene->registry,
                                        entity,
                                        writer,
                                        [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, PostProcessProbe& c) {
    wr.String("vignette_enabled");
    wr.Bool(c.vignette_enabled);

    wr.String("vignette_intensity");
    wr.Double(c.vignette_intensity);

    wr.String("film_grain_enabled");
    wr.Bool(c.film_grain_enabled);

    wr.String("film_grain_intensity");
    wr.Double(c.film_grain_intensity);

    wr.String("chromatic_aberration_enabled");
    wr.Bool(c.chromatic_aberration_enabled);

    wr.String("chromatic_aberration_intensity");
    wr.Double(c.chromatic_aberration_intensity);

    wr.String("sharpen_enabled");
    wr.Bool(c.sharpen_enabled);

    wr.String("sharpen_intensity");
    wr.Double(c.sharpen_intensity);
  });

  serialize_component<CameraComponent>("CameraComponent",
                                       scene->registry,
                                       entity,
                                       writer,
                                       [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, CameraComponent& c) {
    wr.String("projection");
    wr.Uint(static_cast<uint32>(c.projection));

    wr.String("fov");
    wr.Double(c.fov);

    wr.String("near");
    wr.Double(c.near_clip);

    wr.String("far");
    wr.Double(c.far_clip);

    wr.String("zoom");
    wr.Double(c.zoom);
  });

  serialize_component<RigidbodyComponent>("RigidbodyComponent",
                                          scene->registry,
                                          entity,
                                          writer,
                                          [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, RigidbodyComponent& rb) {
    wr.String("allowed_dofs");
    wr.Int(static_cast<int>(rb.allowed_dofs));

    wr.String("type");
    wr.Int(static_cast<int>(rb.type));

    wr.String("mass");
    wr.Double(rb.mass);

    wr.String("linear_drag");
    wr.Double(rb.linear_drag);

    wr.String("angular_drag");
    wr.Double(rb.angular_drag);

    wr.String("gravity_scale");
    wr.Double(rb.gravity_scale);

    wr.String("allow_sleep");
    wr.Bool(rb.allow_sleep);

    wr.String("awake");
    wr.Bool(rb.awake);

    wr.String("continuous");
    wr.Bool(rb.continuous);

    wr.String("interpolation");
    wr.Bool(rb.interpolation);

    wr.String("is_sensor");
    wr.Bool(rb.is_sensor);
  });

  serialize_component<BoxColliderComponent>("BoxColliderComponent",
                                            scene->registry,
                                            entity,
                                            writer,
                                            [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, BoxColliderComponent& bc) {
    wr.String("size");
    serialize_vec3(wr, bc.size);

    wr.String("offset");
    serialize_vec3(wr, bc.offset);

    wr.String("density");
    wr.Double(bc.density);

    wr.String("friction");
    wr.Double(bc.friction);

    wr.String("restitution");
    wr.Double(bc.restitution);
  });

  serialize_component<SphereColliderComponent>("SphereColliderComponent",
                                               scene->registry,
                                               entity,
                                               writer,
                                               [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, SphereColliderComponent& sc) {
    wr.String("radius");
    wr.Double(sc.radius);

    wr.String("offset");
    serialize_vec3(wr, sc.offset);

    wr.String("density");
    wr.Double(sc.density);

    wr.String("friction");
    wr.Double(sc.friction);

    wr.String("restitution");
    wr.Double(sc.restitution);
  });

  serialize_component<CapsuleColliderComponent>("CapsuleColliderComponent",
                                                scene->registry,
                                                entity,
                                                writer,
                                                [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, CapsuleColliderComponent& cc) {
    wr.String("height");
    wr.Double(cc.height);

    wr.String("radius");
    wr.Double(cc.radius);

    wr.String("offset");
    serialize_vec3(wr, cc.offset);

    wr.String("density");
    wr.Double(cc.density);

    wr.String("friction");
    wr.Double(cc.friction);

    wr.String("restitution");
    wr.Double(cc.restitution);
  });

  serialize_component<TaperedCapsuleColliderComponent>("TaperedCapsuleColliderComponent",
                                                       scene->registry,
                                                       entity,
                                                       writer,
                                                       [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr,
                                                          TaperedCapsuleColliderComponent& tcc) {
    wr.String("height");
    wr.Double(tcc.height);

    wr.String("top_radius");
    wr.Double(tcc.top_radius);

    wr.String("offset");
    serialize_vec3(wr, tcc.offset);

    wr.String("density");
    wr.Double(tcc.density);

    wr.String("friction");
    wr.Double(tcc.friction);

    wr.String("restitution");
    wr.Double(tcc.restitution);
  });

  serialize_component<CylinderColliderComponent>("CylinderColliderComponent",
                                                 scene->registry,
                                                 entity,
                                                 writer,
                                                 [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, CylinderColliderComponent& cc) {
    wr.String("height");
    wr.Double(cc.height);

    wr.String("radius");
    wr.Double(cc.radius);

    wr.String("offset");
    serialize_vec3(wr, cc.offset);

    wr.String("density");
    wr.Double(cc.density);

    wr.String("friction");
    wr.Double(cc.friction);

    wr.String("restitution");
    wr.Double(cc.restitution);
  });

  serialize_component<MeshColliderComponent>("MeshColliderComponent",
                                             scene->registry,
                                             entity,
                                             writer,
                                             [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, MeshColliderComponent& mc) {
    wr.String("offset");
    serialize_vec3(wr, mc.offset);

    wr.String("friction");
    wr.Double(mc.friction);

    wr.String("restitution");
    wr.Double(mc.restitution);
  });

  serialize_component<CharacterControllerComponent>("CharacterControllerComponent",
                                                    scene->registry,
                                                    entity,
                                                    writer,
                                                    [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, CharacterControllerComponent& c) {
    wr.String("character_height_standing");
    wr.Double(c.character_height_standing);

    wr.String("character_radius_standing");
    wr.Double(c.character_radius_standing);

    wr.String("character_radius_crouching");
    wr.Double(c.character_radius_crouching);

    wr.String("character_height_crouching");
    wr.Double(c.character_height_crouching);

    wr.String("control_movement_during_jump");
    wr.Bool(c.control_movement_during_jump);

    wr.String("jump_force");
    wr.Double(c.jump_force);

    wr.String("friction");
    wr.Double(c.friction);

    wr.String("collision_tolerance");
    wr.Double(c.collision_tolerance);
  });

  serialize_component<LuaScriptComponent>("LuaScriptComponent",
                                          scene->registry,
                                          entity,
                                          writer,
                                          [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, LuaScriptComponent& c) {
    wr.String("systems");
    wr.StartArray();
    for (const auto& system : c.lua_systems)
      wr.String(system->get_path().c_str());
    wr.EndArray();
  });

  serialize_component<CPPScriptComponent>("CPPScriptComponent",
                                          scene->registry,
                                          entity,
                                          writer,
                                          [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, CPPScriptComponent& c) {
    wr.String("system_hashes");
    wr.StartArray();
    for (const auto& system : c.systems) {
      const auto hash_str = std::to_string(system->hash_code);
      wr.String(hash_str.c_str());
    }
    wr.EndArray();
  });

  serialize_component<SpriteComponent>("SpriteComponent",
                                       scene->registry,
                                       entity,
                                       writer,
                                       [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, SpriteComponent& c) {
    wr.String("layer");
    wr.Uint(c.layer);

    wr.String("sort_y");
    wr.Bool(c.sort_y);

    wr.String("color");
    serialize_vec4(wr, c.material->parameters.color);

    wr.String("uv_size");
    serialize_vec2(wr, c.material->parameters.uv_size);

    wr.String("uv_offset");
    serialize_vec2(wr, c.material->parameters.uv_offset);

    const auto path = c.material->get_albedo_texture() ? c.material->get_albedo_texture()->get_path() : "";
    wr.String("texture_path");
    wr.String(path.c_str());
  });

  serialize_component<SpriteAnimationComponent>("SpriteAnimationComponent",
                                                scene->registry,
                                                entity,
                                                writer,
                                                [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, SpriteAnimationComponent& c) {
    wr.String("num_frames");
    wr.Uint(c.num_frames);

    wr.String("loop");
    wr.Bool(c.loop);

    wr.String("inverted");
    wr.Bool(c.inverted);

    wr.String("fps");
    wr.Uint(c.fps);

    wr.String("columns");
    wr.Uint(c.columns);

    wr.String("frame_size");
    serialize_vec2(wr, c.frame_size);
  });

  serialize_component<TilemapComponent>("TilemapComponent",
                                        scene->registry,
                                        entity,
                                        writer,
                                        [](rapidjson::PrettyWriter<rapidjson::StringBuffer>& wr, TilemapComponent& c) {
    wr.String("path");
    wr.String(c.path.c_str());
  });

  writer.EndObject(); // top
}

void EntitySerializer::serialize_entity_binary(Archive& archive, Scene* scene, Entity entity) {
  if (scene->registry.all_of<IDComponent>(entity)) {
    const auto& component = scene->registry.get<IDComponent>(entity);
    archive << component.uuid;
  }
  if (scene->registry.all_of<TagComponent>(entity)) {
    const auto& component = scene->registry.get<TagComponent>(entity);
    archive << component.tag;
  }
}

UUID EntitySerializer::deserialize_entity(rapidjson::Value& entity, Scene* scene, bool preserve_uuid) {
  OX_SCOPED_ZONE;
  auto& registry = scene->registry;
  entt::entity deserialized_entity = {};

  const uint64 uuid = entity["uuid"].GetUint64();
  std::string name = {};
  bool enabled = true;
  for (const auto& tc : entity["TagComponent"].GetArray()) {
    name = tc["tag"].GetString();
    enabled = tc["enabled"].GetBool();
  }

  if (preserve_uuid)
    deserialized_entity = scene->create_entity_with_uuid(uuid, name);
  else
    deserialized_entity = scene->create_entity(name);

  auto& tag_component = registry.get_or_emplace<TagComponent>(deserialized_entity);
  tag_component.tag = name;
  tag_component.enabled = enabled;

  for (const auto& rc : entity["RelationshipComponent"].GetArray()) {
    auto& [parent, children] = registry.get_or_emplace<RelationshipComponent>(deserialized_entity);
    parent = rc["parent"].GetUint64();
    for (const auto& child : rc["childs"].GetArray()) {
      children.emplace_back(child.GetUint64());
    }
  }

  for (const auto& tc : entity["TransformComponent"].GetArray()) {
    auto& transform_component = registry.get_or_emplace<TransformComponent>(deserialized_entity);
    transform_component.position = deserialize_vec3(tc["position"].GetArray());
    transform_component.rotation = deserialize_vec3(tc["rotation"].GetArray());
    transform_component.scale = deserialize_vec3(tc["scale"].GetArray());
  }

  if (entity.HasMember("MeshComponent")) {
    for (const auto& mc : entity["MeshComponent"].GetArray()) {
      const auto mesh_path = mc["mesh_path"].GetString();
      auto mesh = AssetManager::get_mesh_asset(mesh_path);
      auto& mesh_component = registry.get_or_emplace<MeshComponent>(deserialized_entity, mesh);
      mesh_component.cast_shadows = mc["cast_shadows"].GetBool();
      mesh_component.stationary = mc["stationary"].GetBool();
    }
  }

  if (entity.HasMember("LightComponent")) {
    for (const auto& lc : entity["LightComponent"].GetArray()) {
      auto& light_component = registry.emplace<LightComponent>(deserialized_entity);
      light_component.type = static_cast<LightComponent::LightType>(lc["type"].GetUint());
      light_component.color_temperature_mode = lc["color_temperature_mode"].GetBool();
      light_component.temperature = lc["temperature"].GetUint();
      light_component.color = deserialize_vec3(lc["color"].GetArray());
      light_component.intensity = lc["intensity"].GetFloat();
      light_component.range = lc["range"].GetFloat();
      light_component.radius = lc["radius"].GetFloat();
      light_component.length = lc["length"].GetFloat();
      light_component.outer_cone_angle = lc["outer_cone_angle"].GetFloat();
      light_component.inner_cone_angle = lc["inner_cone_angle"].GetFloat();
      light_component.cast_shadows = lc["cast_shadows"].GetBool();
      light_component.shadow_map_res = lc["shadow_map_res"].GetUint();
    }
  }

  if (entity.HasMember("PostProcessProbe")) {
    for (const auto& ppp : entity["PostProcessProbe"].GetArray()) {
      auto& ppp_component = registry.emplace<PostProcessProbe>(deserialized_entity);
      ppp_component.vignette_enabled = ppp["vignette_enabled"].GetBool();
      ppp_component.vignette_intensity = ppp["vignette_intensity"].GetFloat();
      ppp_component.film_grain_enabled = ppp["film_grain_enabled"].GetBool();
      ppp_component.film_grain_intensity = ppp["film_grain_intensity"].GetFloat();
      ppp_component.chromatic_aberration_enabled = ppp["chromatic_aberration_enabled"].GetBool();
      ppp_component.chromatic_aberration_intensity = ppp["chromatic_aberration_intensity"].GetFloat();
      ppp_component.sharpen_enabled = ppp["sharpen_enabled"].GetBool();
      ppp_component.sharpen_intensity = ppp["sharpen_intensity"].GetFloat();
    }
  }

  if (entity.HasMember("CameraComponent")) {
    for (const auto& cc : entity["CameraComponent"].GetArray()) {
      auto& c_component = registry.emplace<CameraComponent>(deserialized_entity);
      c_component.projection = static_cast<CameraComponent::Projection>(cc["projection"].GetUint());
      c_component.fov = cc["fov"].GetFloat();
      c_component.near_clip = cc["near"].GetFloat();
      c_component.far_clip = cc["far"].GetFloat();
      c_component.zoom = cc["zoom"].GetFloat();
    }
  }

  if (entity.HasMember("RigidbodyComponent")) {
    for (const auto& rc : entity["RigidbodyComponent"].GetArray()) {
      auto& rb_component = registry.emplace<RigidbodyComponent>(deserialized_entity);
      rb_component.allowed_dofs = static_cast<RigidbodyComponent::AllowedDOFs>(rc["allowed_dofs"].GetUint());
      rb_component.type = static_cast<RigidbodyComponent::BodyType>(rc["type"].GetUint());

      rb_component.mass = rc["mass"].GetFloat();
      rb_component.linear_drag = rc["linear_drag"].GetFloat();
      rb_component.angular_drag = rc["angular_drag"].GetFloat();
      rb_component.gravity_scale = rc["gravity_scale"].GetFloat();
      rb_component.allow_sleep = rc["allow_sleep"].GetBool();
      rb_component.awake = rc["awake"].GetBool();
      rb_component.continuous = rc["continuous"].GetBool();
      rb_component.interpolation = rc["interpolation"].GetBool();
      rb_component.is_sensor = rc["is_sensor"].GetBool();
    }
  }

  if (entity.HasMember("BoxColliderComponent")) {
    for (const auto& bc : entity["BoxColliderComponent"].GetArray()) {
      auto& bc_component = registry.emplace<BoxColliderComponent>(deserialized_entity);
      bc_component.size = deserialize_vec3(bc["size"].GetArray());
      bc_component.offset = deserialize_vec3(bc["offset"].GetArray());

      bc_component.density = bc["density"].GetFloat();
      bc_component.friction = bc["friction"].GetFloat();
      bc_component.restitution = bc["restitution"].GetFloat();
    }
  }

  if (entity.HasMember("SphereColliderComponent")) {
    for (const auto& scc : entity["SphereColliderComponent"].GetArray()) {
      auto& scc_component = registry.emplace<SphereColliderComponent>(deserialized_entity);
      scc_component.offset = deserialize_vec3(scc["offset"].GetArray());
      scc_component.radius = scc["radius"].GetFloat();
      scc_component.density = scc["density"].GetFloat();
      scc_component.friction = scc["friction"].GetFloat();
      scc_component.restitution = scc["restitution"].GetFloat();
    }
  }

  if (entity.HasMember("CapsuleColliderComponent")) {
    for (const auto& ccc : entity["CapsuleColliderComponent"].GetArray()) {
      auto& cc_component = registry.emplace<CapsuleColliderComponent>(deserialized_entity);
      cc_component.offset = deserialize_vec3(ccc["offset"].GetArray());
      cc_component.height = ccc["height"].GetFloat();
      cc_component.radius = ccc["radius"].GetFloat();
      cc_component.density = ccc["density"].GetFloat();
      cc_component.friction = ccc["friction"].GetFloat();
      cc_component.restitution = ccc["restitution"].GetFloat();
    }
  }

  if (entity.HasMember("TaperedCapsuleColliderComponent")) {
    for (const auto& tcc : entity["TaperedCapsuleColliderComponent"].GetArray()) {
      auto& tcc_component = registry.emplace<TaperedCapsuleColliderComponent>(deserialized_entity);
      tcc_component.offset = deserialize_vec3(tcc["offset"].GetArray());
      tcc_component.height = tcc["height"].GetFloat();
      tcc_component.top_radius = tcc["top_radius"].GetFloat();
      tcc_component.bottom_radius = tcc["bottom_radius"].GetFloat();
      tcc_component.density = tcc["density"].GetFloat();
      tcc_component.friction = tcc["friction"].GetFloat();
      tcc_component.restitution = tcc["restitution"].GetFloat();
    }
  }

  if (entity.HasMember("CylinderColliderComponent")) {
    for (const auto& ccc : entity["CylinderColliderComponent"].GetArray()) {
      auto& ccc_component = registry.emplace<CylinderColliderComponent>(deserialized_entity);
      ccc_component.offset = deserialize_vec3(ccc["offset"].GetArray());

      ccc_component.height = ccc["height"].GetFloat();
      ccc_component.radius = ccc["radius"].GetFloat();
      ccc_component.density = ccc["density"].GetFloat();
      ccc_component.friction = ccc["friction"].GetFloat();
      ccc_component.restitution = ccc["restitution"].GetFloat();
    }
  }

  if (entity.HasMember("MeshColliderComponent")) {
    for (const auto& mcc : entity["MeshColliderComponent"].GetArray()) {
      auto& mcc_component = registry.emplace<MeshColliderComponent>(deserialized_entity);
      mcc_component.offset = deserialize_vec3(mcc["offset"].GetArray());
      mcc_component.friction = mcc["friction"].GetFloat();
      mcc_component.restitution = mcc["restitution"].GetFloat();
    }
  }

  if (entity.HasMember("CharacterControllerComponent")) {
    for (const auto& ccc : entity["CharacterControllerComponent"].GetArray()) {
      auto& ccc_component = registry.emplace<CharacterControllerComponent>(deserialized_entity);
      ccc_component.character_height_standing = ccc["character_height_standing"].GetFloat();
      ccc_component.character_radius_standing = ccc["character_radius_standing"].GetFloat();
      ccc_component.character_height_crouching = ccc["character_height_crouching"].GetFloat();
      ccc_component.character_radius_crouching = ccc["character_radius_crouching"].GetFloat();
      ccc_component.control_movement_during_jump = ccc["control_movement_during_jump"].GetBool();
      ccc_component.jump_force = ccc["jump_force"].GetFloat();
      ccc_component.friction = ccc["friction"].GetFloat();
      ccc_component.collision_tolerance = ccc["collision_tolerance"].GetFloat();
    }
  }

  if (entity.HasMember("LuaScriptComponent")) {
    for (const auto& lcc : entity["LuaScriptComponent"].GetArray()) {
      auto& lcc_component = registry.emplace<LuaScriptComponent>(deserialized_entity);
      for (const auto& path : lcc["systems"].GetArray()) {
        auto ab = App::get_system<VFS>()->resolve_physical_dir(path.GetString());
        lcc_component.lua_systems.emplace_back(create_shared<LuaSystem>(ab));
      }
    }
  }

  if (entity.HasMember("CPPScriptComponent")) {
    for (const auto& cpp : entity["CPPScriptComponent"].GetArray()) {
      auto& cpp_component = registry.emplace<CPPScriptComponent>(deserialized_entity);
      for (const auto& hash : cpp["system_hashes"].GetArray()) {
        auto* system_manager = App::get_system<SystemManager>();
        cpp_component.systems.emplace_back(system_manager->get_system(std::stoull(hash.GetString())));
      }
    }
  }

  if (entity.HasMember("SpriteComponent")) {
    for (const auto& sc : entity["SpriteComponent"].GetArray()) {
      auto& sc_component = registry.emplace<SpriteComponent>(deserialized_entity);
      sc_component.layer = sc["layer"].GetInt();
      sc_component.sort_y = sc["sort_y"].GetInt();
      sc_component.material = create_shared<SpriteMaterial>();
      sc_component.material->parameters.color = deserialize_vec4(sc["color"].GetArray());
      sc_component.material->parameters.uv_offset = deserialize_vec3(sc["offset"].GetArray());
      sc_component.material->parameters.uv_size = deserialize_vec3(sc["size"].GetArray());

      const auto path = std::string(sc["texture_path"].GetString());
      if (!path.empty())
        sc_component.material->set_albedo_texture(AssetManager::get_texture_asset({.path = path}));
    }
  }

  if (entity.HasMember("SpriteAnimationComponent")) {
    for (const auto& sac : entity["SpriteAnimationComponent"].GetArray()) {
      auto& sa_component = registry.emplace<SpriteAnimationComponent>(deserialized_entity);
      sa_component.num_frames = sac["num_frames"].GetUint();
      sa_component.loop = sac["loop"].GetBool();
      sa_component.inverted = sac["inverted"].GetBool();
      sa_component.fps = sac["fps"].GetUint();
      sa_component.columns = sac["columns"].GetUint();
      sa_component.frame_size = deserialize_vec2(sac["frame_size"].GetArray());
    }
  }

  if (entity.HasMember("TilemapComponent")) {
    for (const auto& tc : entity["TilemapComponent"].GetArray()) {
      auto& t_component = registry.emplace<TilemapComponent>(deserialized_entity);
      const auto path = App::get_system<VFS>()->resolve_physical_dir(tc["path"].GetString());
      t_component.load(path);
    }
  }

  return eutil::get_uuid(registry, deserialized_entity);
}

void EntitySerializer::serialize_entity_as_prefab(const char* filepath, Entity entity) {
#if 0 // TODO:
  if (scene->registry.all_of<PrefabComponent>(entity)) {
    OX_CORE_ERROR("Entity already has a prefab component!");
    return;
  }

  ryml::Tree tree;

  ryml::NodeRef node_root = tree.rootref();
  node_root |= ryml::MAP;

  node_root["Prefab"] << entity.add_component_internal<PrefabComponent>().id;

  ryml::NodeRef entities_node = node_root["Entities"];
  entities_node |= ryml::SEQ;

  std::vector<Entity> entities;
  entities.push_back(entity);
  Entity::get_all_children(entity, entities);

  for (const auto& child : entities) {
    if (child)
      serialize_entity(entity.get_scene(), entities_node, child);
  }

  std::stringstream ss;
  ss << tree;
  std::ofstream filestream(filepath);
  filestream << ss.str();
#endif
}

Entity EntitySerializer::deserialize_entity_as_prefab(const char* filepath, Scene* scene) {
#if 0 // TODO:
  auto content = FileSystem::read_file(filepath);
  if (content.empty()) {
    OX_CORE_ERROR("Couldn't read prefab file: {0}", filepath);
  }

  const ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(content));

  if (tree.empty()) {
    OX_CORE_ERROR("Couldn't parse the prefab file {0}", FileSystem::get_file_name(filepath));
  }

  const ryml::ConstNodeRef root = tree.rootref();

  if (!root.has_child("Prefab")) {
    OX_CORE_ERROR("Prefab file doesn't contain a prefab{0}", FileSystem::get_file_name(filepath));
    return {};
  }

  const UUID prefab_id = (uint64_t)root["Prefab"].val().data();

  if (!prefab_id) {
    OX_CORE_ERROR("Invalid prefab ID {0}", FileSystem::get_file_name(filepath));
    return {};
  }

  if (root.has_child("Entities")) {
    const ryml::ConstNodeRef entities_node = root["Entities"];

    Entity root_entity = {};
    std::unordered_map<UUID, UUID> old_new_id_map;
    for (const auto& entity : entities_node) {
      uint64_t old_uuid;
      entity["Entity"] >> old_uuid;
      UUID new_uuid = deserialize_entity(entity, scene, false);
      old_new_id_map.emplace(old_uuid, new_uuid);

      if (!root_entity)
        root_entity = scene->get_entity_by_uuid(new_uuid);
    }

    root_entity.add_component_internal<PrefabComponent>().id = prefab_id;

    // Fix parent/children UUIDs
    for (const auto& [_, newId] : old_new_id_map) {
      auto& relationship_component = scene->get_entity_by_uuid(newId).get_relationship();
      UUID parent = relationship_component.parent;
      if (parent)
        relationship_component.parent = old_new_id_map.at(parent);

      auto& children = relationship_component.children;
      for (auto& id : children)
        id = old_new_id_map.at(id);
    }

    return root_entity;
  }

#endif
  OX_LOG_ERROR("There are no entities in the prefab to deserialize! {0}", fs::get_file_name(filepath));
  return {};
}
} // namespace ox
