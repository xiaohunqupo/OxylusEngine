#pragma once
#include <glm/gtx/norm.hpp>

#include "Core/Types.hpp"

namespace ox {
class RayCast;
}
namespace ox {
struct Plane;
struct Frustum;

enum Intersection { Outside = 0, Intersects = 1, Inside = 2 };

struct AABB {
  Vec3 min = {};
  Vec3 max = {};

  AABB() = default;
  ~AABB() = default;
  AABB(const AABB& other) = default;
  AABB(const Vec3 min, const Vec3 max) : min(min), max(max) {}

  Vec3 get_center() const { return (max + min) * 0.5f; }
  Vec3 get_extents() const { return max - min; }
  Vec3 get_size() const { return get_extents(); }

  void translate(const Vec3& translation);
  void scale(const Vec3& scale);
  void rotate(const Mat3& rotation);
  void transform(const Mat4& transform);
  AABB get_transformed(const Mat4& transform) const;

  void merge(const AABB& other);

  bool is_on_or_forward_plane(const Plane& plane) const;
  bool is_on_frustum(const Frustum& frustum) const;
  bool intersects(const Vec3& point) const;
  Intersection intersects(const AABB& box) const;
  bool intersects_fast(const AABB& box) const;
  bool intersects(const RayCast& ray) const;
};

struct Sphere {
  Vec3 center = {};
  float radius = {};

  Sphere() = default;
  ~Sphere() = default;
  Sphere(const Sphere& other) = default;
  Sphere(const Vec3 center, const float radius) : center(center), radius(radius) {}

  bool intersects(const AABB& b) const;
  bool intersects(const Sphere& b) const;
  bool intersects(const Sphere& b, float& dist) const;
  bool intersects(const Sphere& b, float& dist, Vec3& direction) const;
  bool intersects(const RayCast& ray) const;
  bool intersects(const RayCast& ray, float& dist) const;
  bool intersects(const RayCast& ray, float& dist, Vec3& direction) const;
};
} // namespace ox
