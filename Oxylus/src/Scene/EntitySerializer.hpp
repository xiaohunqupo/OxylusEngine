#pragma once

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include "Core/UUID.hpp"
#include "Entity.hpp"
#include "Utils/Toml.hpp"

namespace ox {
class Archive;
class Scene;

class EntitySerializer {
public:
  static void serialize_entity(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, Scene* scene, Entity entity);
  static void serialize_entity_binary(Archive& archive, Scene* scene, Entity entity);
  static UUID deserialize_entity(rapidjson::Value& entity, Scene* scene, bool preserve_uuid);
  static void serialize_entity_as_prefab(const char* filepath, Entity entity);
  static Entity deserialize_entity_as_prefab(const char* filepath, Scene* scene);
};
}
