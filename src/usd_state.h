#ifndef USD_STATE_H
#define USD_STATE_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace godot {

class UsdState : public Resource {
    GDCLASS(UsdState, Resource);

private:
    // Export state
    String _copyright;
    float _bake_fps;
    
    // USD-specific state
    UsdStageRefPtr _stage;

protected:
    static void _bind_methods();

public:
    UsdState();
    
    // Getters and setters
    void set_copyright(const String &p_copyright);
    String get_copyright() const;
    
    void set_bake_fps(float p_fps);
    float get_bake_fps() const;
    
    // Stage management
    void set_stage(UsdStageRefPtr p_stage);
    UsdStageRefPtr get_stage() const;
};

}

#endif // USD_STATE_H
