extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/textured_preview.usda"
	var save_path := "user://source_aware_static_texture_network_saved.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))

	var loaded := ResourceLoader.load(source_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if loaded == null or not (loaded is PackedScene):
		_fail("Failed to load textured_preview.usda", old_value)
		return

	var root := (loaded as PackedScene).instantiate()
	get_root().add_child(root)

	var mesh_instance := _find_prim_node(root, "/TexturedQuad") as MeshInstance3D
	if mesh_instance == null:
		_fail("Missing /TexturedQuad mesh", old_value)
		return

	var material := mesh_instance.get_active_material(0) as StandardMaterial3D
	if material == null:
		_fail("Missing imported textured material", old_value)
		return
	material.albedo_texture = null
	material.albedo_color = Color(0.1, 0.9, 0.1, 1.0)

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save unsupported texture-network edit scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("inputs:diffuseColor.connect"), "Saved layer lost original diffuse texture connection", old_value):
		return
	if not _require(saved_text.contains("@../images/icon.png@"), "Saved layer lost original texture asset path", old_value):
		return
	if not _require(not saved_text.contains("(0.1, 0.9, 0.1)"), "Unsupported texture-network edit should not be authored as a scalar replacement", old_value):
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
