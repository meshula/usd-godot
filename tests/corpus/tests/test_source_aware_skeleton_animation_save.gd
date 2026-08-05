extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/skeleton_external_animation.usda"
	var save_path := "user://source_aware_skeleton_external_saved.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))

	var loaded := ResourceLoader.load(source_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if loaded == null or not (loaded is PackedScene):
		_fail("Failed to load skeleton_external_animation.usda", old_value)
		return

	var root := (loaded as PackedScene).instantiate()
	get_root().add_child(root)

	var skeleton := _find_prim_node(root, "/Model/Skel") as Skeleton3D
	var player := _find_animation_player(root)
	if skeleton == null or player == null:
		_fail("Missing skeleton or AnimationPlayer in source scene", old_value)
		return

	var elbow_rest := skeleton.get_bone_rest(1)
	elbow_rest.origin.z = 3.0
	skeleton.set_bone_rest(1, elbow_rest)

	if not _require(player.has_animation(&"ElbowAnim"), "Missing ElbowAnim clip", old_value):
		return
	var animation := player.get_animation(&"ElbowAnim")
	if not _require(animation.get_track_count() == 1, "Unexpected ElbowAnim track count", old_value):
		return
	if not _require(animation.track_get_key_count(0) == 2, "Unexpected ElbowAnim key count", old_value):
		return
	animation.track_set_key_value(0, 1, Quaternion(1, 0, 0, 0))

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save source-aware edited USD scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("def Xform \"Animations\""), "Saved layer lost Animations prim", old_value):
		return
	if not _require(saved_text.contains("def Skeleton \"Skel\""), "Saved layer lost Skeleton prim", old_value):
		return
	if not _require(saved_text.contains("def SkelAnimation \"ElbowAnim\""), "Saved layer lost SkelAnimation prim", old_value):
		return
	if not _require(saved_text.contains("rel skel:animationSource = </Animations/ElbowAnim>"), "Saved layer lost animation relationship", old_value):
		return
	if not _require(saved_text.contains("10: [(0, 1, 0, 0)]"), "Saved layer did not merge edited rotation key", old_value):
		return
	if not _require(saved_text.contains("(0, 0, 3, 1)"), "Saved layer did not merge edited skeleton rest transform", old_value):
		return
	if not _require(saved_text.count("def SkelAnimation \"ElbowAnim\"") == 1, "Saved layer duplicated SkelAnimation", old_value):
		return
	if not _require(saved_text.count("def Skeleton \"Skel\"") == 1, "Saved layer duplicated Skeleton", old_value):
		return

	var reloaded := ResourceLoader.load(save_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if reloaded == null or not (reloaded is PackedScene):
		_fail("Failed to reload source-aware saved USD scene", old_value)
		return

	root = (reloaded as PackedScene).instantiate()
	get_root().add_child(root)
	skeleton = _find_prim_node(root, "/Model/Skel") as Skeleton3D
	if not _require(skeleton != null, "Reloaded scene missing skeleton", old_value):
		return
	if not _require(is_equal_approx(skeleton.get_bone_rest(1).origin.z, 3.0), "Reloaded skeleton rest edit mismatch", old_value):
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

func _find_animation_player(node: Node) -> AnimationPlayer:
	if node is AnimationPlayer:
		return node as AnimationPlayer
	for child in node.get_children():
		var found := _find_animation_player(child)
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
