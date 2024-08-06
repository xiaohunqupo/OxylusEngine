#pragma once
#include <map>

#include "Core/ESystem.hpp"
#include "PhysicsInterfaces.hpp"

#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/Collision/CollisionCollectorImpl.h"
#include "Jolt/Physics/PhysicsSystem.h"

namespace ox {
class RayCast;

class Physics : public ESystem {
public:
  using EntityLayer = uint16_t;

  struct EntityLayerData {
    std::string name = "Layer";
    EntityLayer flags = 0xFFFF;
    uint8_t index = 1;
  };

  std::map<EntityLayer, EntityLayerData> layer_collision_mask = {
    {BIT(0), {"Static", static_cast<uint16_t>(0xFFFF), 0}},
    {BIT(1), {"Default", static_cast<uint16_t>(0xFFFF), 1}},
    {BIT(2), {"Player", static_cast<uint16_t>(0xFFFF), 2}},
    {BIT(3), {"Sensor", static_cast<uint16_t>(0xFFFF), 3}},
  };

  static constexpr uint32_t MAX_BODIES = 1024;
  static constexpr uint32_t MAX_BODY_PAIRS = 1024;
  static constexpr uint32_t MAX_CONTACT_CONSTRAINS = 1024;
  BPLayerInterfaceImpl layer_interface;
  ObjectVsBroadPhaseLayerFilterImpl object_vs_broad_phase_layer_filter_interface;
  ObjectLayerPairFilterImpl object_layer_pair_filter_interface;

  void init() override;
  void deinit() override;
  void set_instance();
  void step(float physicsTs);

  static JPH::PhysicsSystem* get_physics_system();
  static JPH::BodyInterface& get_body_interface();
  static const JPH::BroadPhaseQuery& get_broad_phase();

  static JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> cast_ray(const RayCast& ray_cast);

private:
  static Physics* _instance;

  JPH::PhysicsSystem* physics_system;
  JPH::TempAllocatorImpl* temp_allocator;
  JPH::JobSystemThreadPool* job_system;
};
} // namespace ox
