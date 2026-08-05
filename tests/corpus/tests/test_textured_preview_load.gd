extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	if not _check_textured_scene("res://samples/textured_preview.usda", "/TexturedQuad"):
		ProjectSettings.set_setting(SETTING, old_value)
		quit(1)
		return
	if not _check_textured_scene("res://samples/packaged_preview.usdz", "/PackagedQuad"):
		ProjectSettings.set_setting(SETTING, old_value)
		quit(1)
		return

	print("Textured PreviewSurface and packaged USDZ texture import verified.")
	ProjectSettings.set_setting(SETTING, old_value)
	await process_frame
	quit(0)

func _check_textured_scene(path: String, prim_path: String) -> bool:
	var packed := load(path)
	if packed == null or not (packed is PackedScene):
		push_error("Failed to load %s" % path)
		return false

	var root := (packed as PackedScene).instantiate()
	get_root().add_child(root)
	var mesh_instance := _find_prim_node(root, prim_path) as MeshInstance3D
	if mesh_instance == null:
		root.free()
		push_error("Failed to find %s in %s" % [prim_path, path])
		return false
	if mesh_instance.mesh == null or mesh_instance.mesh.get_surface_count() != 1:
		root.free()
		push_error("Textured mesh surface count mismatch in %s" % path)
		return false

	var material := mesh_instance.mesh.surface_get_material(0) as BaseMaterial3D
	if material == null or material.get_texture(BaseMaterial3D.TEXTURE_ALBEDO) == null:
		root.free()
		push_error("Missing imported albedo texture in %s" % path)
		return false

	var metadata := mesh_instance.get_meta("usd", {}) as Dictionary
	var bindings := metadata.get("usd:material_bindings", []) as Array
	if bindings.size() != 1:
		root.free()
		push_error("Material binding metadata mismatch in %s" % path)
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
