#ifndef USD_SCENE_LOADER_H
#define USD_SCENE_LOADER_H

#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

/**
 * UsdResourceFormatLoader - registers .usd/.usda/.usdc/.usdz as loadable scene assets.
 *
 * A `load()` on a USD path returns a baked PackedScene, produced by UsdSceneBuilder. This is the
 * asset-pipeline entry point transplanted (in behavior) from godot-usd: it is what lets USD files
 * behave like glTF/FBX in a Godot project, and it is the seam the fidelity corpus exercises via
 * `load()` / `ResourceLoader.load(path, "PackedScene", ...)`.
 */
class UsdResourceFormatLoader : public ResourceFormatLoader {
    GDCLASS(UsdResourceFormatLoader, ResourceFormatLoader);

protected:
    static void _bind_methods() {}

public:
    PackedStringArray _get_recognized_extensions() const override;
    bool _handles_type(const StringName &p_type) const override;
    String _get_resource_type(const String &p_path) const override;
    Variant _load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const override;
};

} // namespace godot

#endif // USD_SCENE_LOADER_H
