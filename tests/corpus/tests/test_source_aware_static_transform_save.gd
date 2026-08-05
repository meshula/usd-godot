extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/cube.usda"
	var save_path := "user://source_aware_static_transform_saved.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))

	var loaded := ResourceLoader.load(source_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if loaded == null or not (loaded is PackedScene):
		_fail("Failed to load cube.usda", old_value)
		return

	var root := (loaded as PackedScene).instantiate()
	get_root().add_child(root)

	var cube := _find_prim_node(root, "/Root/Cube") as Node3D
	if cube == null:
		_fail("Missing /Root/Cube node", old_value)
		return

	cube.transform = Transform3D(Basis.IDENTITY.scaled(Vector3(1.5, 2.0, 0.5)), Vector3(3.0, 4.0, 5.0))

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save source-aware transform USD scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("def Cube \"Cube\""), "Saved layer lost Cube prim", old_value):
		return
	if not _require(saved_text.contains("xformOp:translate") and saved_text.contains("(3, 4, 5)"), "Saved layer did not merge edited translation", old_value):
		return
	if not _require(saved_text.contains("xformOp:scale") and saved_text.contains("(1.5, 2, 0.5)"), "Saved layer did not merge edited scale", old_value):
		return
	if not _require(saved_text.count("def Cube \"Cube\"") == 1, "Saved layer duplicated Cube prim", old_value):
		return

	var reloaded := ResourceLoader.load(save_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if reloaded == null or not (reloaded is PackedScene):
		_fail("Failed to reload source-aware transform USD scene", old_value)
		return

	root = (reloaded as PackedScene).instantiate()
	get_root().add_child(root)
	cube = _find_prim_node(root, "/Root/Cube") as Node3D
	if not _require(cube != null, "Reloaded scene missing /Root/Cube", old_value):
		return
	if not _require(cube.transform.origin.is_equal_approx(Vector3(3.0, 4.0, 5.0)), "Reloaded transform origin mismatch", old_value):
		return
	if not _require(cube.transform.basis.get_scale().is_equal_approx(Vector3(1.5, 2.0, 0.5)), "Reloaded transform scale mismatch", old_value):
		return

	root.free()
	ProjectSettings.set_setting(SETTING, old_value)
	await process_frame
	quit(0)

func _find_prim_node(node: Node, prim_path: String) -> Node:
	if node.has_meta("usd"):
		var meta := node.get_meta("usd", {}) as Dictionary
		if meta.get("usd:prim_path", "") == prim_path:
			return node
	for child in node.get_children():
		var found := _find_prim_node(child, prim_path)
		if found != null:
			return found
	return null

func _require(condition: bool, message: String, old_value: Variant) -> bool:
	if condition:
		return true
	_fail(message, old_value)
	return false

func _fail(message: String, old_value: Variant) -> void:
	push_error(message)
	ProjectSettings.set_setting(SETTING, old_value)
	quit(1)
