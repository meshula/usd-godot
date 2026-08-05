extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var packed := load("res://samples/preview_texture_channels.usda")
	if packed == null or not (packed is PackedScene):
		_fail("Failed to load preview texture channel fixture", old_value)
		return

	var root := (packed as PackedScene).instantiate()
	get_root().add_child(root)

	var mesh_instance := _find_prim_node(root, "/PreviewChannels/Quad") as MeshInstance3D
	if mesh_instance == null:
		_fail("Failed to find /PreviewChannels/Quad", old_value)
		return
	if not _require(mesh_instance.mesh != null, "Preview texture channel mesh is missing", old_value):
		return

	var material := mesh_instance.mesh.surface_get_material(0) as BaseMaterial3D
	if material == null:
		_fail("Preview texture channel material is missing", old_value)
		return

	if not _require(material.get_texture(BaseMaterial3D.TEXTURE_ALBEDO) != null, "Albedo texture missing", old_value):
		return
	if not _require(material.get_texture(BaseMaterial3D.TEXTURE_METALLIC) != null, "Metallic texture missing", old_value):
		return
	if not _require(material.get_texture(BaseMaterial3D.TEXTURE_ROUGHNESS) != null, "Roughness texture missing", old_value):
		return
	if not _require(material.get_texture(BaseMaterial3D.TEXTURE_NORMAL) != null, "Normal texture missing", old_value):
		return
	if not _require(material.get_feature(BaseMaterial3D.FEATURE_NORMAL_MAPPING), "Normal mapping feature missing", old_value):
		return
	if not _require(is_equal_approx(material.get_normal_scale(), 1.0), "Normal scale mismatch", old_value):
		return
	if not _require(material.get_metallic_texture_channel() == BaseMaterial3D.TEXTURE_CHANNEL_GREEN, "Metallic channel mismatch", old_value):
		return
	if not _require(material.get_roughness_texture_channel() == BaseMaterial3D.TEXTURE_CHANNEL_BLUE, "Roughness channel mismatch", old_value):
		return
	if not _require(material.get_transparency() == BaseMaterial3D.TRANSPARENCY_ALPHA, "Opacity texture did not enable alpha transparency", old_value):
		return

	var uv_scale := material.get_uv1_scale()
	var uv_offset := material.get_uv1_offset()
	if not _require(is_equal_approx(uv_scale.x, 2.0) and is_equal_approx(uv_scale.y, 3.0), "UV transform scale mismatch", old_value):
		return
	if not _require(is_equal_approx(uv_offset.x, 0.25) and is_equal_approx(uv_offset.y, -2.1), "UV transform offset mismatch", old_value):
		return

	var albedo_image := material.get_texture(BaseMaterial3D.TEXTURE_ALBEDO).get_image()
	if not _require(albedo_image != null and albedo_image.detect_alpha() != Image.ALPHA_NONE, "Albedo opacity composition lost alpha", old_value):
		return

	print("Preview texture channels and UV transform import verified.")
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
