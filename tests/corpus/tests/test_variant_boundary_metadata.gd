extends SceneTree

var failed := false

func _fail(message: String) -> void:
	failed = true
	push_error(message)

func _require(condition: bool, message: String) -> void:
	if not condition:
		_fail(message)

func _usd_metadata(node: Node) -> Dictionary:
	var metadata = node.get_meta("usd", {})
	if metadata is Dictionary:
		return metadata
	return {}

func _context_has(context: Array, prim_path: String, variant_set: String, selection: String) -> bool:
	for entry in context:
		if not entry is Dictionary:
			continue
		if entry.get("prim_path", "") == prim_path and entry.get("variant_set", "") == variant_set and entry.get("selection", "") == selection:
			return true
	return false

func _init() -> void:
	var packed_scene: PackedScene = ResourceLoader.load("res://samples/variant_stage.usda", "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	_require(packed_scene != null, "Failed to load variant USD layer as PackedScene.")

	var root := packed_scene.instantiate()
	_require(root is UsdStageInstance, "Variant USD layer did not instantiate as a UsdStageInstance root.")
	var stage_instance := root as UsdStageInstance
	stage_instance.set("variants/Model/modelingVariant", "blue")

	var model := stage_instance.get_node_for_prim_path("/Model")
	var blue_sphere := stage_instance.get_node_for_prim_path("/Model/BlueSphere")
	var nested := stage_instance.get_node_for_prim_path("/Model/Nested")
	var nested_cube := stage_instance.get_node_for_prim_path("/Model/Nested/NestedCube")
	_require(model != null, "Model node was not generated.")
	_require(blue_sphere != null, "BlueSphere node was not generated after selecting the blue variant.")
	_require(nested != null, "Nested node was not generated after selecting the blue variant.")
	_require(nested_cube != null, "NestedCube node was not generated with the nested default variant.")
	_require(stage_instance.get_node_for_prim_path("/Model/RedCube") == null, "RedCube should not be generated after selecting the blue variant.")

	var model_metadata := _usd_metadata(model)
	_require(model_metadata.get("usd:variant_boundary", false), "Model should be marked as a variant boundary.")
	_require(model_metadata.get("usd:variant_sets", {}).has("modelingVariant"), "Model should record its modelingVariant set.")
	_require(_context_has(model_metadata.get("usd:variant_context", []), "/Model", "modelingVariant", "blue"), "Model should record the selected blue modelingVariant context.")

	var blue_metadata := _usd_metadata(blue_sphere)
	_require(not blue_metadata.get("usd:variant_boundary", false), "BlueSphere should not be marked as a local variant boundary.")
	_require(_context_has(blue_metadata.get("usd:variant_context", []), "/Model", "modelingVariant", "blue"), "BlueSphere should record that it came from the selected blue modelingVariant context.")

	var nested_metadata := _usd_metadata(nested)
	_require(nested_metadata.get("usd:variant_boundary", false), "Nested should be marked as a nested variant boundary.")
	_require(nested_metadata.get("usd:variant_sets", {}).has("detail"), "Nested should record its detail variant set.")
	_require(_context_has(nested_metadata.get("usd:variant_context", []), "/Model", "modelingVariant", "blue"), "Nested should inherit the selected blue modelingVariant context.")
	_require(_context_has(nested_metadata.get("usd:variant_context", []), "/Model/Nested", "detail", "cube"), "Nested should record its selected detail context.")

	var nested_cube_metadata := _usd_metadata(nested_cube)
	_require(_context_has(nested_cube_metadata.get("usd:variant_context", []), "/Model", "modelingVariant", "blue"), "NestedCube should inherit the selected blue modelingVariant context.")
	_require(_context_has(nested_cube_metadata.get("usd:variant_context", []), "/Model/Nested", "detail", "cube"), "NestedCube should record the selected nested detail context.")

	root.free()
	quit(1 if failed else 0)
