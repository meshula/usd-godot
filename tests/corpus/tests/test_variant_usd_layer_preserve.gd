extends SceneTree

var failed := false

func _fail(message: String) -> void:
	failed = true
	push_error(message)

func _require(condition: bool, message: String) -> void:
	if not condition:
		_fail(message)

func _save_instantiated_scene(root: Node, path: String) -> int:
	var saved_scene := PackedScene.new()
	var pack_error := saved_scene.pack(root)
	if pack_error != OK:
		return pack_error
	return ResourceSaver.save(saved_scene, path)

func _init() -> void:
	var unchanged_path := "user://variant_stage_unchanged.usda"
	var edited_path := "user://variant_stage_edited.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(unchanged_path))
	DirAccess.remove_absolute(ProjectSettings.globalize_path(edited_path))

	var packed_scene: PackedScene = ResourceLoader.load("res://samples/variant_stage.usda", "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	_require(packed_scene != null, "Failed to load variant USD layer as PackedScene.")

	var root := packed_scene.instantiate()
	_require(root is UsdStageInstance, "Variant USD layer did not instantiate as a UsdStageInstance root.")
	_require(_save_instantiated_scene(root, unchanged_path) == OK, "Saving an unchanged source USD layer should preserve the layer.")
	root.free()

	var source_text := FileAccess.get_file_as_string("res://samples/variant_stage.usda")
	var saved_text := FileAccess.get_file_as_string(unchanged_path)
	_require(source_text == saved_text, "Unchanged USD layer save did not preserve the layer contents.")

	root = packed_scene.instantiate()
	_require(root is UsdStageInstance, "Variant USD layer did not instantiate as a UsdStageInstance root for edited save.")
	root.set("variants/Model/modelingVariant", "blue")
	root.set("variants/Model/Nested/detail", "sphere")
	_require(_save_instantiated_scene(root, edited_path) == OK, "Edited source USD layer variant save should author the default selections.")
	root.free()

	var edited_text := FileAccess.get_file_as_string(edited_path)
	_require(edited_text.begins_with("#usda"), "Edited USD layer should remain a text USDA layer.")
	_require(edited_text.contains("string modelingVariant = \"blue\""), "Edited USD layer did not author the selected model variant.")
	_require(edited_text.contains("string detail = \"sphere\""), "Edited USD layer did not author the selected nested variant.")
	_require(edited_text.contains("RedCube") and edited_text.contains("BlueSphere"), "Edited USD layer lost inactive variant content.")

	var edited_packed_scene: PackedScene = ResourceLoader.load(edited_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	_require(edited_packed_scene != null, "Failed to reload edited USD layer as PackedScene.")
	root = edited_packed_scene.instantiate()
	_require(root is UsdStageInstance, "Edited USD layer did not instantiate as a UsdStageInstance root.")
	(root as UsdStageInstance).rebuild()
	_require(root.get("variants/Model/modelingVariant") == "blue", "Edited USD layer did not load with blue as the default model variant.")
	_require(root.get("variants/Model/Nested/detail") == "sphere", "Edited USD layer did not load with sphere as the nested default variant.")
	_require(root.get_node_for_prim_path("/Model/BlueSphere") != null, "Edited USD layer did not load the blue branch.")
	_require(root.get_node_for_prim_path("/Model/RedCube") == null, "Edited USD layer still loaded the red branch.")
	_require(root.get_node_for_prim_path("/Model/Nested/NestedSphere") != null, "Edited USD layer did not load the nested sphere branch.")
	_require(root.get_node_for_prim_path("/Model/Nested/NestedCube") == null, "Edited USD layer still loaded the nested cube branch.")
	root.free()

	quit(1 if failed else 0)
