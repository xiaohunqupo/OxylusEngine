#include "Scene/ECSModule/Core.hpp"

#include "Core/App.hpp"
#include "Core/UUID.hpp"
#include "Scene/ECSModule/ComponentWrapper.hpp"
#include "Scene/Scene.hpp"
#include "Scripting/LuaManager.hpp"

namespace ox {
Core::Core(flecs::world& world) {
  ZoneScoped;

  world
      .component<glm::vec2>("glm::vec2") //
      .member<f32>("x")
      .member<f32>("y");

  world
      .component<glm::ivec2>("glm::ivec2") //
      .member<i32>("x")
      .member<i32>("y");

  world
      .component<glm::vec3>("glm::vec3") //
      .member<f32>("x")
      .member<f32>("y")
      .member<f32>("z");

  world
      .component<glm::vec4>("glm::vec4") //
      .member<f32>("x")
      .member<f32>("y")
      .member<f32>("z")
      .member<f32>("w");

  world
      .component<glm::mat3>("glm::mat3") //
      .member<glm::vec3>("col0")
      .member<glm::vec3>("col1")
      .member<glm::vec3>("col2");

  world
      .component<glm::mat4>("glm::mat4") //
      .member<glm::vec4>("col0")
      .member<glm::vec4>("col1")
      .member<glm::vec4>("col2")
      .member<glm::vec4>("col3");

  world
      .component<glm::quat>("glm::quat") //
      .member<f32>("x")
      .member<f32>("y")
      .member<f32>("z")
      .member<f32>("w");

  world.component<std::string>("std::string")
      .opaque(flecs::String)
      .serialize([](const flecs::serializer* s, const std::string* data) {
        const char* str = data->c_str();
        return s->value(flecs::String, &str);
      })
      .assign_string([](std::string* data, const char* value) { *data = value; });

  world.component<UUID>("ox::UUID")
      .opaque(flecs::String)
      .serialize([](const flecs::serializer* s, const UUID* data) {
        auto str = data->str();
        return s->value(flecs::String, str.c_str());
      })
      .assign_string([](UUID* data, const char* value) { *data = UUID::from_string(std::string_view(value)).value(); });

#ifdef OX_LUA_BINDINGS
  const auto state = App::get_system<LuaManager>(EngineSystems::LuaManager)->get_state();

  auto component_table = state->create_named_table("Core");

  auto flecs_table = state->create_named_table("flecs");

  // --- entity_t ---
  auto id_type = flecs_table.new_usertype<flecs::entity_t>("entity_t");

  // --- world ---
  auto world_type = flecs_table.new_usertype<flecs::world>(
      "world",

      "entity",
      [](flecs::world& w, const std::string& name) -> flecs::entity { return w.entity(name.c_str()); });

  // --- entity ---
  auto entity_type = state->new_usertype<flecs::entity>(
      "entity",

      "id",
      [](flecs::entity* e) -> flecs::entity_t { return e->id(); },

      "add",
      [](flecs::entity* e, sol::table component_table, sol::optional<sol::table> values = {}) {
        auto component = component_table.get<flecs::entity>("component_id");
        e->add(component);

        auto* ptr = e->try_get_mut(component);
        if (!ptr)
          return e;

        if (values) {
          values->for_each([&](sol::object key, sol::object value) {
            std::string field_name = key.as<std::string>();

            flecs::cursor cur = e->world().cursor(component, ptr);
            cur.push();
            cur.member(field_name.c_str());

            if (value.is<f64>())
              cur.set_float(value.as<f64>());
            else if (value.is<bool>())
              cur.set_float(value.as<bool>());
            else if (value.is<std::string>())
              cur.set_string(value.as<std::string>().c_str());

            cur.pop();
          });
        } else {
          component_table.get<sol::table>("defaults").for_each([&](sol::object key, sol::object value) {
            std::string field_name = key.as<std::string>();
            flecs::cursor cur = e->world().cursor(component, ptr);
            cur.push();
            cur.member(field_name.c_str());

            if (value.is<f64>())
              cur.set_float(value.as<f64>());
            else if (value.is<bool>())
              cur.set_float(value.as<bool>());
            else if (value.is<std::string>())
              cur.set_string(value.as<std::string>().c_str());

            cur.pop();
          });
        }

        return e;
      },

      "has",
      [](flecs::entity* e, sol::table component_table) -> bool {
        auto component = component_table.get<flecs::entity>("component_id");
        return e->has(component);
      },

      "get",
      [state](flecs::entity* e, sol::table component_table) -> sol::optional<sol::table> {
        auto comp = component_table.get<flecs::entity>("component_id");
        if (!e->has(comp))
          return sol::nullopt;

        void* ptr = e->try_get_mut(comp);
        if (!ptr)
          return sol::nullopt;

        sol::table result = state->create_table();

        ECS::ComponentWrapper component_wrapped(*e, comp);

        component_wrapped.for_each([&](usize&, std::string_view member_name, ECS::ComponentWrapper::Member& member) {
          std::visit(ox::match{
                         [](const auto&) {},
                         [&](bool* v) { result[member_name] = *v; },
                         [&](u8* v) { result[member_name] = *v; },
                         [&](u16* v) { result[member_name] = *v; },
                         [&](u32* v) { result[member_name] = *v; },
                         [&](u64* v) { result[member_name] = *v; },
                         [&](i8* v) { result[member_name] = *v; },
                         [&](i16* v) { result[member_name] = *v; },
                         [&](i32* v) { result[member_name] = *v; },
                         [&](i64* v) { result[member_name] = *v; },
                         [&](f32* v) { result[member_name] = *v; },
                         [&](f64* v) { result[member_name] = *v; },
                         [&](std::string* v) { result[member_name] = *v; },
                         [&](glm::vec2* v) { result[member_name] = *v; },
                         [&](glm::vec3* v) { result[member_name] = *v; },
                         [&](glm::vec4* v) { result[member_name] = *v; },
                         [&](glm::mat4* v) { result[member_name] = *v; },
                         [&](UUID* v) { result[member_name] = *v; },
                     },
                     member);
        });

        return result;
      },

      // TODO:
      "set",
      [](flecs::entity* e, sol::table component_data) { return e; });

  // --- define ---
  auto define_table = state->create_named_table("Component");
  define_table["define"] = [state](Scene* scene, const std::string& name, sol::table properties) -> sol::table {
    scene->defer_function([state, properties, name](Scene* scene) {
      auto component = scene->world.component(name.c_str());

      sol::table defaults = state->create_table();

      properties.for_each([&](sol::object key, sol::object value) {
        std::string field_name = key.as<std::string>();

        // explicit types
        if (value.is<sol::table>()) {
          sol::table field_def = value.as<sol::table>();
          if (field_def["type"]) {
            std::string type = field_def["type"];
            sol::object default_val = field_def["default"];

            if (type == "f32") {
              component.member<f32>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            } else if (type == "f64") {
              component.member<f64>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            } else if (type == "i8") {
              component.member<i8>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            } else if (type == "i16") {
              component.member<i16>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            } else if (type == "i32") {
              component.member<i32>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            } else if (type == "i64") {
              component.member<i64>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            } else if (type == "u8") {
              component.member<u8>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            } else if (type == "u16") {
              component.member<u16>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            } else if (type == "u32") {
              component.member<u32>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            } else if (type == "u64") {
              component.member<u64>(field_name.c_str());
              defaults[field_name] = value.as<f64>();
            }
          }
        }

        // default types
        if (value.is<f64>()) {
          component.member<f64>(field_name.c_str());
          defaults[field_name] = value.as<f64>();
        } else if (value.is<bool>()) {
          component.member<bool>(field_name.c_str());
          defaults[field_name] = value.as<bool>();
        } else if (value.is<std::string>()) {
          component.member<std::string>(field_name.c_str());
          defaults[field_name] = value.as<std::string>();
        }
      });

      if (!scene->component_db.is_component_known(component))
        scene->component_db.components.emplace_back(component);

      (*state)[name]["defaults"] = defaults;
    });

    auto component = scene->world.component(name.c_str());

    sol::table component_table = state->create_table();
    component_table["component_id"] = flecs::entity(component);

    (*state)[name] = component_table;
    return component_table;
  };
#endif

  // clang-format off
#undef ECS_EXPORT_TYPES
#define ECS_REFLECT_TYPES
#include "Scene/ECSModule/Reflect.hpp"
#include "Scene/Components.hpp"
#undef ECS_REFLECT_TYPES
  // clang-format on
}
} // namespace ox
