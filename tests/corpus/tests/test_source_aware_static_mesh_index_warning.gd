extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/preview_surface.usda"
	var save_path := "user://source_aware_static_mesh_index_saved.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))

	var loaded := ResourceLoader.load(source_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if loaded == null or not (loaded is PackedScene):
		_fail("Failed to load preview_surface.usda", old_value)
		return

	var root := (loaded as PackedScene).instantiate()
	get_root().add_child(root)

	var mesh_instance := _find_prim_node(root, "/Root/Quad") as MeshInstance3D
	if mesh_instance == null or not (mesh_instance.mesh is ArrayMesh):
		_fail("Missing /Root/Quad ArrayMesh", old_value)
		return

	var source_mesh := mesh_instance.mesh as ArrayMesh
	var arrays := source_mesh.surface_get_arrays(0)
	var indices := arrays[Mesh.ARRAY_INDEX] as PackedInt32Array
	if indices.size() < 3:
		_fail("Source mesh did not import triangle indices", old_value)
		return
	var first := indices[1]
	indices.set(1, indices[2])
	indices.set(2, first)
	arrays[Mesh.ARRAY_INDEX] = indices

	var edited_mesh := ArrayMesh.new()
	edited_mesh.add_surface_from_arrays(source_mesh.surface_get_primitive_type(0), arrays)
	edited_mesh.surface_set_material(0, source_mesh.surface_get_material(0))
	mesh_instance.mesh = edited_mesh

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save unsupported index edit scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("int[] faceVertexIndices = [0, 1, 2, 3]"), "Original source faceVertexIndices should be preserved", old_value):
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
