extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/preview_surface.usda"
	var save_path := "user://source_aware_static_mesh_unsupported_saved.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))

	var loaded := ResourceLoader.load(source_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if loaded == null or not (loaded is PackedScene):
		_fail("Failed to load preview_surface.usda", old_value)
		return

	var root := (loaded as PackedScene).instantiate()
	get_root().add_child(root)

	var mesh_instance := _find_prim_node(root, "/Root/Quad") as MeshInstance3D
	if mesh_instance == null or mesh_instance.mesh == null:
		_fail("Missing /Root/Quad mesh", old_value)
		return

	var old_material := mesh_instance.get_active_material(0)
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	var vertices := PackedVector3Array([
		Vector3(-1, -1, 0),
		Vector3(1, -1, 0),
		Vector3(0, 1, 2),
	])
	var normals := PackedVector3Array([
		Vector3(0, 0, 1),
		Vector3(0, 0, 1),
		Vector3(0, 0, 1),
	])
	var uvs := PackedVector2Array([
		Vector2(0, 0),
		Vector2(1, 0),
		Vector2(0.5, 1),
	])
	var indices := PackedInt32Array([0, 1, 2])
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_INDEX] = indices

	var edited_mesh := ArrayMesh.new()
	edited_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	if old_material != null:
		edited_mesh.surface_set_material(0, old_material)
	mesh_instance.mesh = edited_mesh

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save unsupported static mesh edit scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("def Mesh \"Quad\""), "Saved layer lost original Mesh prim", old_value):
		return
	if not _require(not saved_text.contains("(0, 0, 2)"), "Unsupported topology edit should not be partially merged", old_value):
		return
	if not _require(saved_text.count("def Mesh \"Quad\"") == 1, "Saved layer duplicated Mesh prim", old_value):
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
