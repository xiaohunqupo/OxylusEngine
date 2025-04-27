#include "SceneSerializer.hpp"

#include "EntitySerializer.hpp"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>

#include "Core/FileSystem.hpp"

namespace ox {
SceneSerializer::SceneSerializer(const Shared<Scene>& scene) : _scene(scene) {}

void SceneSerializer::serialize(const std::string& filePath) const {
  rapidjson::StringBuffer sb;

  rapidjson::PrettyWriter writer(sb);
  writer.StartObject(); // root

  writer.String("name");
  writer.String(_scene->scene_name.c_str());

  writer.String("entities");
  writer.StartArray(); // entities
  // for (const auto [e] : _scene->registry.storage<entt::entity>().each()) {
    // EntitySerializer::serialize_entity(writer, _scene.get(), e);
  // }
  writer.EndArray(); // entities

  writer.EndObject(); // root

  std::ofstream filestream(filePath);
  filestream << sb.GetString();

  OX_LOG_INFO("Saved scene {0}.", _scene->scene_name);
}

bool SceneSerializer::deserialize(const std::string& file_path) const {
  const auto content = fs::read_file(file_path);
  if (content.empty()) {
    OX_ASSERT(!content.empty(), fmt::format("Couldn't read scene file: {0}", file_path).c_str());
  }

  rapidjson::Document doc;
  doc.Parse(content.data());

  rapidjson::ParseResult parse_result = doc.Parse(content.c_str());

  if (doc.HasParseError())
    OX_LOG_ERROR("Json parser error for: {0} {1}", file_path, rapidjson::GetParseError_En(parse_result.Code()));

  _scene->scene_name = doc["name"].GetString();

  auto entities_array = doc["entities"].GetArray();

  for (auto& entity : entities_array) {
    // EntitySerializer::deserialize_entity(entity.GetObject(), _scene.get(), true);
  }

  OX_LOG_INFO("Scene loaded : {0}", fs::get_file_name(_scene->scene_name));
  return true;
}
} // namespace ox
