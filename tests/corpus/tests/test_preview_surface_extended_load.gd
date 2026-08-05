extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var packed := load("res://samples/preview_surface_extended.usda")
	if packed == null or not (packed is PackedScene):
		_fail("Failed to load extended PreviewSurface fixture", old_value)
		return

	var root := (packed as PackedScene).instantiate()
	get_root().add_child(root)

	var mesh_instance := _find_prim_node(root, "/PreviewSurfaceExtended/Quad") as MeshInstance3D
	if mesh_instance == null or mesh_instance.mesh == null:
		_fail("Failed to find /PreviewSurfaceExtended/Quad", old_value)
		return

	var material := mesh_instance.mesh.surface_get_material(0) as BaseMaterial3D
	if material == null:
		_fail("Extended PreviewSurface material is missing", old_value)
		return

	if not _require(is_equal_approx(material.get_specular(), 0.5), "Specular workflow approximation mismatch", old_value):
		return
	if not _require(is_equal_approx(material.get_roughness(), 0.45), "Roughness mismatch", old_value):
		return
	if not _require(material.get_transparency() == BaseMaterial3D.TRANSPARENCY_ALPHA, "Opacity texture did not enable alpha", old_value):
		return
	if not _require(material.get_feature(BaseMaterial3D.FEATURE_CLEARCOAT), "Clearcoat feature missing", old_value):
		return
	if not _require(is_equal_approx(material.get_clearcoat(), 0.6), "Clearcoat value mismatch", old_value):
		return
	if not _require(is_equal_approx(material.get_clearcoat_roughness(), 0.2), "Clearcoat roughness mismatch", old_value):
		return
	if not _require(material.get_texture(BaseMaterial3D.TEXTURE_CLEARCOAT) != null, "Clearcoat texture missing", old_value):
		return
	if not _require(material.get_feature(BaseMaterial3D.FEATURE_AMBIENT_OCCLUSION), "AO feature missing", old_value):
		return
	if not _require(material.get_texture(BaseMaterial3D.TEXTURE_AMBIENT_OCCLUSION) != null, "AO texture missing", old_value):
		return
	if not _require(material.get_ao_texture_channel() == BaseMaterial3D.TEXTURE_CHANNEL_BLUE, "AO channel mismatch", old_value):
		return

	var metadata := material.get_meta("usd", {}) as Dictionary
	if not _require(bool(metadata.get("usd:preview_surface_use_specular_workflow", false)), "Specular workflow metadata missing", old_value):
		return
	if not _require(is_equal_approx(float(metadata.get("usd:preview_surface_ior", 0.0)), 1.5), "IOR metadata mismatch", old_value):
		return
	if not _require(metadata.has("usd:preview_surface_specular_color"), "Specular color metadata missing", old_value):
		return
	var texture_sources := metadata.get("usd:preview_surface_texture_sources", {}) as Dictionary
	for key in ["opacity", "clearcoat", "clearcoatRoughness", "occlusion"]:
		if not _require(texture_sources.has(key), "Texture source metadata missing %s" % key, old_value):
			return

	print("Extended PreviewSurface import verified.")
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
