#ifndef USD_SAVER_H
#define USD_SAVER_H

#include <godot_cpp/classes/resource_format_saver.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace godot {

// Phase 4: writes a PackedScene back out to USD. This first increment handles the "authored scene"
// path (a Godot-built tree -> fresh USD layer). Source-preserving/static-merge + composition/variant
// preservation are later increments (see USD_GODOT_INTEGRATION_PLAN.md).
class UsdResourceFormatSaver : public ResourceFormatSaver {
    GDCLASS(UsdResourceFormatSaver, ResourceFormatSaver);

protected:
    static void _bind_methods() {}

public:
    Error _save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) override;
    bool _recognize(const Ref<Resource> &p_resource) const override;
    PackedStringArray _get_recognized_extensions(const Ref<Resource> &p_resource) const override;
    bool _recognize_path(const Ref<Resource> &p_resource, const String &p_path) const override;
};

} // namespace godot

#endif // USD_SAVER_H
