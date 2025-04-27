#include "EditorTheme.hpp"

#include <icons/IconsMaterialDesignIcons.h>

#include "Core/App.hpp"
#include "Core/VFS.hpp"
#include "Scene/Components.hpp"
#include "UI/ImGuiLayer.hpp"

namespace ox {

void EditorTheme::init(this EditorTheme& self) {
  const auto* app = App::get();

  auto* vfs = App::get_system<VFS>(EngineSystems::VFS);
  auto regular_font_path = vfs->resolve_physical_dir(VFS::APP_DIR, "Fonts/FiraSans-Regular.ttf");
  auto bold_font_path = vfs->resolve_physical_dir(VFS::APP_DIR, "Fonts/FiraSans-Bold.ttf");

  auto* imguilayer = app->get_imgui_layer();

  ImFontConfig fonts_config;
  fonts_config.MergeMode = false;
  fonts_config.PixelSnapH = true;
  fonts_config.OversampleH = fonts_config.OversampleV = 3;
  fonts_config.GlyphMinAdvanceX = 4.0f;

  fonts_config.SizePixels = 16.0f;
  self.regular_font = imguilayer->load_font(regular_font_path, fonts_config);
  imguilayer->add_icon_font(fonts_config.SizePixels);

  fonts_config.SizePixels = 12.0f;
  self.small_font = imguilayer->load_font(regular_font_path, fonts_config);
  imguilayer->add_icon_font(fonts_config.SizePixels);

  fonts_config.SizePixels = 16.0f;
  self.bold_font = imguilayer->load_font(bold_font_path, fonts_config);
  imguilayer->add_icon_font(fonts_config.SizePixels);

  imguilayer->build_fonts();

  self.component_icon_map[typeid(LightComponent).hash_code()] = ICON_MDI_LIGHTBULB;
  self.component_icon_map[typeid(CameraComponent).hash_code()] = ICON_MDI_CAMERA;
  self.component_icon_map[typeid(AudioSourceComponent).hash_code()] = ICON_MDI_VOLUME_HIGH;
  self.component_icon_map[typeid(TransformComponent).hash_code()] = ICON_MDI_VECTOR_LINE;
  self.component_icon_map[typeid(MeshComponent).hash_code()] = ICON_MDI_VECTOR_SQUARE;
  self.component_icon_map[typeid(LuaScriptComponent).hash_code()] = ICON_MDI_LANGUAGE_LUA;
  self.component_icon_map[typeid(CPPScriptComponent).hash_code()] = ICON_MDI_LANGUAGE_CPP;
  self.component_icon_map[typeid(PostProcessProbe).hash_code()] = ICON_MDI_SPRAY;
  self.component_icon_map[typeid(AudioListenerComponent).hash_code()] = ICON_MDI_CIRCLE_SLICE_8;
  self.component_icon_map[typeid(RigidbodyComponent).hash_code()] = ICON_MDI_SOCCER;
  self.component_icon_map[typeid(BoxColliderComponent).hash_code()] = ICON_MDI_CHECKBOX_BLANK_OUTLINE;
  self.component_icon_map[typeid(SphereColliderComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  self.component_icon_map[typeid(CapsuleColliderComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  self.component_icon_map[typeid(TaperedCapsuleColliderComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  self.component_icon_map[typeid(CylinderColliderComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  self.component_icon_map[typeid(MeshColliderComponent).hash_code()] = ICON_MDI_CHECKBOX_BLANK_OUTLINE;
  self.component_icon_map[typeid(CharacterControllerComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  self.component_icon_map[typeid(ParticleSystemComponent).hash_code()] = ICON_MDI_LAMP;
  self.component_icon_map[typeid(SpriteComponent).hash_code()] = ICON_MDI_SQUARE_OUTLINE;
  self.component_icon_map[typeid(SpriteAnimationComponent).hash_code()] = ICON_MDI_SHAPE_SQUARE_PLUS;
  self.component_icon_map[typeid(TilemapComponent).hash_code()] = ICON_MDI_SHAPE_POLYGON_PLUS;
}
} // namespace ox
