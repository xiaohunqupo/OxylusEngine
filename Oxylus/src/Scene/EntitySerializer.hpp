#pragma once

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

namespace ox {
class Archive;
class Scene;

class EntitySerializer {
public:
  // static void serialize_entity(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, Scene* scene, Entity entity);
  // static UUID deserialize_entity(rapidjson::Value& entity, Scene* scene, bool preserve_uuid);
};
}
