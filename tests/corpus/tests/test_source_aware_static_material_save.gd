extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/preview_surface.usda"
	var save_path := "user://source_aware_static_material_saved.usda"
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

	var material := mesh.get_active_material(0) as StandardMaterial3D
	if material == null:
		_fail("Missing imported StandardMaterial3D", old_value)
		return

	material.set_albedo(Color(0.2, 0.8, 0.4, 0.75))
	material.set_metallic(0.05)
	material.set_roughness(0.7)

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save source-aware material USD scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("def Material \"TestMaterial\""), "Saved layer lost original material prim", old_value):
		return
	if not _require(saved_text.contains("inputs:diffuseColor = (0.2, 0.8, 0.4)"), "Saved layer did not merge diffuseColor edit", old_value):
		return
	if not _require(saved_text.contains("inputs:metallic = 0.05"), "Saved layer did not merge metallic edit", old_value):
		return
	if not _require(saved_text.contains("inputs:roughness = 0.7"), "Saved layer did not merge roughness edit", old_value):
		return
	if not _require(saved_text.contains("inputs:opacity = 0.75"), "Saved layer did not merge opacity edit", old_value):
		return
	if not _require(saved_text.count("def Material \"TestMaterial\"") == 1, "Saved layer duplicated material prim", old_value):
		return

	var reloaded := ResourceLoader.load(save_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if reloaded == null or not (reloaded is PackedScene):
		_fail("Failed to reload source-aware material USD scene", old_value)
		return

	root = (reloaded as PackedScene).instantiate()
	get_root().add_child(root)
	mesh = _find_prim_node(root, "/Root/Quad") as MeshInstance3D
	if not _require(mesh != null, "Reloaded scene missing /Root/Quad", old_value):
		return

	var reloaded_material := mesh.get_active_material(0) as BaseMaterial3D
	if not _require(reloaded_material != null, "Reloaded scene missing material", old_value):
		return
	var albedo := reloaded_material.get_albedo()
	if not _require(is_equal_approx(albedo.r, 0.2) and is_equal_approx(albedo.g, 0.8) and is_equal_approx(albedo.b, 0.4), "Reloaded albedo edit mismatch", old_value):
		return
	if not _require(is_equal_approx(albedo.a, 0.75), "Reloaded opacity edit mismatch", old_value):
		return
	if not _require(is_equal_approx(reloaded_material.get_metallic(), 0.05), "Reloaded metallic edit mismatch", old_value):
		return
	if not _require(is_equal_approx(reloaded_material.get_roughness(), 0.7), "Reloaded roughness edit mismatch", old_value):
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
