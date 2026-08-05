extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var old_value = ProjectSettings.get_setting(SETTING, 1)
	ProjectSettings.set_setting(SETTING, 0)

	var source_path := "res://samples/blend_shape_basic.usda"
	var save_path := "user://source_aware_inbetween_blend_shape_saved.usda"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(save_path))

	var loaded := ResourceLoader.load(source_path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	if loaded == null or not (loaded is PackedScene):
		_fail("Failed to load blend_shape_basic.usda", old_value)
		return

	var root := (loaded as PackedScene).instantiate()
	get_root().add_child(root)

	var player := _find_animation_player(root)
	if player == null or not player.has_animation(&"Anim"):
		_fail("Missing Anim clip", old_value)
		return

	var animation := player.get_animation(&"Anim")
	var inbetween_track := _find_blend_shape_track(animation, "root/Plane/Plane:Key_1__inbetween__HalfKey")
	if inbetween_track < 0:
		_fail("Missing inbetween blend-shape track", old_value)
		return
	animation.track_set_key_value(inbetween_track, 2, 0.25)

	var saved_scene := PackedScene.new()
	if saved_scene.pack(root) != OK:
		_fail("Failed to pack edited static USD scene", old_value)
		return
	if ResourceSaver.save(saved_scene, save_path) != OK:
		_fail("Failed to save unsupported inbetween blend-shape edit scene", old_value)
		return
	root.free()

	var saved_text := FileAccess.get_file_as_string(save_path)
	if not _require(saved_text.contains("def BlendShape \"Key_1\""), "Saved layer lost original blend shape", old_value):
		return
	if not _require(saved_text.contains("inbetweens:HalfKey"), "Saved layer lost original inbetween", old_value):
		return
	if not _require(saved_text.contains("10: [0.75]"), "Original primary blend-shape weight should be preserved", old_value):
		return

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
