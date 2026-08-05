extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/preview_surface.usda"
	var save_path := "user://source_aware_static_material_rebind_saved.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))

	var loaded := ResourceLoader.load(source_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if loaded == null or not (loaded is PackedScene):
		_fail("Failed to load preview_surface.usda", old_value)
		return

	var root := (loaded as PackedScene).instantiate()
	get_root().add_child(root)

	var mesh := _find_prim_node(root, "/Root/Quad") as MeshInstance3D
	if mesh == null:
		_fail("Missing /Root/Quad mesh", old_value)
		return

	var replacement := StandardMaterial3D.new()
	replacement.albedo_color = Color(0.05, 0.95, 0.2, 1.0)
	replacement.roughness = 0.9
	mesh.material_override = replacement

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save unsupported material rebind scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("def Material \"TestMaterial\""), "Saved layer lost original material prim", old_value):
		return
	if not _require(not saved_text.contains("inputs:diffuseColor = (0.05, 0.95, 0.2)"), "Unsupported material replacement should not overwrite source material", old_value):
		return
	if not _require(saved_text.contains("inputs:diffuseColor = (0.9, 0.2, 0.1)"), "Source material value should be preserved after unsupported rebind", old_value):
		return

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
