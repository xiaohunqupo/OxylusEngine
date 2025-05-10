#pragma once

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>

inline void serialize_vec2(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                           const glm::vec2& vec) {
  writer.StartArray();
  writer.Double(vec.x);
  writer.Double(vec.y);
  writer.EndArray();
}

inline void serialize_vec3(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                           const glm::vec3& vec) {
  writer.StartArray();
  writer.Double(vec.x);
  writer.Double(vec.y);
  writer.Double(vec.z);
  writer.EndArray();
}

inline void serialize_vec4(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                           const glm::vec4& vec) {
  writer.StartArray();
  writer.Double(vec.x);
  writer.Double(vec.y);
  writer.Double(vec.z);
  writer.Double(vec.w);
  writer.EndArray();
}

inline void deserialize_vec2(const rapidjson::GenericArray<true,
                                                           rapidjson::GenericValue<rapidjson::UTF8<>>>& array,
                             glm::vec2* v) {
  v->x = static_cast<float>(array[0].GetDouble());
  v->y = static_cast<float>(array[1].GetDouble());
}

inline void deserialize_vec3(const rapidjson::GenericArray<true,
                                                           rapidjson::GenericValue<rapidjson::UTF8<>>>& array,
                             glm::vec3* v) {
  v->x = static_cast<float>(array[0].GetDouble());
  v->y = static_cast<float>(array[1].GetDouble());
  v->z = static_cast<float>(array[2].GetDouble());
}

inline void deserialize_vec4(const rapidjson::GenericArray<true,
                                                           rapidjson::GenericValue<rapidjson::UTF8<>>>& array,
                             glm::vec4* v) {
  v->x = static_cast<float>(array[0].GetDouble());
  v->y = static_cast<float>(array[1].GetDouble());
  v->z = static_cast<float>(array[2].GetDouble());
  v->w = static_cast<float>(array[3].GetDouble());
}
