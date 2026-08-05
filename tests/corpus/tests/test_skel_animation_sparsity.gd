extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/skel_animation_sparsity.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load skel_animation_sparsity.usda", old_value)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    var player := _find_animation_player(root)
    if player == null:
        _fail("Missing AnimationPlayer for skel_animation_sparsity", old_value)
        return

    if not _require(player.has_animation(&"Anim"), "Anim clip missing", old_value):
        return
    var animation := player.get_animation(&"Anim")
    if not _require(animation.get_track_count() == 1, "Sparse clip track count mismatch", old_value):
        return
    if not _require(animation.track_get_type(0) == Animation.TYPE_ROTATION_3D, "Sparse clip visible track type mismatch", old_value):
        return
    if not _require(str(animation.track_get_path(0)) == "root/Model/Skel:joint1", "Sparse clip visible track path mismatch", old_value):
        return
    if not _require(animation.track_get_key_count(0) >= 2, "Sparse clip visible track key count too small", old_value):
        return

    var meta := animation.get_meta("usd", {}) as Dictionary
    if not _require(bool(meta.get("usd:has_authored_translations", false)), "Missing authored translations metadata", old_value):
        return
    if not _require(bool(meta.get("usd:has_authored_rotations", false)), "Missing authored rotations metadata", old_value):
        return
    if not _require(bool(meta.get("usd:has_authored_scales", false)), "Missing authored scales metadata", old_value):
        return
    if not _require(bool(meta.get("usd:has_authored_blend_shape_weights", false)), "Missing authored blend shape weight metadata", old_value):
        return
    if not _require(bool(meta.get("usd:translations_constant", true)) == false, "translations_constant metadata mismatch", old_value):
        return
    if not _require(bool(meta.get("usd:scales_constant", true)) == false, "scales_constant metadata mismatch", old_value):
        return
    if not _require(bool(meta.get("usd:blend_shape_weights_constant", true)) == false, "blend_shape_weights_constant metadata mismatch", old_value):
        return

    var translation_time_codes := meta.get("usd:translation_time_codes", []) as Array
    var rotation_time_codes := meta.get("usd:rotation_time_codes", []) as Array
    var scale_time_codes := meta.get("usd:scale_time_codes", []) as Array
    var blend_shape_weight_time_codes := meta.get("usd:blend_shape_weight_time_codes", []) as Array
    if not _require(translation_time_codes.size() == 2, "Translation time-code count mismatch", old_value):
        return
    if not _require(rotation_time_codes.size() == 2, "Rotation time-code count mismatch", old_value):
        return
    if not _require(scale_time_codes.size() == 2, "Scale time-code count mismatch", old_value):
        return
    if not _require(blend_shape_weight_time_codes.size() == 2, "Blend-shape time-code count mismatch", old_value):
        return
    if not _require(is_equal_approx(float(translation_time_codes[0]), 1.0) and is_equal_approx(float(translation_time_codes[1]), 9.0), "Translation time-code values mismatch", old_value):
        return
    if not _require(is_equal_approx(float(rotation_time_codes[0]), 5.0) and is_equal_approx(float(rotation_time_codes[1]), 10.0), "Rotation time-code values mismatch", old_value):
        return
    if not _require(is_equal_approx(float(scale_time_codes[0]), 3.0) and is_equal_approx(float(scale_time_codes[1]), 7.0), "Scale time-code values mismatch", old_value):
        return
    if not _require(is_equal_approx(float(blend_shape_weight_time_codes[0]), 2.0) and is_equal_approx(float(blend_shape_weight_time_codes[1]), 8.0), "Blend-shape time-code values mismatch", old_value):
        return

    print("Sparse SkelAnimation metadata preservation verified.")

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

func _require(condition: bool, message: String, old_value: Variant) -> bool:
    if condition:
        return true
    _fail(message, old_value)
    return false

func _fail(message: String, old_value: Variant) -> void:
    push_error(message)
    ProjectSettings.set_setting(SETTING, old_value)
    quit(1)
