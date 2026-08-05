extends SceneTree

const SETTING := "filesystem/import/usd/preview_lighting_mode"

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var old_value = ProjectSettings.get_setting(SETTING, 1)
    ProjectSettings.set_setting(SETTING, 0)

    var packed := load("res://samples/skeleton_multi_anim_blendshape.usda")
    if packed == null or not (packed is PackedScene):
        _fail("Failed to load skeleton_multi_anim_blendshape.usda", old_value)
        return

    var root := (packed as PackedScene).instantiate()
    get_root().add_child(root)

    var skeleton := _find_prim_node(root, "/root/Actor/Skel") as Skeleton3D
    var mesh_instance := _find_prim_node(root, "/root/Actor/Body") as MeshInstance3D
    var player := _find_animation_player(root)
    if skeleton == null or mesh_instance == null or player == null:
        _fail("Missing skeleton, mesh, or AnimationPlayer for skeleton_multi_anim_blendshape", old_value)
        return

    if not _require(mesh_instance.get_mesh() != null, "Body mesh is missing", old_value):
        return
    if not _require(player.has_animation(&"Rotate"), "Rotate clip missing", old_value):
        return
    if not _require(player.has_animation(&"Combo"), "Combo clip missing", old_value):
        return

    var combo := player.get_animation(&"Combo")
    var found_combo_rotation_track := false
    var found_combo_blend_shape_track := false
    for track_index in combo.get_track_count():
        var track_path := str(combo.track_get_path(track_index))
        if combo.track_get_type(track_index) == Animation.TYPE_ROTATION_3D and track_path == "root/Actor/Skel:joint1":
            found_combo_rotation_track = true
        elif combo.track_get_type(track_index) == Animation.TYPE_BLEND_SHAPE and track_path == "root/Actor/Body:Smile":
            found_combo_blend_shape_track = true

    if not _require(found_combo_rotation_track, "Combo rotation track missing", old_value):
        return
    if not _require(found_combo_blend_shape_track, "Combo blend shape track missing", old_value):
        return

    print("Mixed SkelAnimation clip import verified.")

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
