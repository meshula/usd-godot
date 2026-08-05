extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/skeleton_multi_anim_blendshape.usda"
	var save_path := "user://source_aware_blend_shape_weight_saved.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))

	var loaded := ResourceLoader.load(source_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if loaded == null or not (loaded is PackedScene):
		_fail("Failed to load skeleton_multi_anim_blendshape.usda", old_value)
		return

	var root := (loaded as PackedScene).instantiate()
	get_root().add_child(root)

	var player := _find_animation_player(root)
	if player == null or not player.has_animation(&"Combo"):
		_fail("Missing Combo animation", old_value)
		return

	var animation := player.get_animation(&"Combo")
	var blend_track := _find_blend_shape_track(animation, "root/Actor/Body:Smile")
	if not _require(blend_track >= 0, "Missing Smile blend-shape track", old_value):
		return
	if not _require(animation.track_get_key_count(blend_track) == 3, "Unexpected Smile key count", old_value):
		return

	animation.track_set_key_value(blend_track, 1, 0.4)

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save source-aware blend-shape USD scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("def SkelAnimation \"Combo\""), "Saved layer lost Combo animation prim", old_value):
		return
	if not _require(saved_text.contains("10: [0.4]"), "Saved layer did not merge edited blend-shape weight", old_value):
		return
	if not _require(saved_text.count("def SkelAnimation \"Combo\"") == 1, "Saved layer duplicated Combo animation prim", old_value):
		return

	var reloaded := ResourceLoader.load(save_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if reloaded == null or not (reloaded is PackedScene):
		_fail("Failed to reload source-aware blend-shape USD scene", old_value)
		return

	root = (reloaded as PackedScene).instantiate()
	get_root().add_child(root)
	player = _find_animation_player(root)
	if not _require(player != null and player.has_animation(&"Combo"), "Reloaded scene missing Combo", old_value):
		return
	animation = player.get_animation(&"Combo")
	blend_track = _find_blend_shape_track(animation, "root/Actor/Body:Smile")
	if not _require(blend_track >= 0 and is_equal_approx(float(animation.track_get_key_value(blend_track, 1)), 0.4), "Reloaded blend-shape edit mismatch", old_value):
		return

	root.free()
	ProjectSettings.set_setting(SETTING, old_value)
	await process_frame
	quit(0)

func _find_animation_player(node: Node) -> AnimationPlayer:
	if node is AnimationPlayer:
		return node as AnimationPlayer
	for child in node.get_children():
		var found := _find_animation_player(child)
		if found != null:
			return found
	return null

func _find_blend_shape_track(animation: Animation, path: String) -> int:
	for track_index in animation.get_track_count():
		if animation.track_get_type(track_index) == Animation.TYPE_BLEND_SHAPE and str(animation.track_get_path(track_index)) == path:
			return track_index
	return -1

func _require(condition: bool, message: String, old_value: Variant) -> bool:
	if condition:
		return true
	_fail(message, old_value)
	return false

func _fail(message: String, old_value: Variant) -> void:
	push_error(message)
	ProjectSettings.set_setting(SETTING, old_value)
	quit(1)
