#ifndef USD_STAGE_PROXY_H
#define USD_STAGE_PROXY_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace godot {

class UsdPrimProxy;

/**
 * UsdStageProxy - Ergonomic GDScript wrapper for USD Stage operations.
 *
 * This class provides a GDScript-friendly interface to USD stages,
 * abstracting away C++ details and providing Godot-native types.
 *
 * Example GDScript usage:
 *   var stage = UsdStageProxy.new()
 *   stage.open("res://assets/model.usda")
 *   var prim = stage.get_prim_at_path("/World/MyMesh")
 *   print(prim.get_type_name())  # "Mesh"
 *   stage.save()
 */
class UsdStageProxy : public RefCounted {
    GDCLASS(UsdStageProxy, RefCounted);

private:
    UsdStageRefPtr _stage;
    String _file_path;
    bool _is_modified;
    double _current_time_code;

protected:
    static void _bind_methods();

public:
    UsdStageProxy();
    ~UsdStageProxy();

    // -------------------------------------------------------------------------
    // Stage Lifecycle
    // -------------------------------------------------------------------------

    /// Open an existing USD file. Returns OK on success, error code otherwise.
    /// Supports .usd, .usda, .usdc, .usdz formats.
    Error open(const String &p_path);

    /// Create a new empty USD stage at the given path.
    /// The file is not written until save() is called.
    Error create_new(const String &p_path);

    /// Save the stage to disk. If path is empty, saves to the original location.
    Error save(const String &p_path = String());

    /// Export to a different path without changing the stage's root layer.
    Error export_to(const String &p_path, bool p_binary = true);

    /// Close the stage and release resources.
    void close();

    /// Reload the stage from disk, discarding any unsaved changes.
    Error reload();

    /// Returns true if a stage is currently open.
    bool is_open() const;

    /// Returns true if the stage has unsaved modifications.
    bool is_modified() const;

    // -------------------------------------------------------------------------
    // Prim Access
    // -------------------------------------------------------------------------

    /// Get the default prim of the stage (the root of the scene).
    Ref<UsdPrimProxy> get_default_prim() const;

    /// Set the default prim by path.
    Error set_default_prim(const String &p_prim_path);

    /// Get a prim at the specified path. Returns null if not found.
    /// Path should be absolute (starting with "/").
    Ref<UsdPrimProxy> get_prim_at_path(const String &p_path) const;

    /// Check if a prim exists at the given path.
    bool has_prim_at_path(const String &p_path) const;

    /// Traverse all prims in the stage depth-first.
    /// Returns an Array of UsdPrimProxy objects.
    Array traverse() const;

    /// Traverse prims matching a predicate (by type name).
    /// Example: traverse_by_type("Mesh") returns all Mesh prims.
    Array traverse_by_type(const String &p_type_name) const;

    // -------------------------------------------------------------------------
    // Prim Creation
    // -------------------------------------------------------------------------

    /// Define a new prim at the given path with the specified type.
    /// Creates parent Xforms automatically if they don't exist.
    /// Type examples: "Xform", "Mesh", "Cube", "Sphere", "Camera", "Light"
    Ref<UsdPrimProxy> define_prim(const String &p_path, const String &p_type_name);

    /// Remove a prim at the given path.
    Error remove_prim(const String &p_path);

    // -------------------------------------------------------------------------
    // Time / Animation
    // -------------------------------------------------------------------------

    /// Set the current time code for evaluation.
    void set_time_code(double p_time);

    /// Get the current time code.
    double get_time_code() const;

    /// Get the start time code of the stage's time range.
    double get_start_time_code() const;

    /// Get the end time code of the stage's time range.
    double get_end_time_code() const;

    /// Set the stage's time range.
    void set_time_range(double p_start, double p_end);

    /// Get frames per second for the stage.
    double get_frames_per_second() const;

    /// Set frames per second for the stage.
    void set_frames_per_second(double p_fps);

    // -------------------------------------------------------------------------
    // Stage Metadata
    // -------------------------------------------------------------------------

    /// Get the file path of the root layer.
    String get_root_layer_path() const;

    /// Get the up axis ("Y" or "Z").
    String get_up_axis() const;

    /// Set the up axis ("Y" or "Z").
    void set_up_axis(const String &p_axis);

    /// Get meters per unit scale factor.
    double get_meters_per_unit() const;

    /// Set meters per unit scale factor.
    void set_meters_per_unit(double p_meters_per_unit);

    // -------------------------------------------------------------------------
    // Layer Management (Simplified)
    // -------------------------------------------------------------------------

    /// Get the list of sublayer paths.
    PackedStringArray get_sublayer_paths() const;

    /// Add a sublayer to the root layer.
    Error add_sublayer(const String &p_path);

    /// Remove a sublayer from the root layer.
    Error remove_sublayer(const String &p_path);

    // -------------------------------------------------------------------------
    // Convenience Methods
    // -------------------------------------------------------------------------

    /// Import stage contents into a Godot scene tree under the given parent.
    /// Returns the root Node3D of the imported hierarchy.
    Node *import_to_scene(Node *p_parent) const;

    /// Export a Godot scene tree to this stage, replacing current contents.
    Error export_from_scene(Node *p_root);

    // -------------------------------------------------------------------------
    // Internal Access (for advanced users)
    // -------------------------------------------------------------------------

    /// Get the raw USD stage pointer. For advanced C++ interop only.
    UsdStageRefPtr get_stage_ptr() const { return _stage; }
};

} // namespace godot

#endif // USD_STAGE_PROXY_H
