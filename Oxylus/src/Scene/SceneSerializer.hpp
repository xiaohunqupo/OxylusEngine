#pragma once

#include "Scene.hpp"

namespace ox {
class SceneSerializer {
public:
  SceneSerializer(const Shared<Scene>& scene);

  void serialize(const std::string& filePath) const;

  bool deserialize(const std::string& filePath) const;
private:
  Shared<Scene> _scene = nullptr;
};
}
