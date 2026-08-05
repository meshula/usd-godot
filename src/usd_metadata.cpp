// ---------------------------------------------------------------------------------------------
// Transplanted from godot-usd (https://github.com/migueldeicaza) — MIT License, (c) 2026 Miguel de
// Icaza. Harvested per USD_GODOT_INTEGRATION_PLAN.md (Phase 4/composition), lifted near-verbatim.
// Captures composition arcs (references/payloads/inherits/specializes -> usd:* metadata +
// usd:composition_preservation_mode="read_only") and unmapped USD properties on import.
// See THIRD_PARTY.md.
// ---------------------------------------------------------------------------------------------

#include "usd_metadata.h"

#include <string>
#include <unordered_set>

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/layerOffset.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/payload.h>
#include <pxr/usd/sdf/reference.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/relationship.h>

#include "usd_stage_utils.h"

namespace godot_usd {

using namespace godot;
using namespace pxr;

namespace {

Dictionary serialize_layer_offset(const SdfLayerOffset &p_layer_offset) {
	Dictionary description;
	description["offset"] = p_layer_offset.GetOffset();
	description["scale"] = p_layer_offset.GetScale();
	return description;
}

Dictionary serialize_attribute(const UsdAttribute &p_attribute, const UsdTimeCode &p_time) {
	Dictionary description;
	description["type_name"] = to_godot_string(p_attribute.GetTypeName().GetAsToken().GetString());
	description["is_custom"] = p_attribute.IsCustom();

	VtValue value;
	if (!p_attribute.Get(&value, p_time)) {
		return description;
	}

	description["value"] = to_godot_string(TfStringify(value));

	const SdfValueTypeName type_name = p_attribute.GetTypeName();
	if (type_name == SdfValueTypeNames->Bool) {
		description["typed_value_kind"] = "bool";
		description["typed_value"] = value.UncheckedGet<bool>();
	} else if (type_name == SdfValueTypeNames->Int) {
		description["typed_value_kind"] = "int";
		description["typed_value"] = value.UncheckedGet<int>();
	} else if (type_name == SdfValueTypeNames->Int64) {
		description["typed_value_kind"] = "int64";
		description["typed_value"] = (int64_t)value.UncheckedGet<int64_t>();
	} else if (type_name == SdfValueTypeNames->Float) {
		description["typed_value_kind"] = "float";
		description["typed_value"] = value.UncheckedGet<float>();
	} else if (type_name == SdfValueTypeNames->Double) {
		description["typed_value_kind"] = "double";
		description["typed_value"] = value.UncheckedGet<double>();
	} else if (type_name == SdfValueTypeNames->String) {
		description["typed_value_kind"] = "string";
		description["typed_value"] = to_godot_string(value.UncheckedGet<std::string>());
	} else if (type_name == SdfValueTypeNames->Token) {
		description["typed_value_kind"] = "token";
		description["typed_value"] = to_godot_string(value.UncheckedGet<TfToken>().GetString());
	} else if (type_name == SdfValueTypeNames->Asset) {
		description["typed_value_kind"] = "asset";
		description["typed_value"] = to_godot_string(value.UncheckedGet<SdfAssetPath>().GetAssetPath());
	} else if (type_name == SdfValueTypeNames->Float2 || type_name == SdfValueTypeNames->TexCoord2f) {
		const GfVec2f vec = value.UncheckedGet<GfVec2f>();
		description["typed_value_kind"] = "vector2";
		description["typed_value"] = Vector2(vec[0], vec[1]);
	} else if (type_name == SdfValueTypeNames->Float3 || type_name == SdfValueTypeNames->Color3f || type_name == SdfValueTypeNames->Normal3f || type_name == SdfValueTypeNames->Point3f || type_name == SdfValueTypeNames->Vector3f) {
		const GfVec3f vec = value.UncheckedGet<GfVec3f>();
		description["typed_value_kind"] = "vector3";
		description["typed_value"] = Vector3(vec[0], vec[1], vec[2]);
	} else if (type_name == SdfValueTypeNames->BoolArray) {
		const VtArray<bool> values = value.UncheckedGet<VtArray<bool>>();
		Array serialized_values;
		for (size_t i = 0; i < values.size(); i++) {
			serialized_values.push_back(values[i]);
		}
		description["typed_value_kind"] = "bool_array";
		description["typed_value"] = serialized_values;
	} else if (type_name == SdfValueTypeNames->IntArray) {
		const VtArray<int> values = value.UncheckedGet<VtArray<int>>();
		Array serialized_values;
		for (size_t i = 0; i < values.size(); i++) {
			serialized_values.push_back(values[i]);
		}
		description["typed_value_kind"] = "int_array";
		description["typed_value"] = serialized_values;
	} else if (type_name == SdfValueTypeNames->Int64Array) {
		const VtArray<int64_t> values = value.UncheckedGet<VtArray<int64_t>>();
		Array serialized_values;
		for (size_t i = 0; i < values.size(); i++) {
			serialized_values.push_back((int64_t)values[i]);
		}
		description["typed_value_kind"] = "int64_array";
		description["typed_value"] = serialized_values;
	} else if (type_name == SdfValueTypeNames->FloatArray) {
		const VtArray<float> values = value.UncheckedGet<VtArray<float>>();
		Array serialized_values;
		for (size_t i = 0; i < values.size(); i++) {
			serialized_values.push_back(values[i]);
		}
		description["typed_value_kind"] = "float_array";
		description["typed_value"] = serialized_values;
	} else if (type_name == SdfValueTypeNames->DoubleArray) {
		const VtArray<double> values = value.UncheckedGet<VtArray<double>>();
		Array serialized_values;
		for (size_t i = 0; i < values.size(); i++) {
			serialized_values.push_back(values[i]);
		}
		description["typed_value_kind"] = "double_array";
		description["typed_value"] = serialized_values;
	} else if (type_name == SdfValueTypeNames->StringArray) {
		const VtArray<std::string> values = value.UncheckedGet<VtArray<std::string>>();
		Array serialized_values;
		for (size_t i = 0; i < values.size(); i++) {
			serialized_values.push_back(to_godot_string(values[i]));
		}
		description["typed_value_kind"] = "string_array";
		description["typed_value"] = serialized_values;
	} else if (type_name == SdfValueTypeNames->TokenArray) {
		const VtArray<TfToken> values = value.UncheckedGet<VtArray<TfToken>>();
		Array serialized_values;
		for (size_t i = 0; i < values.size(); i++) {
			serialized_values.push_back(to_godot_string(values[i].GetString()));
		}
		description["typed_value_kind"] = "token_array";
		description["typed_value"] = serialized_values;
	} else if (type_name == SdfValueTypeNames->Float2Array || type_name == SdfValueTypeNames->TexCoord2fArray) {
		const VtArray<GfVec2f> values = value.UncheckedGet<VtArray<GfVec2f>>();
		Array serialized_values;
		for (size_t i = 0; i < values.size(); i++) {
			serialized_values.push_back(Vector2(values[i][0], values[i][1]));
		}
		description["typed_value_kind"] = "vector2_array";
		description["typed_value"] = serialized_values;
	} else if (type_name == SdfValueTypeNames->Float3Array || type_name == SdfValueTypeNames->Color3fArray || type_name == SdfValueTypeNames->Normal3fArray || type_name == SdfValueTypeNames->Point3fArray || type_name == SdfValueTypeNames->Vector3fArray) {
		const VtArray<GfVec3f> values = value.UncheckedGet<VtArray<GfVec3f>>();
		Array serialized_values;
		for (size_t i = 0; i < values.size(); i++) {
			serialized_values.push_back(Vector3(values[i][0], values[i][1], values[i][2]));
		}
		description["typed_value_kind"] = "vector3_array";
		description["typed_value"] = serialized_values;
	}

	return description;
}

Dictionary serialize_relationships(const UsdPrim &p_prim) {
	Dictionary relationships;
	UsdRelationshipVector authored_relationships = p_prim.GetAuthoredRelationships();
	for (const UsdRelationship &relationship : authored_relationships) {
		SdfPathVector targets;
		relationship.GetTargets(&targets);

		Array target_paths;
		for (const SdfPath &target : targets) {
			target_paths.push_back(to_godot_string(target.GetString()));
		}

		Dictionary relationship_description;
		relationship_description["is_custom"] = relationship.IsCustom();
		relationship_description["targets"] = target_paths;
		relationships[to_godot_string(relationship.GetName().GetString())] = relationship_description;
	}
	return relationships;
}

Dictionary serialize_reference(const SdfReference &p_reference) {
	Dictionary description;
	description["asset_path"] = to_godot_string(p_reference.GetAssetPath());
	description["prim_path"] = to_godot_string(p_reference.GetPrimPath().GetString());
	description["layer_offset"] = serialize_layer_offset(p_reference.GetLayerOffset());
	if (!p_reference.GetCustomData().empty()) {
		description["custom_data_status"] = "deferred";
		description["custom_data_debug"] = to_godot_string(TfStringify(p_reference.GetCustomData()));
	}
	return description;
}

Dictionary serialize_payload(const SdfPayload &p_payload) {
	Dictionary description;
	description["asset_path"] = to_godot_string(p_payload.GetAssetPath());
	description["prim_path"] = to_godot_string(p_payload.GetPrimPath().GetString());
	description["layer_offset"] = serialize_layer_offset(p_payload.GetLayerOffset());
	return description;
}

Array serialize_path_vector(const SdfPathVector &p_paths) {
	Array paths;
	for (const SdfPath &path : p_paths) {
		paths.push_back(to_godot_string(path.GetString()));
	}
	return paths;
}

Array serialize_authored_references(const UsdPrim &p_prim) {
	Array references;
	std::unordered_set<std::string> seen_references;
	const SdfPrimSpecHandleVector prim_stack = p_prim.GetPrimStack();
	for (const SdfPrimSpecHandle &prim_spec : prim_stack) {
		if (!prim_spec || !prim_spec->HasReferences()) {
			continue;
		}

		const SdfReferenceVector authored_references = prim_spec->GetReferenceList().GetAddedOrExplicitItems();
		for (const SdfReference &reference : authored_references) {
			const std::string key = TfStringPrintf("%s|%s|%.17g|%.17g",
					reference.GetAssetPath().c_str(),
					reference.GetPrimPath().GetString().c_str(),
					reference.GetLayerOffset().GetOffset(),
					reference.GetLayerOffset().GetScale());
			if (seen_references.count(key) != 0) {
				continue;
			}
			seen_references.insert(key);
			references.push_back(serialize_reference(reference));
		}
	}
	return references;
}

Array serialize_authored_payloads(const UsdPrim &p_prim) {
	Array payloads;
	std::unordered_set<std::string> seen_payloads;
	const SdfPrimSpecHandleVector prim_stack = p_prim.GetPrimStack();
	for (const SdfPrimSpecHandle &prim_spec : prim_stack) {
		if (!prim_spec || !prim_spec->HasPayloads()) {
			continue;
		}

		const SdfPayloadVector authored_payloads = prim_spec->GetPayloadList().GetAddedOrExplicitItems();
		for (const SdfPayload &payload : authored_payloads) {
			const std::string key = TfStringPrintf("%s|%s|%.17g|%.17g",
					payload.GetAssetPath().c_str(),
					payload.GetPrimPath().GetString().c_str(),
					payload.GetLayerOffset().GetOffset(),
					payload.GetLayerOffset().GetScale());
			if (seen_payloads.count(key) != 0) {
				continue;
			}
			seen_payloads.insert(key);
			payloads.push_back(serialize_payload(payload));
		}
	}
	return payloads;
}

Array serialize_authored_inherits(const UsdPrim &p_prim) {
	SdfPathVector paths;
	std::unordered_set<std::string> seen_paths;
	const SdfPrimSpecHandleVector prim_stack = p_prim.GetPrimStack();
	for (const SdfPrimSpecHandle &prim_spec : prim_stack) {
		if (!prim_spec || !prim_spec->HasInheritPaths()) {
			continue;
		}

		const SdfPathVector authored_paths = prim_spec->GetInheritPathList().GetAddedOrExplicitItems();
		for (const SdfPath &path : authored_paths) {
			const std::string key = path.GetString();
			if (seen_paths.count(key) != 0) {
				continue;
			}
			seen_paths.insert(key);
			paths.push_back(path);
		}
	}
	return serialize_path_vector(paths);
}

Array serialize_authored_specializes(const UsdPrim &p_prim) {
	SdfPathVector paths;
	std::unordered_set<std::string> seen_paths;
	const SdfPrimSpecHandleVector prim_stack = p_prim.GetPrimStack();
	for (const SdfPrimSpecHandle &prim_spec : prim_stack) {
		if (!prim_spec || !prim_spec->HasSpecializes()) {
			continue;
		}

		const SdfPathVector authored_paths = prim_spec->GetSpecializesList().GetAddedOrExplicitItems();
		for (const SdfPath &path : authored_paths) {
			const std::string key = path.GetString();
			if (seen_paths.count(key) != 0) {
				continue;
			}
			seen_paths.insert(key);
			paths.push_back(path);
		}
	}
	return serialize_path_vector(paths);
}

} // namespace

void store_unmapped_properties(const UsdPrim &p_prim, const UsdTimeCode &p_time, Object *p_target) {
	ERR_FAIL_NULL(p_target);

	Dictionary unmapped_attributes;
	UsdAttributeVector attributes = p_prim.GetAttributes();
	for (const UsdAttribute &attribute : attributes) {
		if (!attribute.IsCustom()) {
			continue;
		}
		unmapped_attributes[to_godot_string(attribute.GetName().GetString())] = serialize_attribute(attribute, p_time);
	}
	if (!unmapped_attributes.is_empty()) {
		set_usd_metadata(p_target, "usd:unmapped_attributes", unmapped_attributes);
	}

	Dictionary unmapped_relationships = serialize_relationships(p_prim);
	if (!unmapped_relationships.is_empty()) {
		set_usd_metadata(p_target, "usd:unmapped_relationships", unmapped_relationships);
	}
}

void store_composition_arcs(const UsdPrim &p_prim, Object *p_target) {
	ERR_FAIL_NULL(p_target);

	const Array serialized_references = serialize_authored_references(p_prim);
	if (!serialized_references.is_empty()) {
		set_usd_metadata(p_target, "usd:references", serialized_references);
		set_usd_metadata(p_target, "usd:composition_preservation_mode", "read_only");
	}

	const Array serialized_payloads = serialize_authored_payloads(p_prim);
	if (!serialized_payloads.is_empty()) {
		set_usd_metadata(p_target, "usd:payloads", serialized_payloads);
		set_usd_metadata(p_target, "usd:composition_preservation_mode", "read_only");
	}

	const Array serialized_inherits = serialize_authored_inherits(p_prim);
	if (!serialized_inherits.is_empty()) {
		set_usd_metadata(p_target, "usd:inherits", serialized_inherits);
		set_usd_metadata(p_target, "usd:composition_preservation_mode", "read_only");
	}

	const Array serialized_specializes = serialize_authored_specializes(p_prim);
	if (!serialized_specializes.is_empty()) {
		set_usd_metadata(p_target, "usd:specializes", serialized_specializes);
		set_usd_metadata(p_target, "usd:composition_preservation_mode", "read_only");
	}
}

} // namespace godot_usd
