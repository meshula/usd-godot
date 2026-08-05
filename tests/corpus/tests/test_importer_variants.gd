extends SceneTree

var failed := false

func _initialize() -> void:
	call_deferred("_run_test")

func _fail(message: String) -> void:
	failed = true
	push_error(message)

func _require(condition: bool, message: String) -> void:
	if not condition:
		_fail(message)

func _find_option(options: Array, name: String) -> Dictionary:
	for option in options:
		if option is Dictionary and option.get("name", "") == name:
			return option
	return {}

func _usd_metadata(node: Node) -> Dictionary:
	var metadata = node.get_meta("usd", {})
	if metadata is Dictionary:
		return metadata
	return {}

func _find_prim_node(node: Node, prim_path: String) -> Node:
	if _usd_metadata(node).get("usd:prim_path", "") == prim_path:
		return node
	for child in node.get_children():
		var found := _find_prim_node(child, prim_path)
		if found != null:
			return found
	return null

func _cleanup_node(node: Node) -> void:
	if node != null:
		node.free()
	await process_frame

func _run_test() -> void:
	var usd_path := "res://samples/variant_stage.usda"
	var importer := UsdSceneFormatImporter.new()
	_require(importer != null, "UsdSceneFormatImporter was not registered.")

	var options := importer.get_import_options_snapshot(usd_path)
	var legacy := _find_option(options, "usd/variant_selections")
	_require(not legacy.is_empty(), "Importer did not expose the legacy variant JSON option.")
	_require((int(legacy.get("usage", 0)) & PROPERTY_USAGE_NO_EDITOR) != 0, "Legacy variant JSON option should be hidden from the editor.")
	_require(importer.get_option_visibility(usd_path, "usd/variant_selections") == false, "Legacy variant JSON option should report invisible.")

	var modeling := _find_option(options, "usd/variants/Model/modelingVariant")
	_require(not modeling.is_empty(), "Importer did not expose the Model modelingVariant option.")
	_require(int(modeling.get("type", -1)) == TYPE_STRING, "Model modelingVariant option should be a string.")
	_require(int(modeling.get("hint", -1)) == PROPERTY_HINT_ENUM, "Model modelingVariant option should be an enum.")
	_require(String(modeling.get("hint_string", "")).contains("red"), "Model modelingVariant option should include red.")
	_require(String(modeling.get("hint_string", "")).contains("blue"), "Model modelingVariant option should include blue.")
	_require(String(modeling.get("default_value", "")) == "red", "Model modelingVariant default should be red.")

	var detail := _find_option(options, "usd/variants/Model/Nested/detail")
	_require(not detail.is_empty(), "Importer did not expose the nested detail option.")
	_require(String(detail.get("hint_string", "")).contains("cube"), "Nested detail option should include cube.")
	_require(String(detail.get("hint_string", "")).contains("sphere"), "Nested detail option should include sphere.")

	var root := importer.import_scene(usd_path, {})
	_require(root != null, "Importer failed to bake the default variant stage.")
	_require(not root is UsdStageInstance, "Importer should bake a static scene root, not return a live UsdStageInstance.")
	_require(root.get_node_or_null("_Generated") == null, "Importer should not return the generated staging root.")
	_require(_find_prim_node(root, "/Model/RedCube") != null, "Default importer bake should include RedCube.")
	_require(_find_prim_node(root, "/Model/BlueSphere") == null, "Default importer bake should not include BlueSphere.")
	await _cleanup_node(root)

	var overridden_options := {
		"usd/variant_selections": "{\"/Model\":{\"modelingVariant\":\"red\"},\"/Model/Nested\":{\"detail\":\"cube\"}}",
		"usd/variants/Model/modelingVariant": "blue",
		"usd/variants/Model/Nested/detail": "sphere",
	}
	root = importer.import_scene(usd_path, overridden_options)
	_require(root != null, "Importer failed to bake the structured variant override stage.")
	_require(not root is UsdStageInstance, "Structured importer bake should return a static scene root.")
	_require(_find_prim_node(root, "/Model/RedCube") == null, "Structured option should override legacy JSON and hide RedCube.")
	_require(_find_prim_node(root, "/Model/BlueSphere") != null, "Structured option should override legacy JSON and include BlueSphere.")
	_require(_find_prim_node(root, "/Model/Nested/NestedSphere") != null, "Structured nested option should include NestedSphere.")
	_require(_find_prim_node(root, "/Model/Nested/NestedCube") == null, "Structured nested option should hide NestedCube.")
	_require(not root.has_meta("usd:importer_variant_selections"), "Importer should not attach legacy importer variant metadata to the static root.")
	await _cleanup_node(root)

	quit(1 if failed else 0)
