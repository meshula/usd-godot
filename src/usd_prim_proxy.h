#ifndef USD_PRIM_PROXY_H
#define USD_PRIM_PROXY_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace godot {

class UsdStageProxy;

/**
 * UsdPrimProxy - Ergonomic GDScript wrapper for USD Prim operations.
 *
 * This class provides a GDScript-friendly interface to USD prims,
 * with automatic type conversion between USD and Godot types.
 *
 * Example GDScript usage:
 *   var prim = stage.get_prim_at_path("/World/MyMesh")
 *   print(prim.get_name())        # "MyMesh"
 *   print(prim.get_type_name())   # "Mesh"
 *   var xform = prim.get_local_transform()
 *   prim.set_attribute("visibility", "invisible")
 */
class UsdPrimProxy : public RefCounted {
    GDCLASS(UsdPrimProxy, RefCounted);

private:
    UsdPrim _prim;
    // Weak reference to stage to ensure prim validity
    UsdStageRefPtr _stage;

protected:
    static void _bind_methods();

public:
    UsdPrimProxy();
    ~UsdPrimProxy();

    // Factory method for internal use
    static Ref<UsdPrimProxy> create(const UsdPrim &p_prim, UsdStageRefPtr p_stage);

    // -------------------------------------------------------------------------
    // Prim Identity
    // -------------------------------------------------------------------------

    /// Get the prim's name (last component of path).
    String get_name() const;

    /// Get the full path of the prim (e.g., "/World/Characters/Hero").
    String get_path() const;

    /// Get the schema type name (e.g., "Mesh", "Xform", "Camera").
    String get_type_name() const;

    /// Check if this prim reference is valid.
    bool is_valid() const;

    /// Check if this prim is active.
    bool is_active() const;

    /// Set whether this prim is active.
    void set_active(bool p_active);

    // -------------------------------------------------------------------------
    // Hierarchy Navigation
    // -------------------------------------------------------------------------

    /// Get the parent prim. Returns null for root prims.
    Ref<UsdPrimProxy> get_parent() const;

    /// Get all direct children of this prim.
    Array get_children() const;

    /// Check if this prim has a child with the given name.
    bool has_child(const String &p_name) const;

    /// Get a child prim by name. Returns null if not found.
    Ref<UsdPrimProxy> get_child(const String &p_name) const;

    /// Get all descendants (recursive children).
    Array get_descendants() const;

    // -------------------------------------------------------------------------
    // Attributes
    // -------------------------------------------------------------------------

    /// Get the names of all attributes on this prim.
    PackedStringArray get_attribute_names() const;

    /// Check if an attribute exists.
    bool has_attribute(const String &p_name) const;

    /// Get an attribute value as a Godot Variant.
    /// Automatically converts USD types to appropriate Godot types:
    ///   - float/double -> float
    ///   - int -> int
    ///   - bool -> bool
    ///   - string/token -> String
    ///   - GfVec3f/d -> Vector3
    ///   - GfVec4f/d -> Color (if 4 components)
    ///   - GfMatrix4d -> Transform3D
    ///   - VtArray<T> -> PackedXxxArray
    Variant get_attribute(const String &p_name) const;

    /// Get an attribute value at a specific time code.
    Variant get_attribute_at_time(const String &p_name, double p_time) const;

    /// Set an attribute value from a Godot Variant.
    /// Creates the attribute if it doesn't exist.
    Error set_attribute(const String &p_name, const Variant &p_value);

    /// Set an attribute value at a specific time code (for animation).
    Error set_attribute_at_time(const String &p_name, const Variant &p_value, double p_time);

    /// Remove an attribute from the prim.
    Error remove_attribute(const String &p_name);

    /// Get attribute metadata (type, variability, etc.) as a Dictionary.
    Dictionary get_attribute_metadata(const String &p_name) const;

    // -------------------------------------------------------------------------
    // Transform Operations (convenience methods)
    // -------------------------------------------------------------------------

    /// Get the local transform of this prim.
    Transform3D get_local_transform() const;

