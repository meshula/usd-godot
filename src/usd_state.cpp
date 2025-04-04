#include "usd_state.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void UsdState::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_copyright", "copyright"), &UsdState::set_copyright);
    ClassDB::bind_method(D_METHOD("get_copyright"), &UsdState::get_copyright);
    
    ClassDB::bind_method(D_METHOD("set_bake_fps", "fps"), &UsdState::set_bake_fps);
    ClassDB::bind_method(D_METHOD("get_bake_fps"), &UsdState::get_bake_fps);
    
    // Add properties to the inspector
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_copyright", "get_copyright");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_fps", PROPERTY_HINT_RANGE, "1,120,0.1"), "set_bake_fps", "get_bake_fps");
}

UsdState::UsdState() {
    // Set default values
    _copyright = "";
    _bake_fps = 30.0f;
}

void UsdState::set_copyright(const String &p_copyright) {
    _copyright = p_copyright;
}

String UsdState::get_copyright() const {
    return _copyright;
}

void UsdState::set_bake_fps(float p_fps) {
    _bake_fps = p_fps;
}

float UsdState::get_bake_fps() const {
    return _bake_fps;
}

}
