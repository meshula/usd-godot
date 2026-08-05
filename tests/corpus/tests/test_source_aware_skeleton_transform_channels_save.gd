extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/skeleton_transform_channels.usda"
	var save_path := "user://source_aware_skeleton_transform_channels_saved.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))

	var loaded := ResourceLoader.load(source_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if loaded == null or not (loaded is PackedScene):
		_fail("Failed to load skeleton_transform_channels.usda", old_value)
		return

	var root := (loaded as PackedScene).instantiate()
	get_root().add_child(root)

	var player := _find_animation_player(root)
	if player == null or not player.has_animation(&"MoveScale"):
		_fail("Missing MoveScale animation", old_value)
		return

	var animation := player.get_animation(&"MoveScale")
	var position_track := _find_track(animation, Animation.TYPE_POSITION_3D)
	var scale_track := _find_track(animation, Animation.TYPE_SCALE_3D)
	if not _require(position_track >= 0, "Missing position track", old_value):
		return
	if not _require(scale_track >= 0, "Missing scale track", old_value):
		return
	if not _require(animation.track_get_key_count(position_track) == 2, "Unexpected position key count", old_value):
		return
	if not _require(animation.track_get_key_count(scale_track) == 2, "Unexpected scale key count", old_value):
		return

	animation.track_set_key_value(position_track, 1, Vector3(0, 0, 4))
	animation.track_set_key_value(scale_track, 1, Vector3(3, 2, 1))

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save source-aware transform-channel USD scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("def SkelAnimation \"MoveScale\""), "Saved layer lost animation prim", old_value):
		return
	if not _require(saved_text.contains("10: [(0, 0, 4)]"), "Saved layer did not merge edited translation key", old_value):
		return
	if not _require(saved_text.contains("10: [(3, 2, 1)]"), "Saved layer did not merge edited scale key", old_value):
		return
	if not _require(saved_text.count("def SkelAnimation \"MoveScale\"") == 1, "Saved layer duplicated animation prim", old_value):
		return

	var reloaded := ResourceLoader.load(save_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if reloaded == null or not (reloaded is PackedScene):
		_fail("Failed to reload source-aware transform-channel USD scene", old_value)
		return

	root = (reloaded as PackedScene).instantiate()
	get_root().add_child(root)
	player = _find_animation_player(root)
	if not _require(player != null and player.has_animation(&"MoveScale"), "Reloaded scene missing MoveScale", old_value):
		return
	animation = player.get_animation(&"MoveScale")
	position_track = _find_track(animation, Animation.TYPE_POSITION_3D)
	scale_track = _find_track(animation, Animation.TYPE_SCALE_3D)
	if not _require(position_track >= 0 and animation.track_get_key_value(position_track, 1).is_equal_approx(Vector3(0, 0, 4)), "Reloaded translation edit mismatch", old_value):
		return
	if not _require(scale_track >= 0 and animation.track_get_key_value(scale_track, 1).is_equal_approx(Vector3(3, 2, 1)), "Reloaded scale edit mismatch", old_value):
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

func _find_track(animation: Animation, type: int) -> int:
	for track_index in animation.get_track_count():
		if animation.track_get_type(track_index) == type:
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
