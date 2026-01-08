#include "usd_prim_proxy.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/relationship.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usd/references.h>
#include <pxr/usd/usd/payloads.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/boundableLightBase.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/array.h>

namespace godot {

void UsdPrimProxy::_bind_methods() {
    // Prim Identity
    ClassDB::bind_method(D_METHOD("get_name"), &UsdPrimProxy::get_name);
    ClassDB::bind_method(D_METHOD("get_path"), &UsdPrimProxy::get_path);
    ClassDB::bind_method(D_METHOD("get_type_name"), &UsdPrimProxy::get_type_name);
    ClassDB::bind_method(D_METHOD("is_valid"), &UsdPrimProxy::is_valid);
    ClassDB::bind_method(D_METHOD("is_active"), &UsdPrimProxy::is_active);
    ClassDB::bind_method(D_METHOD("set_active", "active"), &UsdPrimProxy::set_active);

    // Hierarchy Navigation
    ClassDB::bind_method(D_METHOD("get_parent"), &UsdPrimProxy::get_parent);
    ClassDB::bind_method(D_METHOD("get_children"), &UsdPrimProxy::get_children);
    ClassDB::bind_method(D_METHOD("has_child", "name"), &UsdPrimProxy::has_child);
    ClassDB::bind_method(D_METHOD("get_child", "name"), &UsdPrimProxy::get_child);
    ClassDB::bind_method(D_METHOD("get_descendants"), &UsdPrimProxy::get_descendants);

    // Attributes
    ClassDB::bind_method(D_METHOD("get_attribute_names"), &UsdPrimProxy::get_attribute_names);
    ClassDB::bind_method(D_METHOD("has_attribute", "name"), &UsdPrimProxy::has_attribute);
    ClassDB::bind_method(D_METHOD("get_attribute", "name"), &UsdPrimProxy::get_attribute);
    ClassDB::bind_method(D_METHOD("get_attribute_at_time", "name", "time"), &UsdPrimProxy::get_attribute_at_time);
    ClassDB::bind_method(D_METHOD("set_attribute", "name", "value"), &UsdPrimProxy::set_attribute);
    ClassDB::bind_method(D_METHOD("set_attribute_at_time", "name", "value", "time"), &UsdPrimProxy::set_attribute_at_time);
    ClassDB::bind_method(D_METHOD("remove_attribute", "name"), &UsdPrimProxy::remove_attribute);
    ClassDB::bind_method(D_METHOD("get_attribute_metadata", "name"), &UsdPrimProxy::get_attribute_metadata);

    // Transform Operations
    ClassDB::bind_method(D_METHOD("get_local_transform"), &UsdPrimProxy::get_local_transform);
    ClassDB::bind_method(D_METHOD("get_local_transform_at_time", "time"), &UsdPrimProxy::get_local_transform_at_time);
    ClassDB::bind_method(D_METHOD("set_local_transform", "transform"), &UsdPrimProxy::set_local_transform);
    ClassDB::bind_method(D_METHOD("set_local_transform_at_time", "transform", "time"), &UsdPrimProxy::set_local_transform_at_time);
    ClassDB::bind_method(D_METHOD("get_world_transform"), &UsdPrimProxy::get_world_transform);
    ClassDB::bind_method(D_METHOD("get_world_transform_at_time", "time"), &UsdPrimProxy::get_world_transform_at_time);

    // Relationships
    ClassDB::bind_method(D_METHOD("get_relationship_names"), &UsdPrimProxy::get_relationship_names);
    ClassDB::bind_method(D_METHOD("has_relationship", "name"), &UsdPrimProxy::has_relationship);
    ClassDB::bind_method(D_METHOD("get_relationship_targets", "name"), &UsdPrimProxy::get_relationship_targets);
    ClassDB::bind_method(D_METHOD("set_relationship_targets", "name", "targets"), &UsdPrimProxy::set_relationship_targets);
    ClassDB::bind_method(D_METHOD("add_relationship_target", "name", "target_path"), &UsdPrimProxy::add_relationship_target);

    // Variants
    ClassDB::bind_method(D_METHOD("get_variant_sets"), &UsdPrimProxy::get_variant_sets);
    ClassDB::bind_method(D_METHOD("has_variant_set", "set_name"), &UsdPrimProxy::has_variant_set);
    ClassDB::bind_method(D_METHOD("get_variants", "set_name"), &UsdPrimProxy::get_variants);
    ClassDB::bind_method(D_METHOD("get_variant_selection", "set_name"), &UsdPrimProxy::get_variant_selection);
    ClassDB::bind_method(D_METHOD("set_variant_selection", "set_name", "variant_name"), &UsdPrimProxy::set_variant_selection);

    // References & Payloads
    ClassDB::bind_method(D_METHOD("has_references"), &UsdPrimProxy::has_references);
    ClassDB::bind_method(D_METHOD("add_reference", "file_path", "prim_path"), &UsdPrimProxy::add_reference, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("has_payloads"), &UsdPrimProxy::has_payloads);
    ClassDB::bind_method(D_METHOD("add_payload", "file_path", "prim_path"), &UsdPrimProxy::add_payload, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("load_payloads"), &UsdPrimProxy::load_payloads);
    ClassDB::bind_method(D_METHOD("unload_payloads"), &UsdPrimProxy::unload_payloads);

    // Metadata
    ClassDB::bind_method(D_METHOD("get_custom_data"), &UsdPrimProxy::get_custom_data);
    ClassDB::bind_method(D_METHOD("set_custom_data", "data"), &UsdPrimProxy::set_custom_data);
    ClassDB::bind_method(D_METHOD("get_asset_info"), &UsdPrimProxy::get_asset_info);
    ClassDB::bind_method(D_METHOD("set_asset_info", "info"), &UsdPrimProxy::set_asset_info);
    ClassDB::bind_method(D_METHOD("get_documentation"), &UsdPrimProxy::get_documentation);
    ClassDB::bind_method(D_METHOD("set_documentation", "doc"), &UsdPrimProxy::set_documentation);

    // Type Checks
    ClassDB::bind_method(D_METHOD("is_xform"), &UsdPrimProxy::is_xform);
    ClassDB::bind_method(D_METHOD("is_mesh"), &UsdPrimProxy::is_mesh);
    ClassDB::bind_method(D_METHOD("is_camera"), &UsdPrimProxy::is_camera);
    ClassDB::bind_method(D_METHOD("is_light"), &UsdPrimProxy::is_light);
    ClassDB::bind_method(D_METHOD("is_gprim"), &UsdPrimProxy::is_gprim);
}

UsdPrimProxy::UsdPrimProxy() {
}

UsdPrimProxy::~UsdPrimProxy() {
}

Ref<UsdPrimProxy> UsdPrimProxy::create(const UsdPrim &p_prim, UsdStageRefPtr p_stage) {
    Ref<UsdPrimProxy> proxy;
    proxy.instantiate();
    proxy->_prim = p_prim;
    proxy->_stage = p_stage;
    return proxy;
}

// -----------------------------------------------------------------------------
// Prim Identity
// -----------------------------------------------------------------------------

String UsdPrimProxy::get_name() const {
    if (!_prim.IsValid()) {
        return String();
    }
    return String(_prim.GetName().GetText());
}

String UsdPrimProxy::get_path() const {
    if (!_prim.IsValid()) {
        return String();
    }
    return String(_prim.GetPath().GetString().c_str());
}

String UsdPrimProxy::get_type_name() const {
    if (!_prim.IsValid()) {
        return String();
    }
    return String(_prim.GetTypeName().GetText());
}

bool UsdPrimProxy::is_valid() const {
    return _prim.IsValid();
}

bool UsdPrimProxy::is_active() const {
    if (!_prim.IsValid()) {
        return false;
    }
    return _prim.IsActive();
}

void UsdPrimProxy::set_active(bool p_active) {
    if (!_prim.IsValid()) {
        return;
    }
    _prim.SetActive(p_active);
}

// -----------------------------------------------------------------------------
// Hierarchy Navigation
// -----------------------------------------------------------------------------

Ref<UsdPrimProxy> UsdPrimProxy::get_parent() const {
    if (!_prim.IsValid()) {
        return Ref<UsdPrimProxy>();
    }

    pxr::UsdPrim parent = _prim.GetParent();
    if (!parent.IsValid() || parent.IsPseudoRoot()) {
        return Ref<UsdPrimProxy>();
    }

    return UsdPrimProxy::create(parent, _stage);
}

Array UsdPrimProxy::get_children() const {
    Array result;
    if (!_prim.IsValid()) {
        return result;
    }

    for (const pxr::UsdPrim &child : _prim.GetChildren()) {
        result.push_back(UsdPrimProxy::create(child, _stage));
    }

    return result;
}

bool UsdPrimProxy::has_child(const String &p_name) const {
    if (!_prim.IsValid()) {
        return false;
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    return _prim.GetChild(name_token).IsValid();
}

Ref<UsdPrimProxy> UsdPrimProxy::get_child(const String &p_name) const {
    if (!_prim.IsValid()) {
        return Ref<UsdPrimProxy>();
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    pxr::UsdPrim child = _prim.GetChild(name_token);
    if (!child.IsValid()) {
        return Ref<UsdPrimProxy>();
    }

    return UsdPrimProxy::create(child, _stage);
}

Array UsdPrimProxy::get_descendants() const {
    Array result;
    if (!_prim.IsValid()) {
        return result;
    }

    for (const pxr::UsdPrim &desc : _prim.GetDescendants()) {
        result.push_back(UsdPrimProxy::create(desc, _stage));
    }

    return result;
}

// -----------------------------------------------------------------------------
// Attributes
// -----------------------------------------------------------------------------

PackedStringArray UsdPrimProxy::get_attribute_names() const {
    PackedStringArray result;
    if (!_prim.IsValid()) {
        return result;
    }

    for (const pxr::UsdAttribute &attr : _prim.GetAttributes()) {
        result.push_back(String(attr.GetName().GetText()));
    }

    return result;
}

bool UsdPrimProxy::has_attribute(const String &p_name) const {
    if (!_prim.IsValid()) {
        return false;
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    return _prim.HasAttribute(name_token);
}

Variant UsdPrimProxy::get_attribute(const String &p_name) const {
    return get_attribute_at_time(p_name, pxr::UsdTimeCode::Default().GetValue());
}

Variant UsdPrimProxy::get_attribute_at_time(const String &p_name, double p_time) const {
    if (!_prim.IsValid()) {
        return Variant();
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    pxr::UsdAttribute attr = _prim.GetAttribute(name_token);
    if (!attr.IsValid()) {
        return Variant();
    }

    pxr::VtValue value;
    pxr::UsdTimeCode time_code(p_time);
    if (!attr.Get(&value, time_code)) {
        return Variant();
    }

    return _usd_value_to_variant(value);
}

Error UsdPrimProxy::set_attribute(const String &p_name, const Variant &p_value) {
    return set_attribute_at_time(p_name, p_value, pxr::UsdTimeCode::Default().GetValue());
}

Error UsdPrimProxy::set_attribute_at_time(const String &p_name, const Variant &p_value, double p_time) {
    if (!_prim.IsValid()) {
        return ERR_UNCONFIGURED;
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    pxr::UsdAttribute attr = _prim.GetAttribute(name_token);

    // If attribute doesn't exist, we need type info to create it
    if (!attr.IsValid()) {
        UtilityFunctions::printerr("UsdPrimProxy: Attribute does not exist: ", p_name);
        return ERR_DOES_NOT_EXIST;
    }

    pxr::VtValue usd_value = _variant_to_usd_value(p_value, attr.GetTypeName());
    if (usd_value.IsEmpty()) {
        UtilityFunctions::printerr("UsdPrimProxy: Could not convert value for attribute: ", p_name);
        return ERR_INVALID_PARAMETER;
    }

    pxr::UsdTimeCode time_code(p_time);
    if (!attr.Set(usd_value, time_code)) {
        return ERR_CANT_CREATE;
    }

    return OK;
}

Error UsdPrimProxy::remove_attribute(const String &p_name) {
    if (!_prim.IsValid()) {
        return ERR_UNCONFIGURED;
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    if (_prim.RemoveProperty(name_token)) {
        return OK;
    }
    return ERR_DOES_NOT_EXIST;
}

Dictionary UsdPrimProxy::get_attribute_metadata(const String &p_name) const {
    Dictionary result;
    if (!_prim.IsValid()) {
        return result;
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    pxr::UsdAttribute attr = _prim.GetAttribute(name_token);
    if (!attr.IsValid()) {
        return result;
    }

    result["type_name"] = String(attr.GetTypeName().GetAsToken().GetText());
    result["variability"] = attr.GetVariability() == pxr::SdfVariabilityVarying ? "varying" : "uniform";
    result["is_authored"] = attr.IsAuthored();
    result["has_value"] = attr.HasValue();

    return result;
}

// -----------------------------------------------------------------------------
// Transform Operations
// -----------------------------------------------------------------------------

Transform3D UsdPrimProxy::get_local_transform() const {
    return get_local_transform_at_time(pxr::UsdTimeCode::Default().GetValue());
}

Transform3D UsdPrimProxy::get_local_transform_at_time(double p_time) const {
    Transform3D result;
    if (!_prim.IsValid()) {
        return result;
    }

    if (!_prim.IsA<pxr::UsdGeomXformable>()) {
        return result;
    }

    pxr::UsdGeomXformable xformable(_prim);
    pxr::GfMatrix4d matrix;
    bool reset_xform_stack;
    pxr::UsdTimeCode time_code(p_time);

    if (!xformable.GetLocalTransformation(&matrix, &reset_xform_stack, time_code)) {
        return result;
    }

    // Convert USD matrix to Godot transform
    Basis basis;
    basis.set_column(0, Vector3(matrix[0][0], matrix[0][1], matrix[0][2]));
    basis.set_column(1, Vector3(matrix[1][0], matrix[1][1], matrix[1][2]));
    basis.set_column(2, Vector3(matrix[2][0], matrix[2][1], matrix[2][2]));

    Vector3 origin(matrix[3][0], matrix[3][1], matrix[3][2]);

    result.set_basis(basis);
    result.set_origin(origin);

    return result;
}

void UsdPrimProxy::set_local_transform(const Transform3D &p_transform) {
    set_local_transform_at_time(p_transform, pxr::UsdTimeCode::Default().GetValue());
}

void UsdPrimProxy::set_local_transform_at_time(const Transform3D &p_transform, double p_time) {
    if (!_prim.IsValid()) {
        return;
    }

    if (!_prim.IsA<pxr::UsdGeomXformable>()) {
        UtilityFunctions::printerr("UsdPrimProxy: Prim is not transformable");
        return;
    }

    pxr::UsdGeomXformable xformable(_prim);

    // Convert Godot transform to USD matrix
    Basis basis = p_transform.get_basis();
    Vector3 origin = p_transform.get_origin();

    pxr::GfMatrix4d matrix;
    matrix.SetRow(0, pxr::GfVec4d(basis.get_column(0).x, basis.get_column(0).y, basis.get_column(0).z, 0.0));
    matrix.SetRow(1, pxr::GfVec4d(basis.get_column(1).x, basis.get_column(1).y, basis.get_column(1).z, 0.0));
    matrix.SetRow(2, pxr::GfVec4d(basis.get_column(2).x, basis.get_column(2).y, basis.get_column(2).z, 0.0));
    matrix.SetRow(3, pxr::GfVec4d(origin.x, origin.y, origin.z, 1.0));

    // Clear existing xform ops and add a single transform op
    xformable.ClearXformOpOrder();
    pxr::UsdGeomXformOp transform_op = xformable.AddTransformOp();
    pxr::UsdTimeCode time_code(p_time);
    transform_op.Set(matrix, time_code);
}

Transform3D UsdPrimProxy::get_world_transform() const {
    return get_world_transform_at_time(pxr::UsdTimeCode::Default().GetValue());
}

Transform3D UsdPrimProxy::get_world_transform_at_time(double p_time) const {
    Transform3D result;
    if (!_prim.IsValid()) {
        return result;
    }

    if (!_prim.IsA<pxr::UsdGeomXformable>()) {
        return result;
    }

    pxr::UsdGeomXformable xformable(_prim);
    pxr::UsdTimeCode time_code(p_time);

    pxr::GfMatrix4d matrix = xformable.ComputeLocalToWorldTransform(time_code);

    // Convert USD matrix to Godot transform
    Basis basis;
    basis.set_column(0, Vector3(matrix[0][0], matrix[0][1], matrix[0][2]));
    basis.set_column(1, Vector3(matrix[1][0], matrix[1][1], matrix[1][2]));
    basis.set_column(2, Vector3(matrix[2][0], matrix[2][1], matrix[2][2]));

    Vector3 origin(matrix[3][0], matrix[3][1], matrix[3][2]);

    result.set_basis(basis);
    result.set_origin(origin);

    return result;
}

// -----------------------------------------------------------------------------
// Relationships
// -----------------------------------------------------------------------------

PackedStringArray UsdPrimProxy::get_relationship_names() const {
    PackedStringArray result;
    if (!_prim.IsValid()) {
        return result;
    }

    for (const pxr::UsdRelationship &rel : _prim.GetRelationships()) {
        result.push_back(String(rel.GetName().GetText()));
    }

    return result;
}

bool UsdPrimProxy::has_relationship(const String &p_name) const {
    if (!_prim.IsValid()) {
        return false;
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    return _prim.HasRelationship(name_token);
}

PackedStringArray UsdPrimProxy::get_relationship_targets(const String &p_name) const {
    PackedStringArray result;
    if (!_prim.IsValid()) {
        return result;
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    pxr::UsdRelationship rel = _prim.GetRelationship(name_token);
    if (!rel.IsValid()) {
        return result;
    }

    pxr::SdfPathVector targets;
    rel.GetTargets(&targets);

    for (const pxr::SdfPath &target : targets) {
        result.push_back(String(target.GetString().c_str()));
    }

    return result;
}

Error UsdPrimProxy::set_relationship_targets(const String &p_name, const PackedStringArray &p_targets) {
    if (!_prim.IsValid()) {
        return ERR_UNCONFIGURED;
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    pxr::UsdRelationship rel = _prim.GetRelationship(name_token);
    if (!rel.IsValid()) {
        rel = _prim.CreateRelationship(name_token);
    }

    pxr::SdfPathVector targets;
    for (int i = 0; i < p_targets.size(); ++i) {
        targets.push_back(pxr::SdfPath(p_targets[i].utf8().get_data()));
    }

    if (rel.SetTargets(targets)) {
        return OK;
    }
    return ERR_CANT_CREATE;
}

Error UsdPrimProxy::add_relationship_target(const String &p_name, const String &p_target_path) {
    if (!_prim.IsValid()) {
        return ERR_UNCONFIGURED;
    }

    pxr::TfToken name_token(p_name.utf8().get_data());
    pxr::UsdRelationship rel = _prim.GetRelationship(name_token);
    if (!rel.IsValid()) {
        rel = _prim.CreateRelationship(name_token);
    }

    pxr::SdfPath target(p_target_path.utf8().get_data());
    if (rel.AddTarget(target)) {
        return OK;
    }
    return ERR_CANT_CREATE;
}

// -----------------------------------------------------------------------------
// Variants
// -----------------------------------------------------------------------------

PackedStringArray UsdPrimProxy::get_variant_sets() const {
    PackedStringArray result;
    if (!_prim.IsValid()) {
        return result;
    }

    pxr::UsdVariantSets variant_sets = _prim.GetVariantSets();
    for (const std::string &name : variant_sets.GetNames()) {
        result.push_back(String(name.c_str()));
    }

    return result;
}

bool UsdPrimProxy::has_variant_set(const String &p_set_name) const {
    if (!_prim.IsValid()) {
        return false;
    }

    return _prim.GetVariantSets().HasVariantSet(p_set_name.utf8().get_data());
}

PackedStringArray UsdPrimProxy::get_variants(const String &p_set_name) const {
    PackedStringArray result;
    if (!_prim.IsValid()) {
        return result;
    }

    pxr::UsdVariantSet variant_set = _prim.GetVariantSet(p_set_name.utf8().get_data());
    if (!variant_set.IsValid()) {
        return result;
    }

    for (const std::string &name : variant_set.GetVariantNames()) {
        result.push_back(String(name.c_str()));
    }

    return result;
}

String UsdPrimProxy::get_variant_selection(const String &p_set_name) const {
    if (!_prim.IsValid()) {
        return String();
    }

    pxr::UsdVariantSet variant_set = _prim.GetVariantSet(p_set_name.utf8().get_data());
    if (!variant_set.IsValid()) {
        return String();
    }

    return String(variant_set.GetVariantSelection().c_str());
}

Error UsdPrimProxy::set_variant_selection(const String &p_set_name, const String &p_variant_name) {
    if (!_prim.IsValid()) {
        return ERR_UNCONFIGURED;
    }

    pxr::UsdVariantSet variant_set = _prim.GetVariantSet(p_set_name.utf8().get_data());
    if (!variant_set.IsValid()) {
        return ERR_DOES_NOT_EXIST;
    }

    if (variant_set.SetVariantSelection(p_variant_name.utf8().get_data())) {
        return OK;
    }
    return ERR_CANT_CREATE;
}

// -----------------------------------------------------------------------------
// References & Payloads
// -----------------------------------------------------------------------------

bool UsdPrimProxy::has_references() const {
    if (!_prim.IsValid()) {
        return false;
    }
    return _prim.HasAuthoredReferences();
}

Error UsdPrimProxy::add_reference(const String &p_file_path, const String &p_prim_path) {
    if (!_prim.IsValid()) {
        return ERR_UNCONFIGURED;
    }

    pxr::UsdReferences refs = _prim.GetReferences();
    pxr::SdfPath prim_path;
    if (!p_prim_path.is_empty()) {
        prim_path = pxr::SdfPath(p_prim_path.utf8().get_data());
    }

    if (refs.AddReference(p_file_path.utf8().get_data(), prim_path)) {
        return OK;
    }
    return ERR_CANT_CREATE;
}

bool UsdPrimProxy::has_payloads() const {
    if (!_prim.IsValid()) {
        return false;
    }
    return _prim.HasAuthoredPayloads();
}

Error UsdPrimProxy::add_payload(const String &p_file_path, const String &p_prim_path) {
    if (!_prim.IsValid()) {
        return ERR_UNCONFIGURED;
    }

    pxr::UsdPayloads payloads = _prim.GetPayloads();
    pxr::SdfPath prim_path;
    if (!p_prim_path.is_empty()) {
        prim_path = pxr::SdfPath(p_prim_path.utf8().get_data());
    }

    if (payloads.AddPayload(p_file_path.utf8().get_data(), prim_path)) {
        return OK;
    }
    return ERR_CANT_CREATE;
}

void UsdPrimProxy::load_payloads() {
    if (!_prim.IsValid() || !_stage) {
        return;
    }
    _stage->Load(_prim.GetPath());
}

void UsdPrimProxy::unload_payloads() {
    if (!_prim.IsValid() || !_stage) {
        return;
    }
    _stage->Unload(_prim.GetPath());
}

// -----------------------------------------------------------------------------
// Metadata
// -----------------------------------------------------------------------------

Dictionary UsdPrimProxy::get_custom_data() const {
    Dictionary result;
    if (!_prim.IsValid()) {
        return result;
    }

    pxr::VtDictionary custom_data = _prim.GetCustomData();
    for (const auto &pair : custom_data) {
        result[String(pair.first.c_str())] = _usd_value_to_variant(pair.second);
    }

    return result;
}

void UsdPrimProxy::set_custom_data(const Dictionary &p_data) {
    if (!_prim.IsValid()) {
        return;
    }

    pxr::VtDictionary custom_data;
    Array keys = p_data.keys();
    for (int i = 0; i < keys.size(); ++i) {
        String key = keys[i];
        Variant value = p_data[key];

        // Convert basic types
        switch (value.get_type()) {
            case Variant::STRING:
                custom_data[key.utf8().get_data()] = pxr::VtValue(std::string(String(value).utf8().get_data()));
                break;
            case Variant::INT:
                custom_data[key.utf8().get_data()] = pxr::VtValue(int64_t(value));
                break;
            case Variant::FLOAT:
                custom_data[key.utf8().get_data()] = pxr::VtValue(double(value));
                break;
            case Variant::BOOL:
                custom_data[key.utf8().get_data()] = pxr::VtValue(bool(value));
                break;
            default:
                break;
        }
    }

    _prim.SetCustomData(custom_data);
}

Dictionary UsdPrimProxy::get_asset_info() const {
    Dictionary result;
    if (!_prim.IsValid()) {
        return result;
    }

    pxr::VtDictionary asset_info = _prim.GetAssetInfo();
    for (const auto &pair : asset_info) {
        result[String(pair.first.c_str())] = _usd_value_to_variant(pair.second);
    }

    return result;
}

void UsdPrimProxy::set_asset_info(const Dictionary &p_info) {
    if (!_prim.IsValid()) {
        return;
    }

    pxr::VtDictionary asset_info;
    Array keys = p_info.keys();
    for (int i = 0; i < keys.size(); ++i) {
        String key = keys[i];
        Variant value = p_info[key];

        if (value.get_type() == Variant::STRING) {
            asset_info[key.utf8().get_data()] = pxr::VtValue(std::string(String(value).utf8().get_data()));
        }
    }

    _prim.SetAssetInfo(asset_info);
}

String UsdPrimProxy::get_documentation() const {
    if (!_prim.IsValid()) {
        return String();
    }

    std::string doc;
    if (_prim.GetMetadata(pxr::SdfFieldKeys->Documentation, &doc)) {
        return String(doc.c_str());
    }
    return String();
}

void UsdPrimProxy::set_documentation(const String &p_doc) {
    if (!_prim.IsValid()) {
        return;
    }

    _prim.SetMetadata(pxr::SdfFieldKeys->Documentation, std::string(p_doc.utf8().get_data()));
}

// -----------------------------------------------------------------------------
// Type Checks
// -----------------------------------------------------------------------------

bool UsdPrimProxy::is_xform() const {
    if (!_prim.IsValid()) {
        return false;
    }
    return _prim.IsA<pxr::UsdGeomXform>();
}

bool UsdPrimProxy::is_mesh() const {
    if (!_prim.IsValid()) {
        return false;
    }
    return _prim.IsA<pxr::UsdGeomMesh>();
}

bool UsdPrimProxy::is_camera() const {
    if (!_prim.IsValid()) {
        return false;
    }
    return _prim.IsA<pxr::UsdGeomCamera>();
}

bool UsdPrimProxy::is_light() const {
    if (!_prim.IsValid()) {
        return false;
    }
    return _prim.HasAPI<pxr::UsdLuxLightAPI>();
}

bool UsdPrimProxy::is_gprim() const {
    if (!_prim.IsValid()) {
        return false;
    }
    return _prim.IsA<pxr::UsdGeomGprim>();
}

// -----------------------------------------------------------------------------
// Type Conversion Helpers
// -----------------------------------------------------------------------------

Variant UsdPrimProxy::_usd_value_to_variant(const pxr::VtValue &p_value) {
    if (p_value.IsEmpty()) {
        return Variant();
    }

    // Basic types
    if (p_value.IsHolding<bool>()) {
        return p_value.Get<bool>();
    }
    if (p_value.IsHolding<int>()) {
        return p_value.Get<int>();
    }
    if (p_value.IsHolding<int64_t>()) {
        return (int64_t)p_value.Get<int64_t>();
    }
    if (p_value.IsHolding<float>()) {
        return p_value.Get<float>();
    }
    if (p_value.IsHolding<double>()) {
        return p_value.Get<double>();
    }
    if (p_value.IsHolding<std::string>()) {
        return String(p_value.Get<std::string>().c_str());
    }
    if (p_value.IsHolding<pxr::TfToken>()) {
        return String(p_value.Get<pxr::TfToken>().GetText());
    }

    // Vector types
    if (p_value.IsHolding<pxr::GfVec3f>()) {
        pxr::GfVec3f v = p_value.Get<pxr::GfVec3f>();
        return Vector3(v[0], v[1], v[2]);
    }
    if (p_value.IsHolding<pxr::GfVec3d>()) {
        pxr::GfVec3d v = p_value.Get<pxr::GfVec3d>();
        return Vector3(v[0], v[1], v[2]);
    }
    if (p_value.IsHolding<pxr::GfVec4f>()) {
        pxr::GfVec4f v = p_value.Get<pxr::GfVec4f>();
        return Color(v[0], v[1], v[2], v[3]);
    }

    // Matrix type
    if (p_value.IsHolding<pxr::GfMatrix4d>()) {
        pxr::GfMatrix4d m = p_value.Get<pxr::GfMatrix4d>();
        Transform3D t;
        Basis basis;
        basis.set_column(0, Vector3(m[0][0], m[0][1], m[0][2]));
        basis.set_column(1, Vector3(m[1][0], m[1][1], m[1][2]));
        basis.set_column(2, Vector3(m[2][0], m[2][1], m[2][2]));
        t.set_basis(basis);
        t.set_origin(Vector3(m[3][0], m[3][1], m[3][2]));
        return t;
    }

    // Array types
    if (p_value.IsHolding<pxr::VtArray<float>>()) {
        pxr::VtArray<float> arr = p_value.Get<pxr::VtArray<float>>();
        PackedFloat32Array result;
        for (float f : arr) {
            result.push_back(f);
        }
        return result;
    }
    if (p_value.IsHolding<pxr::VtArray<int>>()) {
        pxr::VtArray<int> arr = p_value.Get<pxr::VtArray<int>>();
        PackedInt32Array result;
        for (int i : arr) {
            result.push_back(i);
        }
        return result;
    }
    if (p_value.IsHolding<pxr::VtArray<pxr::GfVec3f>>()) {
        pxr::VtArray<pxr::GfVec3f> arr = p_value.Get<pxr::VtArray<pxr::GfVec3f>>();
        PackedVector3Array result;
        for (const pxr::GfVec3f &v : arr) {
            result.push_back(Vector3(v[0], v[1], v[2]));
        }
        return result;
    }

    // Unknown type - return type name as string
    return String("USD: ") + String(p_value.GetTypeName().c_str());
}

pxr::VtValue UsdPrimProxy::_variant_to_usd_value(const Variant &p_value, const pxr::SdfValueTypeName &p_type_hint) {
    switch (p_value.get_type()) {
        case Variant::BOOL:
            return pxr::VtValue(bool(p_value));

        case Variant::INT:
            if (p_type_hint == pxr::SdfValueTypeNames->Int64) {
                return pxr::VtValue(int64_t(p_value));
            }
            return pxr::VtValue(int(p_value));

        case Variant::FLOAT:
            if (p_type_hint == pxr::SdfValueTypeNames->Float) {
                return pxr::VtValue(float(p_value));
            }
            return pxr::VtValue(double(p_value));

        case Variant::STRING: {
            String s = p_value;
            if (p_type_hint == pxr::SdfValueTypeNames->Token) {
                return pxr::VtValue(pxr::TfToken(s.utf8().get_data()));
            }
            return pxr::VtValue(std::string(s.utf8().get_data()));
        }

        case Variant::VECTOR3: {
            Vector3 v = p_value;
            if (p_type_hint == pxr::SdfValueTypeNames->Float3 ||
                p_type_hint == pxr::SdfValueTypeNames->Vector3f) {
                return pxr::VtValue(pxr::GfVec3f(v.x, v.y, v.z));
            }
            return pxr::VtValue(pxr::GfVec3d(v.x, v.y, v.z));
        }

        case Variant::COLOR: {
            Color c = p_value;
            return pxr::VtValue(pxr::GfVec4f(c.r, c.g, c.b, c.a));
        }

        case Variant::TRANSFORM3D: {
            Transform3D t = p_value;
            Basis basis = t.get_basis();
            Vector3 origin = t.get_origin();
            pxr::GfMatrix4d m;
            m.SetRow(0, pxr::GfVec4d(basis.get_column(0).x, basis.get_column(0).y, basis.get_column(0).z, 0.0));
            m.SetRow(1, pxr::GfVec4d(basis.get_column(1).x, basis.get_column(1).y, basis.get_column(1).z, 0.0));
            m.SetRow(2, pxr::GfVec4d(basis.get_column(2).x, basis.get_column(2).y, basis.get_column(2).z, 0.0));
            m.SetRow(3, pxr::GfVec4d(origin.x, origin.y, origin.z, 1.0));
            return pxr::VtValue(m);
        }

        default:
            return pxr::VtValue();
    }
}

} // namespace godot