    /// Get the local transform at a specific time code.
    Transform3D get_local_transform_at_time(double p_time) const;

    /// Set the local transform of this prim.
    void set_local_transform(const Transform3D &p_transform);

    /// Set the local transform at a specific time code.
    void set_local_transform_at_time(const Transform3D &p_transform, double p_time);

    /// Get the world (accumulated) transform.
    Transform3D get_world_transform() const;

    /// Get the world transform at a specific time code.
    Transform3D get_world_transform_at_time(double p_time) const;

    // -------------------------------------------------------------------------
    // Relationships
    // -------------------------------------------------------------------------

    /// Get the names of all relationships on this prim.
    PackedStringArray get_relationship_names() const;

    /// Check if a relationship exists.
    bool has_relationship(const String &p_name) const;

    /// Get relationship targets as an array of prim paths (Strings).
    PackedStringArray get_relationship_targets(const String &p_name) const;

    /// Set relationship targets from an array of prim paths.
    Error set_relationship_targets(const String &p_name, const PackedStringArray &p_targets);

    /// Add a target to a relationship.
    Error add_relationship_target(const String &p_name, const String &p_target_path);

    // -------------------------------------------------------------------------
    // Variants
    // -------------------------------------------------------------------------

    /// Get the names of all variant sets on this prim.
    PackedStringArray get_variant_sets() const;

    /// Check if a variant set exists.
    bool has_variant_set(const String &p_set_name) const;

    /// Get all variant names in a variant set.
    PackedStringArray get_variants(const String &p_set_name) const;

    /// Get the currently selected variant in a variant set.
    String get_variant_selection(const String &p_set_name) const;

    /// Set the variant selection.
    Error set_variant_selection(const String &p_set_name, const String &p_variant_name);

    // -------------------------------------------------------------------------
    // References & Payloads
    // -------------------------------------------------------------------------

    /// Check if this prim has any references.
    bool has_references() const;

    /// Add a reference to another USD file/prim.
    Error add_reference(const String &p_file_path, const String &p_prim_path = String());

    /// Check if this prim has any payloads.
    bool has_payloads() const;

    /// Add a payload (lazy-loaded reference).
    Error add_payload(const String &p_file_path, const String &p_prim_path = String());

    /// Load all payloads under this prim.
    void load_payloads();

    /// Unload all payloads under this prim.
    void unload_payloads();

    // -------------------------------------------------------------------------
    // Metadata
    // -------------------------------------------------------------------------

    /// Get custom data as a Dictionary.
    Dictionary get_custom_data() const;

    /// Set custom data from a Dictionary.
    void set_custom_data(const Dictionary &p_data);

    /// Get asset info as a Dictionary.
    Dictionary get_asset_info() const;

    /// Set asset info from a Dictionary.
    void set_asset_info(const Dictionary &p_info);

    /// Get documentation string.
    String get_documentation() const;

    /// Set documentation string.
    void set_documentation(const String &p_doc);

    // -------------------------------------------------------------------------
    // Convenience Type Checks
    // -------------------------------------------------------------------------

    /// Check if this is an Xform prim.
    bool is_xform() const;

    /// Check if this is a Mesh prim.
    bool is_mesh() const;

    /// Check if this is a Camera prim.
    bool is_camera() const;

    /// Check if this is a Light prim (any type).
    bool is_light() const;

    /// Check if this is a geometric primitive (Cube, Sphere, etc.).
    bool is_gprim() const;

    // -------------------------------------------------------------------------
    // Internal Access
    // -------------------------------------------------------------------------

    /// Get the raw USD prim. For advanced C++ interop only.
    UsdPrim get_prim() const { return _prim; }

private:
    // Type conversion helpers
    static Variant _usd_value_to_variant(const pxr::VtValue &p_value);
    static pxr::VtValue _variant_to_usd_value(const Variant &p_value, const pxr::SdfValueTypeName &p_type_hint);
};

} // namespace godot

#endif // USD_PRIM_PROXY_H
