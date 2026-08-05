extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/preview_surface.usda"
	var save_path := "user://source_aware_static_mesh_points_saved.usda"
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
	var arrays := mesh_instance.mesh.surface_get_arrays(0)
	var vertices := arrays[Mesh.ARRAY_VERTEX] as PackedVector3Array
	for i in vertices.size():
		if vertices[i].is_equal_approx(Vector3(-1, -1, 0)):
			vertices[i] = Vector3(-1, -1, 0.5)
	arrays[Mesh.ARRAY_VERTEX] = vertices

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
		_fail("Failed to save source-aware mesh points USD scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("def Mesh \"Quad\""), "Saved layer lost original Mesh prim", old_value):
		return
	if not _require(saved_text.contains("(-1, -1, 0.5)"), "Saved layer did not merge edited point", old_value):
		return
	if not _require(saved_text.count("def Mesh \"Quad\"") == 1, "Saved layer duplicated Mesh prim", old_value):
		return

	var reloaded := ResourceLoader.load(save_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if reloaded == null or not (reloaded is PackedScene):
		_fail("Failed to reload source-aware mesh points USD scene", old_value)
		return

	root = (reloaded as PackedScene).instantiate()
	get_root().add_child(root)
	mesh_instance = _find_prim_node(root, "/Root/Quad") as MeshInstance3D
	if not _require(mesh_instance != null and mesh_instance.mesh != null, "Reloaded scene missing /Root/Quad mesh", old_value):
		return

	arrays = mesh_instance.mesh.surface_get_arrays(0)
	vertices = arrays[Mesh.ARRAY_VERTEX] as PackedVector3Array
	var found_edited_point := false
	for vertex in vertices:
		if vertex.is_equal_approx(Vector3(-1, -1, 0.5)):
			found_edited_point = true
			break
	if not _require(found_edited_point, "Reloaded mesh point edit mismatch", old_value):
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
