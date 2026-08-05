extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	if not _check_winding(old_value):
		return
	if not _check_face_varying_normals(old_value):
		return

	print("Mesh winding and face-varying normal import verified.")
	ProjectSettings.set_setting(SETTING, old_value)
	await process_frame
	quit(0)

func _check_winding(old_value: Variant) -> bool:
	var packed := load("res://samples/winding_right_handed.usda")
	if packed == null or not (packed is PackedScene):
		_fail("Failed to load winding fixture", old_value)
		return false

	var root := (packed as PackedScene).instantiate()
	get_root().add_child(root)
	var mesh_instance := _find_prim_node(root, "/WindingTriangle") as MeshInstance3D
	if mesh_instance == null or mesh_instance.mesh == null:
		_fail("Failed to find /WindingTriangle", old_value)
		return false

	var arrays := mesh_instance.mesh.surface_get_arrays(0)
	var vertices := arrays[Mesh.ARRAY_VERTEX] as PackedVector3Array
	if vertices.size() < 3:
		_fail("Winding mesh did not import at least one triangle", old_value)
		return false

	var face_normal := (vertices[1] - vertices[0]).cross(vertices[2] - vertices[0]).normalized()
	if not _require(face_normal.z < 0.0, "Right-handed USD winding was not preserved", old_value):
		return false

	root.free()
	return true

func _check_face_varying_normals(old_value: Variant) -> bool:
	var packed := load("res://samples/primvar_normals.usda")
	if packed == null or not (packed is PackedScene):
		_fail("Failed to load primvar normals fixture", old_value)
		return false

	var root := (packed as PackedScene).instantiate()
	get_root().add_child(root)
	var mesh_instance := _find_prim_node(root, "/Root/Quad") as MeshInstance3D
	if mesh_instance == null or mesh_instance.mesh == null:
		_fail("Failed to find /Root/Quad", old_value)
		return false

	var arrays := mesh_instance.mesh.surface_get_arrays(0)
	var vertices := arrays[Mesh.ARRAY_VERTEX] as PackedVector3Array
	var normals := arrays[Mesh.ARRAY_NORMAL] as PackedVector3Array
	if not _require(vertices.size() == 6, "Primvar normal mesh vertex count mismatch", old_value):
		return false
	if not _require(normals.size() == 6, "Primvar normal count mismatch", old_value):
		return false
	if not _require(is_equal_approx(normals[0].z, 1.0) and is_equal_approx(normals[5].z, 1.0), "Face-varying normals were not preserved", old_value):
		return false

	root.free()
	return true

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
