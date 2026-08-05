extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var source_path := "res://samples/skeleton_basic.usda"
    var save_path := "user://usd_skeleton_roundtrip_saved.usda"
    var loaded := load(source_path)
    if loaded == null or not (loaded is PackedScene):
        _fail("Failed to load source skeleton_basic.usda", old_value)
        return

    if ResourceSaver.save(loaded, save_path) != OK:
        _fail("Failed to save round-trip USDA", old_value)
        return

    var reloaded := load(save_path)
    if reloaded == null or not (reloaded is PackedScene):
        _fail("Failed to reload saved USDA", old_value)
        return

    var root := (reloaded as PackedScene).instantiate()
    get_root().add_child(root)

    var skeleton := _find_prim_node(root, "/Model/Skel") as Skeleton3D
    var player := _find_animation_player(root)
    if skeleton == null or player == null:
        _fail("Missing skeleton or AnimationPlayer after round-trip save", old_value)
        return

    if not _require(skeleton.get_bone_count() == 3, "Round-trip bone count mismatch", old_value):
        return
    if not _require(skeleton.get_bone_name(1) == "Elbow", "Round-trip elbow bone missing", old_value):
        return
    if not _require(player.has_animation(&"Anim1"), "Round-trip Anim1 clip missing", old_value):
        return

    var animation := player.get_animation(&"Anim1")
    if not _require(animation.get_track_count() == 1, "Round-trip animation track count mismatch", old_value):
        return
    if not _require(animation.track_get_type(0) == Animation.TYPE_ROTATION_3D, "Round-trip animation track type mismatch", old_value):
        return
    if not _require(str(animation.track_get_path(0)) == "Model/Skel:Elbow", "Round-trip animation track path mismatch", old_value):
        return
    if not _require(animation.track_get_key_count(0) == 2, "Round-trip animation key count mismatch", old_value):
        return

    print("Skeleton USDA round-trip save verified.")

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
